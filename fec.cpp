
#include "csp_modem.hpp"
#if 0
#include "libfec/fec.h"

#define CSP_RS_LEN      32
#define CSP_RS_MSGLEN   256


int csp_fec_append(csp_packet_t * packet) {
	if (packet->length + CSP_RS_LEN >= 256)
		return CSP_ERR_INVAL;

	return CSP_ERR_DRIVER;
	// encode_rs_8((uint8_t*)&packet->id, &packet->data[packet->length], CSP_RS_MSGLEN - packet->length);
	packet->length += CSP_RS_LEN;

	return CSP_ERR_NONE;
}


int csp_fec_decode(csp_packet_t * packet) {

	if (packet->length < CSP_RS_LEN)
		return CSP_ERR_INVAL;

	return CSP_ERR_DRIVER;
	int ret;
	if ((ret = decode_rs_8((uint8_t*)&packet->id, NULL, 0, CSP_RS_MSGLEN + CSP_RS_LEN - packet->length)) < 0)
		return CSP_ERR_DRIVER; /* Reed-Solomon decode failed */

	return ret;
}

#else

int csp_fec_append(csp_packet_t *packet) {
	return CSP_ERR_DRIVER;
}

int csp_fec_decode(csp_packet_t *packet) {
	return CSP_ERR_DRIVER;
}
#endif