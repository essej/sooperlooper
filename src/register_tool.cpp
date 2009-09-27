#include <cstdio>
#include <iostream>
#include <cerrno>


#include <time.h>
#include <fcntl.h>


#include <lo/lo.h>


using namespace std;


class RegisterTool
{

public:

   lo_address _osc_addr;
   std::string  _our_url;

	RegisterTool(const string & target_port, const string & sl_port) {
		
		_osc_addr = lo_address_new(NULL, sl_port.c_str());

		char tmpbuf[100];
		snprintf(tmpbuf, sizeof(tmpbuf), "osc.udp://localhost:%s", target_port.c_str()); 
		_our_url = tmpbuf;
	}

void
register_global_updates(const string & path="/ctrl", bool unreg=false)
{
	char buf[50];

	if (!_osc_addr) return;
	
	if (unreg) {
		snprintf(buf, sizeof(buf), "/unregister_update");

	} else {
		snprintf(buf, sizeof(buf), "/register_update");
	}

	// results will come back with instance = -2
	lo_send(_osc_addr, buf, "sss", "tempo", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "sync_source", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "eighth_per_cycle", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "tap_tempo", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "wet", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "dry", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "input_gain", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "auto_disable_latency", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "output_midi_clock", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "use_midi_stop", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "use_midi_start", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "send_midi_start_on_trigger", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "smart_eighths", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "selected_loop_num", _our_url.c_str(), path.c_str());
	
	if (unreg) {
		lo_send(_osc_addr, "/unregister_auto_update", "siss", "in_peak_meter", 100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, "/unregister_auto_update", "siss", "out_peak_meter", 100, _our_url.c_str(), path.c_str());
	}
	else {
		lo_send(_osc_addr, "/register_auto_update", "siss", "in_peak_meter", 100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, "/register_auto_update", "siss", "out_peak_meter", 100, _our_url.c_str(), path.c_str());
	}
}


void
register_auto_updates(int index, const string & path, bool unreg=false)
{
	if (!_osc_addr) return;
	char buf[30];

	if (unreg) {
		snprintf(buf, sizeof(buf), "/sl/%d/unregister_auto_update", index);
		lo_send(_osc_addr, buf, "sss", "state", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "next_state", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "loop_pos", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "loop_len", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "cycle_len", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "free_time", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "total_time", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "waiting", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "rate_output", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "in_peak_meter", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "out_peak_meter", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "is_soloed", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "stretch_ratio", _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "sss", "pitch_shift", _our_url.c_str(), path.c_str());
	} else {
		snprintf(buf, sizeof(buf), "/sl/%d/register_auto_update", index);
		// send request for auto updates
		lo_send(_osc_addr, buf, "siss", "state",     100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss", "next_state",     100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss", "loop_pos",  100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss", "loop_len",  100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss", "cycle_len", 100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss", "free_time", 100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss", "total_time",100,  _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss", "waiting",   100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss", "rate_output",100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss", "in_peak_meter", 100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss", "out_peak_meter", 100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss",  "is_soloed", 100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss", "stretch_ratio", 100, _our_url.c_str(), path.c_str());
		lo_send(_osc_addr, buf, "siss", "pitch_shift", 100, _our_url.c_str(), path.c_str());
	}

	

}

void
register_input_controls(int index, const std::string & path="/ctrl", bool unreg=false)
{
	if (!_osc_addr) return;
	char buf[50];

	if (unreg) {
		snprintf(buf, sizeof(buf), "/sl/%d/unregister_update", index);

	} else {
		snprintf(buf, sizeof(buf), "/sl/%d/register_update", index);
	}
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", "rec_thresh", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "feedback", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "use_feedback_play", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "dry", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "wet", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "input_gain", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "rate", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "scratch_pos", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "delay_trigger", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "quantize", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "fade_samples", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "round", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "sync", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "playback_sync", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "redo_is_tap", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "use_rate", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "use_common_ins", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "use_common_outs", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "relative_sync", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "input_latency", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "output_latency", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "trigger_latency", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "autoset_latency", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "mute_quantized", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "overdub_quantized", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "replace_quantized", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "round_integer_tempo", _our_url.c_str(), path.c_str());
	//lo_send(_osc_addr, buf, "sss", "pitch_shift", _our_url.c_str(), path.c_str());
	//lo_send(_osc_addr, buf, "sss", "stretch_ratio", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "tempo_stretch", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "pan_1", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "pan_2", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "pan_3", _our_url.c_str(), path.c_str());
	lo_send(_osc_addr, buf, "sss", "pan_4", _our_url.c_str(), path.c_str());
}

void
register_control (int index, std::string ctrl, const string & path, bool unreg=false)
{
	if (!_osc_addr) return;

	char buf[30];
	
	if (unreg) {
		snprintf(buf, sizeof(buf), "/sl/%d/unregister_update", index);

	} else {
		snprintf(buf, sizeof(buf), "/sl/%d/register_update", index);
	}
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", (const char *) ctrl.c_str(), _our_url.c_str(), path.c_str());

}


};


int main(int argc, char ** argv)
{

	if (argc < 3) {
		printf("Usage: %s target_port_#  sl_server_port_#  [unregister]\n\n", argv[0]);
		printf("   Where target_port_# is the OSC port of the server you\n    wish to receive status updates from SooperLooper\n");
		printf("   And sl_server_port_# is your SL port (for example use 9951 for the standalone SL,  or 10051 for the AU plugin)\n");
		return 2;
	}

	string targ_port = "8000";
	string sl_port = "9951";
	bool unreg = false;

	if (argc > 2) {
		targ_port = argv[1];
		sl_port = argv[2];
		if (argc > 3) {
			unreg = true;
		}
	}

	RegisterTool regtool(targ_port, sl_port);

	regtool.register_global_updates("/sl/global", unreg);
	
	char pathstr[30];
	for (int i=0; i < 8; ++i) {
		snprintf(pathstr, sizeof(pathstr), "/sl/%d/ctrl", i);
		regtool.register_auto_updates(i, pathstr, unreg);
		regtool.register_input_controls(i, pathstr, unreg);
	}
	
	return 0;
}
