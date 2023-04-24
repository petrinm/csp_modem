
#include "csp_gnuradio_adapter.hpp"

#include <zmq.hpp>

#include <pmt/pmt.h>

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

#define CSP_RS_LEN      32
#define CSP_RS_MSGLEN   256
#define CSP_SUO_MTU     256


using namespace std;

zmq::context_t zmq_ctx;


CSPGNURadioCAdapter::Config::Config()
{
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



CSPGNURadioCAdapter::CSPGNURadioCAdapter(const Config& _conf) :
	conf(_conf)
{
	memset(&csp_iface, 0, sizeof(csp_iface));
	memset(&stats, 0, sizeof(stats));

	if (conf.use_xtea)
		csp_xtea_set_key(conf.xtea_key, 20);
	if (conf.use_hmac)
		csp_hmac_set_key(conf.hmac_key, conf.legacy_hmac ? 4 : 16);


	/* Setup ZMQ sockets for GNUradio connection */
	sock_pub = zmq::socket_t(zmq_ctx, zmq::socket_type::pub);
	sock_pub.connect("tcp://127.0.0.1:6100");

	sock_sub = zmq::socket_t(zmq_ctx, zmq::socket_type::sub);
	sock_sub.connect("tcp://127.0.0.1:6200");
	//sock_sub.set(zmq::sockopt::subscribe, "");
	

	/* Initialize CSP interface struct */
	csp_iface.name = "GNURadio",
	csp_iface.interface_data = this;
	csp_iface.mtu = csp_buffer_data_size(); 
	if (conf.use_crc)
		csp_iface.mtu -= sizeof(uint32_t); // Reserve space for CRC32
	if (conf.use_hmac)
		csp_iface.mtu -= CSP_HMAC_LENGTH; // Reserve space for hash
	if (conf.use_hmac)
		csp_iface.mtu -= sizeof(uint32_t); // Reserve space for nonce

	csp_iface.nexthop = [](const csp_route_t *route, csp_packet_t *packet) -> int {
		return static_cast<CSPGNURadioCAdapter*>(route->iface->interface_data)->transmit(packet);
	};

	csp_bin_sem_create(&tx_wait);

	/* Register interface */
	csp_iflist_add(&csp_iface);
}

CSPGNURadioCAdapter::~CSPGNURadioCAdapter()
{
	csp_bin_sem_remove(&tx_wait);
}

int CSPGNURadioCAdapter::transmit(csp_packet_t *packet) {

	csp_log_packet("\033[0;35m"
	               "TX: Src %u, Dst %u, Dport %u, Sport %u, Pri %u, Flags 0x%02X, Size %" PRIu16,
	               packet->id.src, packet->id.dst, packet->id.dport,
	               packet->id.sport, packet->id.pri, packet->id.flags, packet->length);



	stats.tx_count++;
	stats.tx_bytes += tx_packet->length;

	/* Save the outgoing id in the buffer */
	tx_packet->id.ext = csp_hton32(tx_packet->id.ext);

	/* Calculate HMAC if selected */
	if (conf.use_hmac)
	{
		int ret = csp_hmac_append(tx_packet, true);
		if (ret != CSP_ERR_NONE) {
			csp_log_error("HMAC append failed %d\n", ret);
			return ret;
		}
	}

	/* Calculate CRC32 if selected */
	if (conf.use_crc)
	{
		int ret = csp_crc32_append(tx_packet, true);
		if (ret != CSP_ERR_NONE) {
			csp_log_error("CRC32 append failed! %d\n", ret);
			return ret;
		}
	}

	/* Calculate XTEA encryption if selected */
	if (conf.use_xtea) {
#if (CSP_USE_XTEA)
		int ret = csp_xtea_encrypt_packet(tx_packet);
		if(ret != CSP_ERR_NONE) {
			csp_log_error("XTEA Encryption failed! %d\n", ret);
			return ret;
		}
#else
		csp_log_error("Attempt to send XTEA encrypted packet, but CSP was compiled without XTEA support. Discarding packet\n");
		return CSP_ERR_NOTSUP;
#endif
	}

	packet->length += sizeof(packet->id.ext);

	/* Append Reed-Solomon error correction code */
	if (conf.use_rs) {
#ifdef LIBFEC
		encode_rs_8((uint8_t*)&packet->id, &packet->data[packet->length], CSP_RS_MSGLEN - packet->length);
		packet->length += CSP_RS_LEN;
#else
		csp_log_error("libfec not supported\n");
		return CSP_ERR_NOTSUP;
#endif
	}

	/* Randomize data if necessary */
	if (conf.use_rand)
		csp_apply_rand(packet);


	/* Create a PMT */
	pmt::pmt_t data_vector = pmt::init_u8vector(packet->length, reinterpret_cast<const uint8_t*>(&packet->id));
	pmt::pmt_t root = pmt::cons(pmt::get_PMT_NIL(), data_vector);

	stringbuf sb;;
	if (pmt::serialize(root, sb) == false)
		throw runtime_error("todo");

	string msg(sb.str());
	sock_pub.send(zmq::const_buffer(msg.data(), msg.size()));
	return CSP_ERR_NONE;
}


void CSPGNURadioCAdapter::receive()
{
	/* Receive raw frame from ZMQ socket */
	zmq::message_t zmq_msg;
	try {
		auto res = sock_sub.recv(zmq_msg); // zmq::recv_flags::dontwait
		if (res.value() == 0)
			return;
	}
	catch (const zmq::error_t &e)
	{
		throw runtime_error("Failed to read message header. " + string(e.what()));
	}

	stats.rx_count++;

	/* Decode PMT message to CSP packet */
	std::string buf(static_cast<char *>(zmq_msg.data()), zmq_msg.size());
	std::stringbuf sb(buf);
	pmt::pmt_t root = pmt::deserialize(sb);
	if (pmt::is_pair(root) == false)
		throw runtime_error("todo");
	pmt::pmt_t data_vector = pmt::cdr(root);
	if (pmt::is_u8vector(data_vector) == false)
		throw runtime_error("PMT data vector is not u8 vector!");

	size_t raw_data_len;
	const uint8_t* raw_data = pmt::u8vector_elements(data_vector, raw_data_len);

	if (raw_data_len < sizeof(csp_id_t)) {
		csp_log_warn("Too short frame: %lu\n", raw_data_len);
		return;
	}

	if (raw_data_len > CSP_SUO_MTU)  {
		csp_log_warn("Too long frame: %lu\n", raw_data_len);
		return;
	}

	/* Allocate CSP packet and store the bytes inside it. */
	csp_packet_t *packet = static_cast<csp_packet_t *>(csp_buffer_get(raw_data_len - sizeof(csp_id_t)));
	if (packet == NULL)
		throw runtime_error("csp_buffer_get failed!");
	memcpy(&packet->id, raw_data, raw_data_len);
	packet->length = raw_data_len;

	/* Unrandomize data if necessary */
	if (conf.use_rand)
		csp_apply_rand(packet);

	/* Append Reed-Solomon error correction code */
	if (conf.use_rs)
	{
#ifdef LIBFEC
		int ret = decode_rs_8((uint8_t *)&packet->id, NULL, 0, CSP_RS_MSGLEN + CSP_RS_LEN - packet->length);
		if (ret < 0)
		{
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

	/* The CSP packet length is without the header */
	packet->length = raw_data_len - sizeof(csp_id_t);

	/* Convert the packet from network to host order */
	packet->id.ext = csp_ntoh32(packet->id.ext);

	/* Ignore frame if source port indicates that is coming from ground segment. */
	if (conf.filter_ground_addresses && packet->id.sport > 8) {
		csp_log_info("Frame filtered");
		return;
	}

	/* Verify HMAC if selected */
	if (conf.use_hmac)
	{
		int ret = csp_hmac_verify(packet, true);
		if (ret != CSP_ERR_NONE)
		{
			csp_log_error("HMAC error %d", ret);
			csp_buffer_free(packet);
			stats.rx_failed++;
			return;
		}
	}

	/* Validate CRC32 */
	if (conf.use_crc) {
		int ret = csp_crc32_verify(packet, true);
		if (ret != CSP_ERR_NONE)
		{
			csp_log_warn("Invalid CRC32 %d", ret);
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

