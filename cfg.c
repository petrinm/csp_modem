
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
	.csp_crc = false,
	.csp_rand = true,
	.csp_hmac_key = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.legacy_hmac = false,

	.preamble = 0xAA,
	.preamble_len = 0x50,
	.syncword = 0xC9D08A7B, // 0x930B51DE
	.syncword_len = 32,

};
