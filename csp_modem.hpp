#pragma once
#include "csp_suo_adapter.hpp"

#include <stdint.h>
#include <csp/csp.h>

#include <suo.hpp>
#include <signal-io/soapysdr_io.hpp>
#include <modem/demod_gmsk_cont.hpp>
#include <modem/mod_gmsk.hpp>
#include <framing/golay_framer.hpp>
#include <framing/golay_deframer.hpp>
#ifdef USE_PORTHOUSE_TRACKER
#include <misc/porthouse_tracker.hpp>
#endif

using namespace std; 
using namespace suo;

int csp_fec_append(csp_packet_t *packet);
int csp_fec_decode(csp_packet_t *packet);
int csp_apply_rand(csp_packet_t *packet);

/*
 Functions returning config structs and implemented by SATELLITE_CONFIG_CPP
 */
float cfg_center_frequency();
SoapySDRIO::Config cfg_sdr();
GMSKContinousDemodulator::Config cfg_gmsk_demodulator();
GolayDeframer::Config cfg_golay_deframer();
GMSKModulator::Config cfg_gmsk_modulator();
GolayFramer::Config cfg_golay_framer();
CSPSuoAdapter::Config cfg_csp_suo_adapter();

#ifdef USE_PORTHOUSE_TRACKER
PorthouseTracker::Config cfg_tracker();
#endif
