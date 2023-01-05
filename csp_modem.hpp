#pragma once

#include <stdint.h>
#include <csp/csp.h>

/*
 * Global configuration struct
 */
struct CSPModemConfig
{
	unsigned int port;

	bool csp_hmac;
	bool csp_rs;
	bool csp_crc;
	bool csp_rand;
	uint8_t csp_hmac_key[16];
	bool legacy_hmac;

	uint8_t preamble;
	uint8_t preamble_len;

	unsigned int syncword;
	unsigned int syncword_len;
};

extern CSPModemConfig cfg;

int csp_fec_append(csp_packet_t *packet);
int csp_fec_decode(csp_packet_t *packet);
int csp_apply_rand(csp_packet_t *packet);

