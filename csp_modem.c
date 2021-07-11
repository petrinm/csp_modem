
#include <unistd.h>
#include <stdlib.h>
#include <zmq.h>
#include <assert.h>
#include <pthread.h>



#include "csp_modem.h"

#include "csp/csp.h"
#include "csp/csp_endian.h"
#include "csp/crypto/csp_hmac.h"
#include "csp/csp_interface.h"
#include "csp/arch/csp_thread.h"
#include "csp/interfaces/csp_if_zmqhub.h"


CSPModemStats_t stats;


int make_frame() {
	//
	return 0;
}

void* zmq_ctx;

void transmit_frame(void* a, int b) { }
void receive_frame() {}


void forwarder_task(void* x)
{
	(void)x;

	csp_log_info("Starting TX forwarder");

	void *subscriber = zmq_socket(zmq_ctx, ZMQ_SUB);
	assert(zmq_connect(subscriber, "tcp://localhost:7000") == 0);
	assert(zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0) == 0);

	csp_packet_t* packet = malloc(1024);

	while (1)
	{
		zmq_msg_t msg;
		zmq_msg_init_size(&msg, 1024);

		/* Receive data */
		if (zmq_msg_recv(&msg, subscriber, 0) < 0) {
			zmq_msg_close(&msg);
			csp_log_error("ZMQ: %s\r\n", zmq_strerror(zmq_errno()));
			continue;
		}

		int datalen = zmq_msg_size(&msg);
		if (datalen < 5) {
			csp_log_warn("ZMQ: Too short datalen: %u\r\n", datalen);
			while(zmq_msg_recv(&msg, subscriber, ZMQ_NOBLOCK) > 0)
				zmq_msg_close(&msg);
			continue;
		}

		zmq_msg_close(&msg);

		csp_log_packet("TX: Src %u, Dst %u, Dport %u, Sport %u, Pri %u, Flags 0x%02X, Size %"PRIu16,
		               packet->id.src, packet->id.dst, packet->id.dport,
		               packet->id.sport, packet->id.pri, packet->id.flags, packet->length);


		/* Save the outgoing id in the buffer */
		packet->id.ext = csp_hton32(packet->id.ext);
		packet->length += sizeof(packet->id.ext);


		/* Calculate HMAC if selected */
		if (cfg.csp_hmac) {
			if (csp_hmac_append(packet, 1) != CSP_ERR_NONE) {
				csp_log_warn("HMAC append failed!");
				goto tx_err;
			}
		}

		/* Calculate CRC32 if selected */
		if (cfg.csp_crc) {
			if (csp_crc32_append(packet, true) != CSP_ERR_NONE) {
				csp_log_warn("CRC32 append failed!");
				goto tx_err;
			}
		}

#if 0
		if (cfg.xtea) {
#if (CSP_USE_XTEA)
			if (csp_xtea_encrypt_packet(packet) != CSP_ERR_NONE) {
				csp_log_warn("XTEA Encryption failed!");
				goto tx_err;
			}
		}
#else
			csp_log_warn("Attempt to send XTEA encrypted packet, but CSP was compiled without XTEA support. Discarding packet");
			goto tx_err;
#endif
#endif

		/* Calculate Reed-Solomon if selected */
		if (cfg.csp_rs)
			csp_fec_append(packet);

		/* Randomize data if necessary */
		if (cfg.csp_rand)
			csp_apply_rand(packet);

		transmit_frame(&packet->id, packet->length);

tx_err:
		x = 0;

	}
}

void rx_fo() {

	csp_packet_t* packet = malloc(1024);

	while (1) {

		receive_frame(packet);

		/* Unrandomize data if necessary */
		if (cfg.csp_rand)
			csp_apply_rand(packet);

		/* Decode Reed-Solomon if selected */
		if (cfg.csp_rs) {
			int ret = csp_fec_decode(packet);
			if (ret < 0) {
				csp_log_error("Failed to decode RS");
				stats.rx_failed++;
				continue;
			}
			csp_log_info("RS corrected %d errors", ret);
			stats.rx_corrected_bytes += ret;
		}

		/* Verify CRC32 if selected */
		if (cfg.csp_crc) {
			if (csp_crc32_verify(packet, true) != CSP_ERR_NONE) {
				csp_log_error("Invalid CRC32");
				continue;
			}
		}

#if 0
		/* Verify HMAC if selected */
		if (cfg.csp_hmac) {
			if (csp_hmac_verify(packet, true) != CSP_ERR_NONE) {
				csp_log_error("Invalid HMAC");
				continue;
			}
		}
#endif

		/* Convert the packet from network to host order */
		packet->id.ext = csp_ntoh32(packet->id.ext);

#if 0
		// Ignore packets from node 10 (client packets loop through zmq otherwise)
		if (packet->id.dst == 10)
			continue;
#endif

		csp_log_packet("RX: Src %u, Dst %u, Dport %u, Sport %u, Pri %u, Flags 0x%02X, Size %"PRIu16,
		               packet->id.src, packet->id.dst, packet->id.dport,
		               packet->id.sport, packet->id.pri, packet->id.flags, packet->length);


		void* publisher = NULL;
		int result = zmq_send(publisher, packet, packet->length, 0);
		if (result < 0)
		{
			fprintf(stderr, "ZMQ send error: %u %s\r\n", result, strerror(result));
		}


	}
}


int main() {

	void* zmq_ctx = zmq_ctx_new();
	assert(zmq_ctx);

	csp_debug_set_level(CSP_WARN, 1);
	csp_debug_set_level(CSP_INFO, 1);
	csp_debug_set_level(CSP_BUFFER, 1);
	csp_debug_set_level(CSP_PACKET, 1);
	csp_debug_set_level(CSP_PROTOCOL, 1);
	csp_debug_set_level(CSP_LOCK, 1);

	csp_iface_t* hub;

	csp_init(&cfg.csp);

	if (cfg.csp_hmac)
		csp_hmac_set_key(cfg.csp_hmac_key, 16);


#if 0
	//csp_zmqhub_init("127.0.0.1", 0, &hub);


	if (csp_route_set(CSP_DEFAULT_ROUTE, &csp_if_zmqegse, CSP_NODE_MAC))
		fprintf(stderr, "error in csp_route_set\n");


	char zmqhost[16] = "127.0.0.1";
	if (csp_zmqhub_make_endpoint(255, zmqhost))
		return 0;
#endif


	pthread_t forwarder_worker;
	pthread_create(&forwarder_worker, NULL, forwarder_task, NULL);


	return 0;
}
