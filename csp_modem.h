#ifndef __CSP_MODEM_H__
#define __CSP_MODEM_H__

#include <stdint.h>
#include "csp/csp.h"
#include "csp/csp_types.h"


#define CSP_ERR_RS     -200
#define CSP_ERR_INVAL_LEN -201



/*
 * Global configuration struct
 */
typedef struct {

	unsigned int port;

	csp_conf_t csp;

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

} CSPModemConfig_t;
extern CSPModemConfig_t cfg;

/*
 * Statistics struct
 */
typedef struct {
	unsigned int tx_count;
	unsigned int tx_bytes;

	unsigned int rx_count;
	unsigned int rx_bytes;
	unsigned int rx_failed;
	unsigned int rx_corrected_bytes;
} CSPModemStats_t;
extern CSPModemStats_t stats;


int csp_apply_rand(csp_packet_t* packet);
int csp_fec_append(csp_packet_t* packet);
int csp_fec_decode(csp_packet_t* packet);

#endif
