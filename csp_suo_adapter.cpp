#include "csp_suo_adapter.hpp"
#include "csp_modem.hpp"

#include <csp/csp.h>
#include <csp/csp_endian.h>
#ifdef OLD_CSP
extern "C"
{
	int csp_hmac_append(csp_packet_t *packet);
	int csp_hmac_verify(csp_packet_t *packet);
	int csp_xtea_encrypt(uint8_t *plain, const uint32_t len, uint32_t iv[2]);
	int csp_xtea_decrypt(uint8_t *cipher, const uint32_t len, uint32_t iv[2]);
}
#else
#include <csp/crypto/csp_hmac.h>
#include <csp/crypto/csp_xtea.h>
#endif
#include <csp/csp_crc32.h>
#include <csp/arch/csp_system.h>
#include <csp/csp_interface.h>

#ifdef LIBFEC
#include "libfec/fec.h"
#endif

#define CSP_RS_LEN      32
#define CSP_RS_MSGLEN   256
#define CSP_SUO_MTU     256

using namespace std;
using namespace suo;


CSPSuoAdapter::Config::Config() {
	use_hmac = false;
	use_rs = true;
	use_crc = false;
	use_rand = false;
	legacy_hmac = false;
	hmac_key[16] = {0};
	use_xtea = false;
	xtea_key[20] = {0};
	filter_ground_addresses = true;
}


CSPSuoAdapter::CSPSuoAdapter(const Config& _conf) :
	conf(_conf),
	tx_packet(nullptr),
	tx_acked(true)
{
	memset(&csp_iface, 0, sizeof(csp_iface));
	memset(&stats, 0, sizeof(stats));

#if OLD_CSP
	if (conf.use_xtea)
		csp_xtea_set_key(static_cast<char*>(conf.xtea_key), 20);
	if (conf.use_hmac)
		csp_hmac_set_key(static_cast<char*>(conf.hmac_key), conf.legacy_hmac ? 4 : 16);
#else
	if (conf.use_xtea)
		csp_xtea_set_key(conf.xtea_key, 20);
	if (conf.use_hmac)
		csp_hmac_set_key(conf.hmac_key, conf.legacy_hmac ? 4 : 16);
#endif

	/* Initialize CSP interface struct */
	csp_iface.name = "SUO",
	csp_iface.interface_data = this;
	csp_iface.mtu = csp_buffer_data_size(); 
	if (conf.use_crc)
		csp_iface.mtu -= sizeof(uint32_t); // Reserve space for CRC32
	if (conf.use_hmac)
		csp_iface.mtu -= CSP_HMAC_LENGTH; // Reserve space for hash
	if (conf.use_hmac)
		csp_iface.mtu -= sizeof(uint32_t); // Reserve space for nonce

#ifdef OLD_CSP
	csp_iface.nexthop = [](csp_iface_t *interface, csp_packet_t *packet, uint32_t timeout) -> int {
		(void)timeout;
		return static_cast<CSPSuoAdapter *>(interface->interface_data)->csp_transmit(packet);
	};
#else
	csp_iface.nexthop = [](const csp_route_t *route, csp_packet_t *packet) -> int {
		return static_cast<CSPSuoAdapter *>(route->iface->interface_data)->csp_transmit(packet);
	};
#endif

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
	if (conf.use_hmac)
	{
#ifdef OLD_CSP
		/* Old CSP interface doesn't allow including the header in the HMAC, so we need to done this manually. */
		uint8_t hmac[CSP_SHA1_DIGESTSIZE];
		csp_sha1_memory(conf.hmac_key, (conf.legacy_hmac ? 4 : 16), hmac);
		int ret = csp_hmac_memory(hmac, 16, &packet->id, packet->length + sizeof(packet->id), hmac);
		memcpy(&packet->data[packet->length], hmac, CSP_HMAC_LENGTH);
		packet->length += CSP_HMAC_LENGTH;
#elif defined(HMAC_HACK)
		/* HMAC calculation without key hashing. */
		uint8_t hmac[CSP_SHA1_DIGESTSIZE];
		int ret = csp_hmac_memory(conf.hmac_key, sizeof(conf.hmac_key), &tx_packet->id, tx_packet->length + sizeof(tx_packet->id), hmac);
		memcpy(&tx_packet->data[tx_packet->length], hmac, CSP_HMAC_LENGTH);
		tx_packet->length += CSP_HMAC_LENGTH;
#else
		int ret = csp_hmac_append(tx_packet, true);
#endif
		if (ret != CSP_ERR_NONE)
		{
			csp_log_warn("HMAC append failed %d\n", ret);
			return;
		}
	}

	/* Calculate CRC32 if selected */
	if (conf.use_crc)
	{
#ifdef OLD_CSP
		int ret = csp_crc32_append(tx_packet);
#else
		int ret = csp_crc32_append(tx_packet, true);
#endif
		if (ret != CSP_ERR_NONE)
		{
			csp_log_warn("CRC32 append failed! %d\n", ret);
			return;
		}
	}

	/* Calculate XTEA encryption if selected */
	if (conf.use_xtea) {
#if (CSP_USE_XTEA)
#ifdef OLD_CSP
		int ret = 0; //csp_xtea_encrypt(& tx_packet->id);
#else
		int ret = csp_xtea_encrypt_packet(tx_packet);
#endif
		if(ret != CSP_ERR_NONE) {
			csp_log_warn("XTEA Encryption failed! %d\n", ret);
			return;
		}
#else
		csp_log_warn("Attempt to send XTEA encrypted packet, but CSP was compiled without XTEA support. Discarding packet\n");
		return;
#endif
	}

	if (conf.use_rs) {
#ifdef LIBFEC
		encode_rs_8((uint8_t *)&tx_packet->id, &tx_packet->data[tx_packet->length], CSP_RS_MSGLEN - tx_packet->length);
		tx_packet->length += CSP_RS_LEN;
#else
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
	
	if (frame.size() < sizeof(csp_id_t)) {
		csp_log_warn("SUO: Too short datalen: %lu\n", frame.size());
		return;
	}

	if (frame.size() > CSP_SUO_MTU)  {
		csp_log_warn("SUO: Too long datalen: %lu\n", frame.size());
		return;
	}

	csp_packet_t *packet = static_cast<csp_packet_t*>(csp_buffer_get(frame.size() - sizeof(csp_id_t)));
	if (packet == NULL)
		throw SuoError("csp_buffer_get failed!");

	memcpy(&packet->id, frame.data.data(), frame.size());
	packet->length = frame.size();
	
	stats.rx_count++;

	/* Unrandomize data if necessary */
	if (conf.use_rand)
		csp_apply_rand(packet);

	/* Decode Reed-Solomon if selected */
	if (conf.use_rs) {
#ifdef LIBFEC
		int ret = decode_rs_8((uint8_t *)&packet->id, NULL, 0, CSP_RS_MSGLEN + CSP_RS_LEN - packet->length);
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
		return;
#endif
	}
	else {
		// Increment statistics based on metadata inside the suo frame
		try {
			stats.rx_corrected_bytes += get<int>(frame.metadata.at("rs_bytes_corrected"));
			stats.rx_bits_corrected += get<int>(frame.metadata.at("rs_bits_corrected"));
		}
		catch (std::out_of_range& e) {
			cerr << "Frame missing field: " << e.what() << endl;
		}
	}

	/* The CSP packet length is without the header */
	packet->length = frame.size() - sizeof(csp_id_t);

	/* Convert the packet from network to host order */
	packet->id.ext = csp_ntoh32(packet->id.ext);


	/* Ignore frame if source port indicates that is coming from ground segment. */
	if (conf.filter_ground_addresses && packet->id.sport > 8) {
		csp_log_info("Frame filtered");
		return;
	}


	/* XTEA encrypted packet */
	if (conf.use_xtea) {
#ifdef OLD_CSP
		if (1)
#else
		if (csp_xtea_decrypt_packet(packet) != CSP_ERR_NONE)
#endif
		{
			csp_log_error("Decryption failed! Discarding packet");
			csp_buffer_free(packet);
			stats.rx_failed++;
			return;
		}
	}


	/* Verify HMAC if selected */
	if (conf.use_hmac) {

#ifdef OLD_CSP
		/* Old CSP interface doesn't allow including the header in the HMAC, so we need to done this manually. */
		uint8_t hmac[CSP_SHA1_DIGESTSIZE];
		csp_sha1_memory(conf.hmac_key, (conf.legacy_hmac ? 4 : 16), hmac);
		int ret = csp_hmac_memory(hmac, 16, &packet->id, packet->length + sizeof(packet->id), hmac);
		if (ret != CSP_ERR_NONE && memcmp(&packet->data[packet->length] - CSP_HMAC_LENGTH, hmac, CSP_HMAC_LENGTH) != 0)
			ret = CSP_ERR_HMAC;
#else
		int ret = csp_hmac_verify(packet, true);
#endif
		if (ret != CSP_ERR_NONE) {
			csp_log_error("HMAC error %d", ret);
			csp_buffer_free(packet);
			stats.rx_failed++;
			return;
		}
	}


	/* Validate CRC32 */
	if (conf.use_crc) {
#ifdef OLD_CSP
		// TODO: Doesn't account the header correctly!
		int ret = csp_crc32_verify(packet);
#else
		int ret = csp_crc32_verify(packet, true);
#endif
		if (ret != CSP_ERR_NONE)
		{
			csp_log_warn("CRC failed %d", ret);
			csp_buffer_free(packet);
			stats.rx_failed++;
			return;
		}
	}

	stats.rx_bytes += packet->length;

	csp_log_packet("\033[0;36m"
				   "RX: Src %u, Dst %u, Dport %u, Sport %u, Pri %u, Flags 0x%02X, Size %" PRIu16,
				   packet->id.src, packet->id.dst, packet->id.dport,
				   packet->id.sport, packet->id.pri, packet->id.flags, packet->length);

	csp_qfifo_write(packet, &csp_iface, NULL);
}
