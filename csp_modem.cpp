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

#ifdef USE_PORTHOUSE_TRACKER
#include <misc/porthouse_tracker.hpp>
#endif
#ifdef USE_RIGCTL
#include <misc/rigctl.hpp>
#endif

#include "csp_adapter.hpp"

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
		/*
		 * SDR
		 */
		SoapySDRIO::Config sdr_conf;
		sdr_conf.rx_on = true;
		sdr_conf.tx_on = false;
		sdr_conf.use_time = 1;
		sdr_conf.samplerate = 500000;
		sdr_conf.tx_latency = 100; // [samples]

		// sdr_conf.buffer = 1024;
		sdr_conf.buffer = (sdr_conf.samplerate / 1000); // buffer lenght in milliseconds

		sdr_conf.args["driver"] = "uhd";

		sdr_conf.rx_centerfreq = 437.00e6;
		sdr_conf.tx_centerfreq = 437.00e6;

		sdr_conf.rx_gain = 30;
		sdr_conf.tx_gain = 60;

		sdr_conf.rx_antenna = "TX/RX";
		sdr_conf.tx_antenna = "TX/RX";

		SoapySDRIO sdr(sdr_conf);


		/*
		 * Setup receiver
		 */
		GMSKContinousDemodulator::Config receiver_conf;
		receiver_conf.symbol_rate = 9600;

		GMSKContinousDemodulator receiver(receiver_conf);
		sdr.sinkSamples.connect_member(&receiver, &GMSKContinousDemodulator::sinkSamples);


		/*
		 * Setup frame decoder
		 */
		GolayDeframer::Config deframer_conf;
		deframer_conf.sync_word = 0x930B51DE;
		deframer_conf.sync_len = 32;
		deframer_conf.sync_threshold = 3;
		deframer_conf.skip_rs = false;
		deframer_conf.skip_randomizer = false;
		deframer_conf.skip_viterbi = false;

		GolayDeframer deframer(deframer_conf);
		deframer.syncDetected.connect_member(&receiver, &GMSKContinousDemodulator::lockReceiver);
		receiver.sinkSymbol.connect_member(&deframer, &GolayDeframer::sinkSymbol);


		/*
		 * Setup transmitter
		 */
		GMSKModulator::Config modulator_conf;
		modulator_conf.sample_rate = sdr_conf.samplerate;
		modulator_conf.symbol_rate = 9600;
		modulator_conf.center_frequency = 125.0e3;
		modulator_conf.bt = 0.5;
		modulator_conf.ramp_up_duration = 2;
		modulator_conf.ramp_down_duration = 2;

		GMSKModulator modulator(modulator_conf);
		sdr.sourceSamples.connect_member(&modulator, &GMSKModulator::sourceSamples);

		/*
		 * Setup framer
		 */
		GolayFramer::Config framer_conf;
		framer_conf.sync_word = 0x930B51DE;
		framer_conf.sync_len = 32;
		framer_conf.preamble_len = 12 * 8;
		framer_conf.use_viterbi = false;
		framer_conf.use_randomizer = true;
		framer_conf.use_rs = true;

		GolayFramer framer(framer_conf);
		modulator.sourceSymbols.connect_member(&framer, &GolayFramer::sourceSymbols);


		/*
		 * CSP stuff
		 */

		csp_debug_set_level(CSP_WARN, 1);
		csp_debug_set_level(CSP_INFO, 1);
		csp_debug_set_level(CSP_PACKET, 1);
		csp_debug_set_level(CSP_PROTOCOL, 1);

#ifdef OLD_CSP
		/* Initialize CSP */
		csp_set_hostname("csp-modem");
		csp_set_model("CSP Modem");
		csp_buffer_init(400, 512);
		csp_init(9);
		csp_rdp_set_opt(6, 30000, 16000, 1, 8000, 3);
#else
		/* Initialize CSP */
		csp_conf_t csp_conf;
		csp_conf_get_defaults(&csp_conf);
		csp_conf.address = 9;
		csp_conf.hostname = "csp-modem";
		csp_conf.model = "CSPModem";
		csp_conf.revision = "v1";
		csp_init(&csp_conf);
#endif

		/* Start the routing task */
		csp_route_start_task(1000, 0);

		/* 
		 * Setup CSP ZMQ interface
		 */
		csp_iface_t* csp_zmq_if;
#ifdef OLD_CSP
		if (csp_zmqserver_init(CSP_NODE_MAC, "0.0.0.0", 0, &csp_zmq_if) != CSP_ERR_NONE)
			throw SuoError("csp_zmqserver_init");
#else
		if (csp_zmqserver_init(CSP_NO_VIA_ADDRESS, "0.0.0.0", 0, &csp_zmq_if) != CSP_ERR_NONE)
			throw SuoError("csp_zmqserver_init");

#endif

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

		/*
		 * Setup CSP proxy
		 */
		CSPAdapter::Config csp_adapter_conf;
		csp_adapter_conf.use_hmac = true;
		//memcpy(csp_adapter_conf.hmac_key, secret_key, sizeof(csp_adapter_conf.hmac_key));
		csp_adapter_conf.use_rs = false; // Done by GolayFramer/GolayDeframer
		csp_adapter_conf.use_rand = false; // Done by GolayFramer/GolayDeframer
		csp_adapter_conf.use_crc = true;

		CSPAdapter csp_adapter(csp_adapter_conf);
		framer.sourceFrame.connect_member(&csp_adapter, &CSPAdapter::sourceFrame);
		deframer.sinkFrame.connect_member(&csp_adapter, &CSPAdapter::sinkFrame);

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
#ifdef OLD_CSP
		csp_thread_create(service_task, (const signed char *const)"Service", 1000, NULL, 0, NULL);
#else
		csp_thread_create(service_task, "Service", 1000, NULL, 0, NULL);
#endif


#ifdef USE_PORTHOUSE_TRACKER
		/*
		 * Setup porthouse tracker
		 */
		PorthouseTracker::Config tracker_conf;
		tracker_conf.amqp_url = "amqp://guest:guest@localhost/";
		tracker_conf.target_name = "Suomi-100";
		tracker_conf.center_frequency = 437.775e6; // [Hz]

		PorthouseTracker tracker(tracker_conf);
		//tracker.setUplinkFrequency.connect_member(&modulator, &GMSKModulator::setFrequency);
		//tracker.setDownlinkFrequency.connect_member(&demodulator, &GMSKContinousDemodulator::setFrequency);
		sdr.sinkTicks.connect_member(&tracker, &PorthouseTracker::tick);
#endif

#ifdef USE_RIGCTL
		/*
		 * Setup rigctl for frequency tracking
		 */
		RigCtl rigctl;
		//rigctl.setUplinkFrequency.connect_member(&modulator, &GMSKModulator::setFrequency);
		//rigctl.setDownlinkFrequency.connect_member(&demodulator, &GMSKContinousDemodulator::setFrequency);
		sdr.sinkTicks.connect_member(&rigctl, &RigCtl::tick);
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
