#pragma once

#include <suo.hpp>
#include <zmq.hpp>

#include <csp/csp.h>
#include <csp/arch/csp_semaphore.h>

/* 
 * Suo block to connect
 */
class CSPAdapter : public suo::Block
{
public:

	struct Config {
		Config();

		bool use_hmac;
		bool use_rs;
		bool use_crc;
		bool use_rand;
		bool legacy_hmac;
		uint8_t hmac_key[16];

		bool use_xtea;
		uint8_t xtea_key[20];
	};

	struct Stats {
		unsigned int tx_count;
		unsigned int tx_bytes;
		unsigned int rx_count;
		unsigned int rx_bytes;
		unsigned int rx_failed;
		unsigned int rx_corrected_bytes;
	};

	explicit CSPAdapter(const Config &conf = Config());
	~CSPAdapter();

	void sourceFrame(suo::Frame &frame, suo::Timestamp now);
	void sinkFrame(suo::Frame &frame, suo::Timestamp now);
	void receiverLocked(bool locked, suo::Timestamp now);

	/* Callback function for CSP. Called when a packet should be outputted. */
	int csp_transmit(csp_packet_t *packet);

	csp_iface_t csp_iface;

private:
	Config conf;
	Stats stats;

	csp_bin_sem_handle_t tx_wait;
	csp_packet_t * tx_packet;
	bool tx_acked;

};