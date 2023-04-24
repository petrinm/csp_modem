
#include <string>
#include <iostream>


/* CSP stuff */
#include <csp/csp.h>
#include <csp/arch/csp_thread.h>

#include "csp_if_zmq_server.hpp"
#include "csp_gnuradio_adapter.hpp"


using namespace std;


CSP_DEFINE_TASK(service_task)
{
	(void)param;
	csp_socket_t *sock = csp_socket(CSP_SO_NONE);

	csp_bind(sock, CSP_ANY);
	csp_listen(sock, 10);

	while (1)
	{
		csp_conn_t *conn;
		if ((conn = csp_accept(sock, 10000)) == NULL)
			continue;

		csp_packet_t *packet;
		while ((packet = csp_read(conn, 50)) != NULL)
			csp_service_handler(conn, packet);

		csp_close(conn);
	}
}


int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;


	/*
	 * CSP stuff
	 */
	csp_debug_set_level(CSP_WARN, 1);
	csp_debug_set_level(CSP_INFO, 1);
	csp_debug_set_level(CSP_PACKET, 1);
	csp_debug_set_level(CSP_PROTOCOL, 1);

	/* Initialize CSP */
	csp_conf_t csp_conf;
	csp_conf_get_defaults(&csp_conf);
	csp_conf.address = (argc > 1 ? 9 : 7);
	csp_conf.hostname = "gnuradiobridge";
	csp_conf.model = "GNURadioBridge";
	csp_conf.revision = "v1";
	csp_init(&csp_conf);


	/* Start the routing task */
	csp_route_start_task(1000, 0);

	/* 
	 * Setup CSP ZMQ interface
	 */
	csp_iface_t* csp_zmq_if;
	if (csp_zmqserver_init(CSP_NO_VIA_ADDRESS, "0.0.0.0", 0, &csp_zmq_if) != CSP_ERR_NONE)
		throw runtime_error("csp_zmqserver_init");

#ifdef CSP_RTABLE_CIDR 
	// Route packets going to addresses 8-15 to ZMQ
	if (csp_rtable_set(8, 3, csp_zmq_if, CSP_NODE_MAC) != CSP_ERR_NONE)
		throw SuoError("csp_rtable_set");
#else
	// Route packets going to addresses 8-15 to ZMQ
	for (uint8_t addr = 8; addr < 16; addr++)
		if (csp_route_set(addr, csp_zmq_if, CSP_NODE_MAC) != CSP_ERR_NONE)
			throw runtime_error("csp_route_set");
#endif

	/*
	 * Setup CSP proxy
	 */

	/*use_hmac = false;
	use_rs = true;
	use_crc = false;
	use_rand = false;
	legacy_hmac = false;
	hmac_key[16] = {0};
	use_xtea = false;
	xtea_key[20] = {0};
	filter_ground_addresses = true;*/

	CSPGNURadioCAdapter::Config csp_adapter_conf;
#ifdef EXTERNAL_SECRET
#include "secret.hpp"
	csp_adapter_conf.use_hmac = true;
	memcpy(csp_adapter_conf.hmac_key, secret_key, sizeof(csp_adapter_conf.hmac_key));
#else
	csp_adapter_conf.use_hmac = false;
#endif
	csp_adapter_conf.use_rs = false; // Done by GolayFramer/GolayDeframer
	csp_adapter_conf.use_rand = false; // Done by GolayFramer/GolayDeframer
	csp_adapter_conf.use_crc = false;

	CSPGNURadioCAdapter csp_adapter(csp_adapter_conf);
	//framer.sourceFrame.connect_member(&csp_adapter, &CSPSuoAdapter::sourceFrame);
	//deframer.sinkFrame.connect_member(&csp_adapter, &CSPSuoAdapter::sinkFrame);

#ifdef CSP_RTABLE_CIDR
	// Route packets going to addresses 0-7 to space 
	if (csp_rtable_set(0, 3, &csp_adapter.csp_iface, CSP_NODE_MAC) != CSP_ERR_NONE)
		throw runtime_error("csp_rtable_set");
#else
	// Route packets going to addresses 0-7 to space
	for (uint8_t addr = 0; addr < 8; addr++)
		if (csp_route_set(addr, &csp_adapter.csp_iface, CSP_NODE_MAC) != CSP_ERR_NONE)
			throw runtime_error("csp_route_set");
#endif


	/* Start CSP service server */
	csp_thread_create(service_task, "Service", 1000, NULL, 0, NULL);


	/*
	 * Run!
	 */
	while (1) {
		csp_adapter.receive();
	}

	cerr << "Suo exited" << endl;

	return 0;
}
