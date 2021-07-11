
#include "csp_modem.h"

CSPModemConfig_t cfg = {

	.csp = {
		.address = 1,
		.hostname = "csp-modem",
		.model = "CSP-Modem",
		.revision = "v1",
		.conn_max = 10,
		.conn_queue_length = 10,
		.fifo_length = 25,
		.port_max_bind = 24,
		.rdp_max_window = 20,
		.buffers = 10,
		.buffer_data_size = 256,
		.conn_dfl_so = CSP_O_NONE,
	},

	.csp_hmac = true,
	.csp_rs = true,
	.csp_crc = true,
	.csp_rand = true,
	.csp_hmac_key = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.legacy_hmac = false,
	.preamb = 0xAA,
	.preamblen = 0x50,
};
