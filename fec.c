
#include "csp_modem.h"
#include "libfec/fec.h"

#define CSP_RS_LEN      32
#define CSP_RS_MSGLEN   256



int csp_fec_append(csp_packet_t * packet) {
	if (packet->length + CSP_RS_LEN >= 256)
		return CSP_ERR_INVAL_LEN;

	encode_rs_8((uint8_t*)&packet->id, &packet->data[packet->length], CSP_RS_MSGLEN - packet->length);
	packet->length += CSP_RS_LEN;

	return CSP_ERR_NONE;
}


int csp_fec_decode(csp_packet_t * packet) {

	if (packet->length < CSP_RS_LEN)
		return CSP_ERR_INVAL_LEN;

	int ret;
	if ((ret = decode_rs_8((uint8_t*)&packet->id, NULL, 0, CSP_RS_MSGLEN + CSP_RS_LEN - packet->length)) < 0)
		return CSP_ERR_RS; /* Reed-Solomon decode failed */

	return ret;
}
