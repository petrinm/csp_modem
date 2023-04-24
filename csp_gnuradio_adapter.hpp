#pragma once

#include <zmq.hpp>

#include <csp/csp.h>
#include <csp/arch/csp_semaphore.h>

int csp_apply_rand(csp_packet_t *packet);

/*
 * 
 */
class CSPGNURadioCAdapter
{
public:

	struct Config
	{
		Config();

		bool use_hmac;
		bool use_rs;
		bool use_crc;
		bool use_rand;
		bool legacy_hmac;
		uint8_t hmac_key[16];

		bool use_xtea;
		uint8_t xtea_key[20];
        bool filter_ground_addresses;
    };

	struct Stats
	{
		unsigned int tx_count;
		unsigned int tx_bytes;
		unsigned int rx_count;
		unsigned int rx_bytes;
		unsigned int rx_failed;
		unsigned int rx_corrected_bytes;
		unsigned int rx_bits_corrected;
	};

	/* Constructor */
	explicit CSPGNURadioCAdapter(const Config &conf = Config());
    ~CSPGNURadioCAdapter();

    CSPGNURadioCAdapter(const CSPGNURadioCAdapter &) = delete;
    CSPGNURadioCAdapter &operator=(const CSPGNURadioCAdapter &) = delete;


    /* Callback function for CSP. Called when a packet should be outputted. */
	int transmit(csp_packet_t *packet);

	/* Block receive call */
	void receive();

	csp_iface_t csp_iface;

private:
	Config conf;
	Stats stats;

	zmq::socket_t sock_pub, sock_sub;
	csp_bin_sem_handle_t tx_wait;

};

