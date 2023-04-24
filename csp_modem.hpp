#pragma once

#include <stdint.h>
#include <csp/csp.h>


int csp_fec_append(csp_packet_t *packet);
int csp_fec_decode(csp_packet_t *packet);
int csp_apply_rand(csp_packet_t *packet);

