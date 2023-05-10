#include "csp_suo_adapter.hpp"
#include "csp_modem.hpp"

#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <csp/crypto/csp_hmac.h>
#include <csp/crypto/csp_xtea.h>
#include <csp/csp_crc32.h>
#include <csp/arch/csp_system.h>
#include <csp/csp_interface.h>

#ifdef LIBFEC
#include "libfec/fec.h"
#endif

#define CSP_RS_MSGLEN   223
#define CSP_RS_PARITYS  32

using namespace std;
using namespace suo;


CSPSuoAdapter::Config::Config() {
	use_libfec = false;

	rx_use_hmac = false;
	rx_use_rs = false;
	rx_use_crc = false;
	rx_use_rand = false;
	rx_legacy_hmac = false;
	rx_hmac_key[16] = {0};
	rx_use_xtea = false;
	rx_xtea_key[20] = {0};
	rx_filter_ground_addresses = true;

	tx_use_hmac = false;
	tx_use_rs = false;
	tx_use_crc = false;
	tx_use_rand = false;
	tx_legacy_hmac = false;
	tx_hmac_key[16] = {0};
	tx_use_xtea = false;
	tx_xtea_key[20] = {0};
}


CSPSuoAdapter::CSPSuoAdapter(const Config& _conf) :
	conf(_conf),
	tx_packet(nullptr),
	tx_acked(true)
{
	memset(&csp_iface, 0, sizeof(csp_iface));
	memset(&stats, 0, sizeof(stats));

	/* Initialize CSP interface struct */
	csp_iface.name = "SUO";
	csp_iface.interface_data = this;
	csp_iface.mtu = csp_buffer_data_size();

	if (conf.tx_use_rs) {
		// Limit frame data size to Reed-Solomon message length
		csp_iface.mtu = CSP_RS_MSGLEN - sizeof(csp_id_t);
		// Make also sure the buffer allocation has room for complete Reed-Solomon codeword.
		assert(csp_buffer_data_size() >= (CSP_RS_MSGLEN + CSP_RS_PARITYS) - sizeof(csp_id_t));
	}

	if (conf.tx_use_crc)
		csp_iface.mtu -= sizeof(uint32_t);  // Reserve space for CRC32
	if (conf.tx_use_hmac)
		csp_iface.mtu -= CSP_HMAC_LENGTH;  // Reserve space for hash
	if (conf.tx_use_xtea)
		csp_iface.mtu -= sizeof(uint32_t);  // Reserve space for nonce

	csp_iface.nexthop = [](const csp_route_t *route, csp_packet_t *packet) -> int {
		return static_cast<CSPSuoAdapter *>(route->iface->interface_data)->csp_transmit(packet);
	};

	csp_bin_sem_create(&tx_wait);

	/* Register interface */
	csp_iflist_add(&csp_iface);
}


CSPSuoAdapter::~CSPSuoAdapter() {
	//cerr << "WARNING! CSPSuoAdapter destructor called!" << endl;
	csp_bin_sem_remove(&tx_wait);
}


int CSPSuoAdapter::csp_transmit(csp_packet_t *packet) {

	if (tx_packet != nullptr) {
		csp_log_warn("csp_transmit is busy!\n");
		return CSP_ERR_BUSY; // CSP_ERR_AGAIN
	}

	csp_log_packet("\033[0;35m"
	               "TX: Src %u, Dst %u, Dport %u, Sport %u, Pri %u, Flags 0x%02X, Size %" PRIu16,
	               packet->id.src, packet->id.dst, packet->id.dport,
	               packet->id.sport, packet->id.pri, packet->id.flags, packet->length);


	// Make sure the waiting semaphore is not locked.
	int ret = csp_bin_sem_wait(&tx_wait, 100);
	if (ret != CSP_SEMAPHORE_OK) {
		cerr << "Error: TX complete semaphora locked! " << ret << endl;
		return CSP_ERR_BUSY;
	}

	// Signal the frame from CSP router thread to suo thread
	tx_packet = packet;

	// Wait until its released by the other task
	ret = csp_bin_sem_wait(&tx_wait, 1000); 
	if (ret != CSP_SEMAPHORE_OK) {
		cerr << "Error: TX completed semaphora timed out! " << ret << endl;
		return CSP_ERR_BUSY;
	}
	csp_bin_sem_wait(&tx_wait, 5); // Add time delay between frames.
	csp_bin_sem_post(&tx_wait);

	return 0;
}


void CSPSuoAdapter::sourceFrame(Frame &frame, Timestamp now)
{
	(void)now;

	// Copy CSP packet from TX queue to Frame
	if (tx_packet == nullptr) {
		if (tx_acked == false) {
			tx_acked = true;
			//cout << "TX done" << endl;
			csp_bin_sem_post(&tx_wait);
		}
		return;
	}

	tx_acked = false;

	stats.tx_count++;
	stats.tx_bytes += tx_packet->length;

	/* Save the outgoing id in the buffer */
	tx_packet->id.ext = csp_hton32(tx_packet->id.ext);

	/* Calculate HMAC if selected */
	if (conf.tx_use_hmac)
	{
		csp_hmac_set_key(conf.tx_hmac_key, conf.tx_legacy_hmac ? 4 : 16);
		int ret = csp_hmac_append(tx_packet, true);
		if (ret != CSP_ERR_NONE)
		{
			csp_log_warn("HMAC append failed %d\n", ret);
			return;
		}
	}

	/* Calculate CRC32 if selected */
	if (conf.tx_use_crc)
	{
		int ret = csp_crc32_append(tx_packet, true);
		if (ret != CSP_ERR_NONE)
		{
			csp_log_warn("CRC32 append failed! %d\n", ret);
			return;
		}
	}

	/* Calculate XTEA encryption if selected */
	if (conf.tx_use_xtea) {
#if (CSP_USE_XTEA)
		csp_xtea_set_key(conf.tx_xtea_key, 20);
		int ret = csp_xtea_encrypt_packet(tx_packet);
		if(ret != CSP_ERR_NONE) {
			csp_log_warn("XTEA Encryption failed! %d\n", ret);
			return;
		}
#else
		csp_log_warn("Attempt to send XTEA encrypted packet, but CSP was compiled without XTEA support. Discarding packet\n");
		return;
#endif
	}

	if (conf.use_libfec && conf.tx_use_rs) {
#ifdef LIBFEC
		encode_rs_8((uint8_t *)&tx_packet->id, &tx_packet->data[tx_packet->length], CSP_RS_MSGLEN - tx_packet->length);
		tx_packet->length += CSP_RS_PARITYS;
#else
		csp_log_error("libfec not supported\n");
		csp_buffer_free(tx_packet);
		return;
#endif
	}

	tx_packet->length += sizeof(tx_packet->id.ext);

	/* Copy data to Suo frame */
	frame.data.resize(tx_packet->length);
	memcpy(&frame.data[0], &tx_packet->id, tx_packet->length);
	cout << frame.data;

	csp_buffer_free(tx_packet);
	tx_packet = nullptr;

	cout << frame;
}


void CSPSuoAdapter::sinkFrame(const Frame &frame, Timestamp now)
{
	(void)now;

	cout << frame;
	
	// Enough bit to 
	if (frame.size() < sizeof(csp_id_t)) {
		csp_log_warn("Too short frame! len: %lu\n", frame.size());
		return;
	}

	// Allocate a new CSP frame
	csp_packet_t *packet = static_cast<csp_packet_t*>(csp_buffer_get(frame.size() - sizeof(csp_id_t)));
	if (packet == NULL)
		throw SuoError("csp_buffer_get failed!");

	memcpy(&packet->id, frame.data.data(), frame.size());
	packet->length = frame.size();
	
	stats.rx_count++;

	/* Unrandomize data if necessary */
	if (conf.rx_use_rand)
		csp_apply_rand(packet);

	/* Decode Reed-Solomon if selected */
	if (conf.rx_use_rs) {
		if (conf.use_libfec) {
			// Enough bytes for Reed-Solomon decoder?
			if (packet->length < CSP_RS_PARITYS || packet->length > (CSP_RS_MSGLEN + CSP_RS_PARITYS)) {
				csp_log_warn("Invalid frame length for Reed-Solomon decoder. len: %d\n", packet->length);
				return;
			}

#ifdef LIBFEC
			int ret = decode_rs_8((uint8_t *)&packet->id, NULL, 0, CSP_RS_MSGLEN + CSP_RS_PARITYS - packet->length);
			if (ret < 0) {
				csp_log_error("Failed to decode RS");
				csp_buffer_free(packet);
				stats.rx_failed++;
				return;
			}

			csp_log_info("RS corrected %d errors", ret);
			stats.rx_corrected_bytes += ret;

#if 0
			/* Count bit errors */
			unsigned int corrected_bits = 0;
			const uint8_t* original = static_cast<uint8_t*>(&packet->id); // TODO
			const uint8_t* corrected = static_cast<uint8_t *>(&packet->id);
			for (unsigned int i = 0; i < packet->length; i++)
				corrected_bits += popcount(original[i] ^ corrected[i]);
			stats.rx_bits_corrected += corrected_bits;
#endif
#else
			csp_log_error("libfec not supported\n");
			csp_buffer_free(packet);
			return;
#endif
		}
		else {
			// RS is used but decoding is implemented by suo.
			// Increment statistics based on metadata inside the suo frame
			try {
				stats.rx_corrected_bytes += get<unsigned int>(frame.metadata.at("rs_bytes_corrected"));
				stats.rx_bits_corrected += get<unsigned int>(frame.metadata.at("rs_bits_corrected"));
			}
			catch (std::out_of_range& e) {
				cerr << "Frame missing field: " << e.what() << endl;
			}
		}
	}

	// Make sure there are enough bytes after RS decoder.
	if (packet->length >= sizeof(csp_id_t)) {
		csp_log_warn("Too short frame after decoding! len: %d\n", packet->length);
		return;
	}

	/* The CSP packet length is without the header */
	packet->length = frame.size() - sizeof(csp_id_t);

	/* XTEA encrypted packet */
	if (conf.rx_use_xtea) {
		csp_xtea_set_key(conf.rx_xtea_key, 20);
		if (csp_xtea_decrypt_packet(packet) != CSP_ERR_NONE)
		{
			csp_log_error("Decryption failed! Discarding packet");
			csp_buffer_free(packet);
			stats.rx_failed++;
			return;
		}
	}

	/* Validate CRC32 */
	if (conf.rx_use_crc) {
		int ret = csp_crc32_verify(packet, true);
		if (ret != CSP_ERR_NONE)
		{
			csp_log_warn("CRC failed %d", ret);
			csp_buffer_free(packet);
			stats.rx_failed++;
			return;
		}
	}

	/* Verify HMAC if selected */
	if (conf.rx_use_hmac) {
		csp_hmac_set_key(conf.rx_hmac_key, conf.rx_legacy_hmac ? 4 : 16);
		int ret = csp_hmac_verify(packet, true);
		if (ret != CSP_ERR_NONE) {
			csp_log_error("HMAC error %d", ret);
			csp_buffer_free(packet);
			stats.rx_failed++;
			return;
		}
	}

	/* Convert the packet from network to host order */
	packet->id.ext = csp_ntoh32(packet->id.ext);

	/* Ignore frame if source port indicates that is coming from ground segment. */
	if (conf.rx_filter_ground_addresses && packet->id.src > 8) {
		csp_log_info("Frame filtered");
		csp_buffer_free(packet);
		return;
	}

	stats.rx_bytes += packet->length;

	csp_log_packet("\033[0;36m"
				   "RX: Src %u, Dst %u, Dport %u, Sport %u, Pri %u, Flags 0x%02X, Size %" PRIu16,
				   packet->id.src, packet->id.dst, packet->id.dport,
				   packet->id.sport, packet->id.pri, packet->id.flags, packet->length);

	csp_qfifo_write(packet, &csp_iface, NULL);
}
