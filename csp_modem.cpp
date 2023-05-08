#include "csp_modem.hpp"

#include <string>
#include <iostream>

/* Suo stuff */
#include <suo.hpp>
#include <signal-io/soapysdr_io.hpp>
#include <modem/demod_fsk_mfilt.hpp>
#include <modem/demod_gmsk_cont.hpp>
#include <modem/mod_gmsk.hpp>
#include <framing/golay_framer.hpp>
#include <framing/golay_deframer.hpp>
#include <frame-io/zmq_interface.hpp>

#ifdef USE_PORTHOUSE_TRACKER
#include <misc/porthouse_tracker.hpp>
#endif
#ifdef USE_RIGCTL
#include <misc/rigctl.hpp>
#endif

#include "csp_suo_adapter.hpp"

/* CSP stuff */
#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include "csp_if_zmq_server.hpp"

using namespace std;
using namespace suo;



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

	try
	{
		const float center_frequency = cfg_center_frequency();
		
		// SDR
		SoapySDRIO sdr(cfg_sdr());

		// Setup receiver
		GMSKContinousDemodulator demodulator(cfg_gmsk_demodulator());
		sdr.sinkSamples.connect_member(&demodulator, &GMSKContinousDemodulator::sinkSamples);

		// Setup frame decoder
		GolayDeframer deframer(cfg_golay_deframer());
		deframer.syncDetected.connect_member(&demodulator, &GMSKContinousDemodulator::lockReceiver);
		//deframer.syncDetected.connect([](bool locked, Timestamp now) {
		//	cout << "locked " << locked << endl;
		//});
		demodulator.sinkSymbol.connect_member(&deframer, &GolayDeframer::sinkSymbol);
		demodulator.setMetadata.connect_member(&deframer, &GolayDeframer::setMetadata);

		// Setup transmitter
		GMSKModulator modulator(cfg_gmsk_modulator());
		sdr.generateSamples.connect_member(&modulator, &GMSKModulator::generateSamples);

		// Setup framer
		GolayFramer framer(cfg_golay_framer());
		modulator.generateSymbols.connect_member(&framer, &GolayFramer::generateSymbols);


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
		csp_conf.address = 9;
		csp_conf.hostname = "csp-modem";
		csp_conf.model = "CSPModem";
		csp_conf.revision = "v1";
		csp_init(&csp_conf);

		/* Start the routing task */
		csp_route_start_task(1000, 0);

		// Setup CSP ZMQ interface
		csp_iface_t* csp_zmq_if;
		if (csp_zmqserver_init(CSP_NO_VIA_ADDRESS, "0.0.0.0", 0, &csp_zmq_if) != CSP_ERR_NONE)
			throw SuoError("csp_zmqserver_init");

#ifdef CSP_RTABLE_CIDR 
		// Route packets going to addresses 8-15 to ZMQ
		if (csp_rtable_set(8, 3, csp_zmq_if, CSP_NODE_MAC) != CSP_ERR_NONE)
			throw SuoError("csp_rtable_set");
#else
		// Route packets going to addresses 8-15 to ZMQ
		for (uint8_t addr = 8; addr < 16; addr++)
			if (csp_route_set(addr, csp_zmq_if, CSP_NODE_MAC) != CSP_ERR_NONE)
				throw SuoError("csp_route_set");
#endif

		//Setup CSP proxy
		CSPSuoAdapter csp_adapter(cfg_csp_suo_adapter());
		framer.sourceFrame.connect_member(&csp_adapter, &CSPSuoAdapter::sourceFrame);
		deframer.sinkFrame.connect_member(&csp_adapter, &CSPSuoAdapter::sinkFrame);

#ifdef CSP_RTABLE_CIDR
		// Route packets going to addresses 0-7 to space 
		if (csp_rtable_set(0, 3, &csp_adapter.csp_iface, CSP_NODE_MAC) != CSP_ERR_NONE)
			throw SuoError("csp_rtable_set");
#else
		// Route packets going to addresses 0-7 to space
		for (uint8_t addr = 0; addr < 8; addr++)
			if (csp_route_set(addr, &csp_adapter.csp_iface, CSP_NODE_MAC) != CSP_ERR_NONE)
				throw SuoError("csp_route_set");
#endif

		/* Start CSP service server */
		csp_thread_create(service_task, "Service", 1000, NULL, 0, NULL);


#ifdef USE_PORTHOUSE_TRACKER
		// Setup porthouse tracker
		PorthouseTracker tracker(cfg_tracker());
		tracker.setUplinkFrequency.connect([&] (float frequency) {
			modulator.setFrequencyOffset(frequency - center_frequency);
		});
		tracker.setDownlinkFrequency.connect([&] (float frequency) {
			demodulator.setFrequencyOffset(frequency - center_frequency);
		});
		sdr.sinkTicks.connect_member(&tracker, &PorthouseTracker::tick);
#endif

#ifdef USE_RIGCTL_TRACKER
		/*
		 * Setup rigctl for frequency tracking
		 */
		RigCtl rigctl;
		rigctl.setUplinkFrequency.connect([&] (float frequency) {
			modulator.setFrequencyOffset(frequency - center_frequency);
		});
		rigctl.setDownlinkFrequency.connect([&] (float frequency) {
			demodulator.setFrequencyOffset(frequency - center_frequency);
		});
		sdr.sinkTicks.connect_member(&rigctl, &RigCtl::tick);
#endif

#ifdef OUTPUT_RAW_FRAMES
		/*
		 * ZMQ output
		 */
		ZMQPublisher::Config zmq_output_conf;
		zmq_output_conf.bind = "tcp://0.0.0.0:7005";

		ZMQPublisher zmq_output(zmq_output_conf);
		//deframer.sinkFrame.connect_member(&zmq_output, &ZMQPublisher::sinkFrame);
		//framer.sinkFrame.connect_member(&zmq_output, &ZMQPublisher::sinkFrame);

		deframer.sinkFrame.connect([&](const Frame& frame, Timestamp now) {
			Frame copy_frame(frame);
			copy_frame.setMetadata("packet_type", "uplink");
			zmq_output.sinkFrame(copy_frame, now);
		});

		deframer.sinkFrame.connect([&](const Frame& frame, Timestamp now) {
			Frame copy_frame(frame);
			copy_frame.setMetadata("packet_type", "downlink");
			zmq_output.sinkFrame(copy_frame, now);
		});

#endif

		/*
		 * Run!
		 */
		sdr.execute();
		cerr << "Suo exited" << endl;

		return 0;
	}
	catch (const SuoError &e)
	{
		cerr << "SuoError: " << e.what() << endl;
		return 1;
	}
}
