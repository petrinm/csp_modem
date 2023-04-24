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
		const float center_frequency = 437.775e6; // [Hz]
		
		/*
		 * SDR
		 */
		SoapySDRIO::Config sdr_conf;
		sdr_conf.rx_on = true;
		sdr_conf.tx_on = true;
		sdr_conf.use_time = 1;
		sdr_conf.samplerate = 500000;
		sdr_conf.tx_latency = 100; // [samples]

		// sdr_conf.buffer = 1024;
		sdr_conf.buffer = (sdr_conf.samplerate / 1000); // buffer lenght in milliseconds

		sdr_conf.args["driver"] = "uhd";

		sdr_conf.rx_centerfreq = 436.00e6;
		sdr_conf.tx_centerfreq = sdr_conf.rx_centerfreq;

		sdr_conf.rx_gain = 30;
		sdr_conf.tx_gain = 60;

		sdr_conf.rx_antenna = "TX/RX";
		sdr_conf.tx_antenna = "TX/RX";

		SoapySDRIO sdr(sdr_conf);


		/*
		 * Setup receiver
		 */
		GMSKContinousDemodulator::Config demodulator_conf;
		demodulator_conf.sample_rate = sdr_conf.samplerate;
		demodulator_conf.center_frequency = center_frequency - sdr_conf.rx_centerfreq;
		demodulator_conf.symbol_rate = 9600;
		demodulator_conf.bt = 0.5;
		demodulator_conf.samples_per_symbol = 4;

		GMSKContinousDemodulator demodulator(demodulator_conf);
		sdr.sinkSamples.connect_member(&demodulator, &GMSKContinousDemodulator::sinkSamples);

		/*
		 * Setup frame decoder
		 */
		GolayDeframer::Config deframer_conf;
		deframer_conf.syncword = 0x930B51DE;
		deframer_conf.syncword_len = 32;
		deframer_conf.sync_threshold = 3;
		deframer_conf.use_viterbi = false;
		deframer_conf.use_randomizer = true;
		deframer_conf.use_rs = true;

		GolayDeframer deframer(deframer_conf);
		deframer.syncDetected.connect_member(&demodulator, &GMSKContinousDemodulator::lockReceiver);
		//deframer.syncDetected.connect([](bool locked, Timestamp now) {
		//	cout << "locked " << locked << endl;
		//});
		demodulator.sinkSymbol.connect_member(&deframer, &GolayDeframer::sinkSymbol);
		demodulator.setMetadata.connect_member(&deframer, &GolayDeframer::setMetadata);

		/*
		 * Setup transmitter
		 */
		GMSKModulator::Config modulator_conf;
		modulator_conf.sample_rate = sdr_conf.samplerate;
		modulator_conf.center_frequency = center_frequency - sdr_conf.tx_centerfreq;
		modulator_conf.symbol_rate = 9600;
		modulator_conf.bt = 0.5;
		modulator_conf.ramp_up_duration = 2; // [symbols]
		modulator_conf.ramp_down_duration = 2; // [symbols]

		GMSKModulator modulator(modulator_conf);
		sdr.generateSamples.connect_member(&modulator, &GMSKModulator::generateSamples);

		/*
		 * Setup framer
		 */
		GolayFramer::Config framer_conf;
		framer_conf.syncword = deframer_conf.syncword;
		framer_conf.syncword_len = deframer_conf.syncword_len;
		framer_conf.preamble_len = 24 * 8; // [symbols]
		framer_conf.use_viterbi = false;
		framer_conf.use_randomizer = true;
		framer_conf.use_rs = true;

		GolayFramer framer(framer_conf);
		modulator.generateSymbols.connect_member(&framer, &GolayFramer::generateSymbols);


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
		CSPSuoAdapter::Config csp_adapter_conf;
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

		CSPSuoAdapter csp_adapter(csp_adapter_conf);
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
		tracker_conf.center_frequency = center_frequency; // [Hz]

		PorthouseTracker tracker(tracker_conf);
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
