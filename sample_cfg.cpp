#include "csp_modem.hpp"

using namespace std;
using namespace suo;


// given in Hz
#define CENTER_FREQUENCY 437.5e6


float cfg_center_frequency() {
	cout << "NOTE: Using sample config file!" << endl;
	cout << "      Please make your own file for your settings and give it to" << endl;
	cout << "      cmake using -DSATELLITE_CONFIG_CPP=[your config file]" << endl;
	return CENTER_FREQUENCY;
}


SoapySDRIO::Config cfg_sdr() {
	SoapySDRIO::Config c;
	c.rx_on = true;
	c.tx_on = true;
	c.use_time = 1;
	c.samplerate = 8000000;
	c.tx_latency = 100; // [samples]

	// c.buffer = 1024;
	c.buffer = (c.samplerate / 1000); // buffer lenght in milliseconds

	c.args["driver"] = "leecher"; // "uhd";

	c.rx_centerfreq = 436.00e6;
	c.tx_centerfreq = c.rx_centerfreq;

	c.rx_gain = 30;
	c.tx_gain = 60;

	c.rx_antenna = "TX/RX";
	c.tx_antenna = "TX/RX";

	return c;
}
SoapySDRIO::Config sdr_conf = cfg_sdr();


GMSKContinousDemodulator::Config cfg_gmsk_demodulator()
{
	GMSKContinousDemodulator::Config c;
	c.sample_rate = sdr_conf.samplerate;
	c.center_frequency = CENTER_FREQUENCY - sdr_conf.rx_centerfreq;
	c.symbol_rate = 9600;
	c.bt = 0.5;
	c.samples_per_symbol = 4;
	c.symsync_bandwidth0 = 0.01;
	c.symsync_bandwidth1 = 0.01;
	c.symsync_rate_tol = 0.01;
	c.verbose = false;

	return c;
}


GolayDeframer::Config cfg_golay_deframer()
{
	GolayDeframer::Config c;
	c.syncword = 0x930B51DE;
	c.syncword_len = 32;
	c.sync_threshold = 3;
	c.use_viterbi = false;
	c.use_randomizer = true;
	c.use_rs = true;
	c.verbose = true;

	return c;
}
GolayDeframer::Config deframer_conf = cfg_golay_deframer();


GMSKModulator::Config cfg_gmsk_modulator()
{
	GMSKModulator::Config c;
	c.sample_rate = sdr_conf.samplerate;
	c.center_frequency = CENTER_FREQUENCY - sdr_conf.tx_centerfreq;
	c.symbol_rate = 9600;
	c.bt = 0.5;
	c.ramp_up_duration = 2; // [symbols]
	c.ramp_down_duration = 2; // [symbols]

	return c;
}


GolayFramer::Config cfg_golay_framer()
{
	GolayFramer::Config c;
	c.syncword = deframer_conf.syncword;
	c.syncword_len = deframer_conf.syncword_len;
	c.preamble_len = 24 * 8; // [symbols]
	c.use_viterbi = false;
	c.use_randomizer = true;
	c.use_rs = true;
	return c;
}


CSPSuoAdapter::Config cfg_csp_suo_adapter()
{
	CSPSuoAdapter::Config c;
	c.rx_use_rs = false;  // Done by GolayDeframer
	c.rx_use_crc = true;
	c.rx_use_rand = false;  // Done by GolayDeframer
	c.rx_use_hmac = false;
	c.rx_legacy_hmac = false;
	// c.rx_hmac_key;
	c.rx_use_xtea = false;
	// c.rx_xtea_key;
	c.rx_filter_ground_addresses = true;

	c.tx_use_rs = false;  // Done by GolayFramer
	c.tx_use_crc = false;
	c.tx_use_rand = false;  // Done by GolayFramer
	c.tx_use_hmac = true;
	c.tx_legacy_hmac = false;

	uint8_t hmac_key[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	memcpy(c.tx_hmac_key, hmac_key, sizeof(c.tx_hmac_key));
	
	c.tx_use_xtea = false;
	// c.tx_xtea_key

	return c;
}


#ifdef USE_PORTHOUSE_TRACKER
PorthouseTracker::Config cfg_tracker()
{
	PorthouseTracker::Config c;
	c.amqp_url = "amqp://guest:guest@localhost/";
	c.target_name = "SampleSat-1000";
	c.center_frequency = CENTER_FREQUENCY; // [Hz]

	return c;
}
#endif
