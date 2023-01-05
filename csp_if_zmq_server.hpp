#pragma once

#include <csp/csp.h>

#define CSP_ZMQSERVER_SUBSCRIBE_PORT 6000

#define CSP_ZMQSERVER_PUBLISH_PORT 7000


/**
   Default ZMQ interface name.
*/
#define CSP_ZMQSERVER_IF_NAME "ZMQSERVER"

int csp_zmqserver_init(uint8_t addr, const char *host, uint32_t flags, csp_iface_t **return_interface);


int csp_zmqserver_init_w_endpoints(uint8_t addr,
                                   const char *publisher_endpoint,
                                   const char *subscriber_endpoint,
                                   uint32_t flags,
                                   csp_iface_t **return_interface);

int csp_zmqserver_init_w_name_endpoints_rxfilter(const char *ifname,
                                                 const uint8_t rxfilter[], unsigned int rxfilter_count,
                                                 const char *publish_endpoint,
                                                 const char *subscribe_endpoint,
                                                 uint32_t flags,
                                                 csp_iface_t **return_interface);