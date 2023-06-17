/*
** Copyright (C) 2004 Jesse Chappell <jesse@essej.net>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif


#include "looper.hpp"

#include <iostream>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <sys/time.h>
#include <time.h>
#include <libgen.h>

#ifdef HAVE_SNDFILE
#include <sndfile.h>
#endif

#include <rubberband/RubberBandStretcher.h>

#include "ladspa.h"
#include "plugin.hpp"
#include "filter.hpp"
#include "engine.hpp"
#include "utils.hpp"
#include "panner.hpp"
#include "command_map.hpp"



using namespace std;
using namespace SooperLooper;
using namespace PBD;
using namespace RubberBand;

extern	LADSPA_Descriptor* create_sl_descriptor ();
extern	void cleanup_sl_descriptor (LADSPA_Descriptor *);



static const double MinResamplingRate = 0.25f;
static const double MaxResamplingRate = 8.0f;
static const int SrcAudioQuality = SRC_LINEAR;


Looper::Looper (AudioDriver * driver, unsigned int index, unsigned int chan_count, float loopsecs, bool discrete)
	: _driver (driver), _index(index), _chan_count(chan_count), _loopsecs(loopsecs)
{
	initialize (index, chan_count, loopsecs, discrete);
}

Looper::Looper (AudioDriver * driver, XMLNode & node)
	: _driver (driver)
{
	_index = 0; // set from state
	_chan_count = 1; // set from state
	_loopsecs = 80.0f;
	_have_discrete_io = false;
	_is_soloed = false;

	if (set_state (node) < 0) {
		cerr << "Set state errored" << endl;
		initialize (_index, _chan_count); // default backup
	}
}

bool
Looper::initialize (unsigned int index, unsigned int chan_count, float loopsecs, bool discrete)
{
	char tmpstr[100];
	int dummyerror;

	_index = index;
	_chan_count = chan_count;
	
	_ok = false;
	requested_cmd = -1;
	last_requested_cmd = -1;
	request_pending = false;
	_input_ports = 0;
	_output_ports = 0;
	_instances = 0;
	_buffersize = 0;
	_use_sync_buf = 0;
	_our_syncin_buf = 0;
	_our_syncout_buf = 0;
	_dummy_buf = 0;
	_tmp_io_bufs = 0;
	_running_frames = 0;
	_use_common_ins = true;
	_use_common_outs = true;
	_auto_latency = true;  // default for now
	_disable_latency = false;
	_last_trigger_latency = 0.0f;
	_last_input_latency = 0.0f;
	_last_output_latency = 0.0f;
	_have_discrete_io = discrete;
    _discrete_prefader = true;
	_curr_dry = 0.0f;
	_target_dry = 0.0f;
    _curr_wet = 1.0f;
    _target_wet = 1.0f;
	_curr_input_gain = 1.0f;
	_targ_input_gain = 1.0f;
	_input_peak = 0.0f;
	_output_peak = 0.0f;
	_panner = 0;
	_relative_sync = false;
	descriptor = 0;
	_pre_solo_muted = false;
	_stretch_ratio = 1.0;
	_pitch_shift = 0.0;
	_stretch_buffer = 0;
	_tempo_stretch = false;
	_pending_stretch = false;
	_pending_stretch_ratio = 0.0;
	_is_soloed = false;

	if (!descriptor) {
		descriptor = create_sl_descriptor ();
	}


	_instances = new LADSPA_Handle[_chan_count];
	_input_ports = new port_id_t[_chan_count];
	_output_ports = new port_id_t[_chan_count];
	
	// SRC stuff
	_in_src_states = new SRC_STATE*[_chan_count];
	_out_src_states = new SRC_STATE*[_chan_count];
	memset (_in_src_states, 0, sizeof(SRC_STATE*) * _chan_count);
	memset (_out_src_states, 0, sizeof(SRC_STATE*) * _chan_count);

	_insync_src_state = src_new (SRC_LINEAR, 1, &dummyerror);
	_outsync_src_state = src_new (SRC_LINEAR, 1, &dummyerror);

	_src_sync_buffer = 0;
	_src_in_buffer = 0;
	_src_buffer_len = 0;
	_src_in_ratio = 1.0;
	_src_out_ratio = 1.0;

	_lp_filter = new OnePoleFilter*[_chan_count];
	memset (_lp_filter, 0, sizeof(OnePoleFilter*) * _chan_count);


	_tmp_io_bufs = new float*[_chan_count];
	memset(_tmp_io_bufs, 0, sizeof(float *) * _chan_count);

	nframes_t srate = _driver->get_samplerate();
	
	// rubberband stretch stuff
	_in_stretcher = new RubberBandStretcher(srate, _chan_count, 
					     RubberBandStretcher::OptionProcessRealTime | RubberBandStretcher::OptionTransientsCrisp);
	_out_stretcher = new RubberBandStretcher(srate, _chan_count, 
					     RubberBandStretcher::OptionProcessRealTime | RubberBandStretcher::OptionTransientsCrisp);

	
	memset (_instances, 0, sizeof(LADSPA_Handle) * _chan_count);
	memset (_input_ports, 0, sizeof(port_id_t) * _chan_count);
	memset (_output_ports, 0, sizeof(port_id_t) * _chan_count);
	memset (ports, 0, sizeof(float) * LASTPORT);

	memset(_down_stamps, 0, sizeof(nframes_t) * (Event::LAST_COMMAND+1));
        /*
	for (int i=0; i < (int) Event::LAST_COMMAND+1; ++i) {
	  _down_stamps[i] = 1 << 31;
	}
        */

	set_buffer_size(_driver->get_buffersize());

	_longpress_frames = (nframes_t) lrint (srate * 1.0); // more than 1 secs is SUS
	_doubletap_frames = (nframes_t) lrint (srate * 0.5); // less than 0.5 sec is double tap

	_falloff_per_sample = 30.0f / srate; // 30db per second falloff
	
	// set some rational defaults
	ports[DryLevel] = 0.0f;
    if (_have_discrete_io && _discrete_prefader) {
        ports[WetLevel] = 1.0f;
    } else {
        ports[WetLevel] = _curr_wet;
    }
	ports[Feedback] = 1.0f;
	ports[Rate] = 1.0f;
	ports[Multi] = -1.0f;
	ports[Sync] = 0.0f;
	ports[Quantize] = 0.0f;
	ports[UseRate] = 0.0f;
	ports[FadeSamples] = 64.0f;
	ports[PlaybackSync] = 0.0f;
	ports[UseSafetyFeedback] = 1.0f;
	ports[TriggerLatency] = 0;
	ports[MuteQuantized] = 0;
	
	ports[RoundIntegerTempo] = 0;

	_slave_sync_port = (_relative_sync && ports[Sync]) ? 2.0f : 1.0f;

	// TODO: fix hack to specify loop length
	char looptimestr[20];
	snprintf(looptimestr, sizeof(looptimestr), "%f", loopsecs);
	setenv("SL_SAMPLE_TIME", looptimestr, 1);

	
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		_tmp_io_bufs[i] = new float[_buffersize];

		if ((_instances[i] = descriptor->instantiate (descriptor, srate)) == 0) {
			return false;
		}

		sl_set_loop_index(_instances[i], (int)_index, i);
		
		if (_have_discrete_io) 
		{
			snprintf(tmpstr, sizeof(tmpstr), "loop%d_in_%d", _index, i+1);
			
			if (!_driver->create_input_port (tmpstr, _input_ports[i])) {
				
				cerr << "cannot register loop input port\n";
				_have_discrete_io = false;
			}
			
			snprintf(tmpstr, sizeof(tmpstr), "loop%d_out_%d", _index, i+1);
			
			if (!_driver->create_output_port (tmpstr, _output_ports[i]))
			{
				cerr << "cannot register loop output port\n";
				_have_discrete_io = false;
			}
		}

		/* connect all scalar ports to data values */
		
		for (unsigned long n = 0; n < LASTPORT; ++n) {
			descriptor->connect_port (_instances[i], n, &ports[n]);
		}

		// connect dedicated Sync port to all other channels
		if (i > 0) {
			descriptor->connect_port (_instances[i], Sync, &_slave_sync_port);

			descriptor->connect_port (_instances[i], State, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], LoopLength, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], LoopPosition, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], CycleLength, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], LoopFreeMemory, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], LoopMemory, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], Waiting, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], TrueRate, &_slave_dummy_port);
		}
		
		descriptor->activate (_instances[i]);

		_lp_filter[i] = new OnePoleFilter(srate);
		
		// SRC stuff
		_in_src_states[i] = src_new (SrcAudioQuality, 1, &dummyerror);
		_out_src_states[i] = src_new (SrcAudioQuality, 1, &dummyerror);
		_lp_filter[i]->set_cutoff (_src_in_ratio * _lp_filter[i]->get_samplerate() * 0.48f);
		
		
	}

	size_t comnouts = _driver->get_engine()->get_common_output_count();
	_panner = 0;
	if (comnouts > 1) {
		// we really only support panning to 2 outputs right now
		_panner = new Panner("pan");
		_panner->reset (comnouts, _chan_count);

		if (_chan_count == 1) {
			(*_panner)[0]->set_position (0.5f);
		}
		else if (_chan_count == 2) {
			(*_panner)[0]->set_position (0.0f);
			(*_panner)[1]->set_position (1.0f);
		}
		else {
			// eh, no good defaults
		}
	}

	_ok = true;

	return _ok;
}


Looper::~Looper ()
{
	destroy();
	
	if (descriptor) {
		cleanup_sl_descriptor (descriptor);
	}
	
}


void
Looper::destroy()
{
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		if (_instances[i]) {
			if (descriptor->deactivate) {
				descriptor->deactivate (_instances[i]);
			}
			if (descriptor->cleanup) {
				descriptor->cleanup (_instances[i]);
			}
			_instances[i] = 0;
		}

		if (_input_ports[i]) {
			_driver->destroy_input_port (_input_ports[i]);
			_input_ports[i] = 0;
		}
		
		if (_output_ports[i]) {
			_driver->destroy_output_port (_output_ports[i]);
			_output_ports[i] = 0;
		}


		if (_out_src_states[i]) {
			src_delete (_out_src_states[i]);
		}
		if (_in_src_states[i]) {
			src_delete (_in_src_states[i]);
		}


		if (_lp_filter[i]) {
			delete _lp_filter[i];
		}

		if (_tmp_io_bufs[i]) {
			delete _tmp_io_bufs[i];
		}
	}

	delete [] _instances;
	delete [] _input_ports;
	delete [] _output_ports;

	if (_our_syncin_buf)
		delete [] _our_syncin_buf;
	
	if (_our_syncout_buf)
		delete [] _our_syncout_buf;
	
	if (_dummy_buf)
		delete [] _dummy_buf;

	if (_tmp_io_bufs)
		delete [] _tmp_io_bufs;

	delete [] _lp_filter;

	if (_panner) {
		delete _panner;
	}
	
	// SRC stuff
	delete [] _in_src_states;
	delete [] _out_src_states;

	if (_insync_src_state)
		src_delete (_insync_src_state);
	if (_outsync_src_state)
		src_delete (_outsync_src_state);
	
	if (_src_sync_buffer) 
		delete [] _src_sync_buffer;
	if (_src_in_buffer) 
		delete [] _src_in_buffer;

	// rubberband
	delete _in_stretcher;
	delete _out_stretcher;

	if (_stretch_buffer) {
		delete [] _stretch_buffer;
		_stretch_buffer = 0;
	}
}


void 
Looper::set_use_common_ins (bool val)
{
	_use_common_ins = val;
}

void 
Looper::set_use_common_outs (bool val)
{
	_use_common_outs = val;
}

void
Looper::set_discrete_outs_prefader (bool val)
{
    _discrete_prefader = val;

    if (_have_discrete_io && _discrete_prefader) {
        ports[WetLevel] = 1.0f;
    } else {
        ports[WetLevel] = _target_wet;
    }
}

void
Looper::use_sync_buf(sample_t * buf)
{
	if (buf) {
		_use_sync_buf = buf;
	}
	else {
		_use_sync_buf = _our_syncin_buf;
	}
}

void 
Looper::set_samples_since_sync(nframes_t ssync)
{
	// this is a bit of a hack
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		sl_set_samples_since_sync(_instances[i], ssync);
	}
}

void 
Looper::set_replace_quantized(bool flag)
{
	// this is a bit of a hack
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		sl_set_replace_quantized(_instances[i], flag);
	}
}

void 
Looper::set_soloed (int index, bool value, bool retrigger)
{
	if (index != (int) _index) {
		// someone else is being soloed (or unsoloed), note our mute state, then mute self
		if (value) {
			if (ports[State] != LooperStateMuted || ports[State] != LooperStateOffMuted) {
				_pre_solo_muted = false;
				Event ev;
				ev.Type = Event::type_cmd_hit;
				ev.Command = Event::MUTE_ON;
				do_event(&ev);
			}
			else {
				_pre_solo_muted = true;
			}
			_is_soloed = false;
		}
		else {
			// they are being unsoloed, restore our mute state
			if (!_pre_solo_muted) {
				Event ev;
				ev.Type = Event::type_cmd_hit;
				ev.Command = Event::MUTE_OFF;
				do_event(&ev);
			}
		}
	}
	else {
                // we are the target of the solo
		_is_soloed = value;
			
		if (value && ports[State] == LooperStateMuted) {                     
			// ensure we are not muted if we are soloed
			Event ev;
			ev.Type = Event::type_cmd_hit;
                        if (retrigger) {
                                ev.Command = Event::TRIGGER;
                        }
                        else {
                                ev.Command = Event::MUTE_OFF;
                        }
			do_event(&ev);
		}
	}
}

bool
Looper::finish_state()
{
	Event ev;
	ev.Type = Event::type_cmd_hit;
	ev.Command = Event::UNKNOWN;
	
	switch ((int)ports[State]) {
		case LooperStateRecording:
			ev.Command = Event::RECORD; break;
		case LooperStateOverdubbing:
			ev.Command = Event::OVERDUB; break;
		case LooperStateMultiplying:
			ev.Command = Event::MULTIPLY; break;
		case LooperStateReplacing:
			ev.Command = Event::REPLACE; break;
		case LooperStateSubstitute:
			ev.Command = Event::SUBSTITUTE; break;
		case LooperStateInserting:
			ev.Command = Event::INSERT; break;
		default: break;
	}

	if (ev.Command != Event::UNKNOWN) {
		do_event(&ev);
		/*
		for (unsigned int i=0; i < _chan_count; ++i)
		{
			// run it for 0 frames just to change state
			descriptor->run (_instances[i], 0);
		}
		*/
		return true;
	}
	return false;
}

void
Looper::set_buffer_size (nframes_t bufsize)
{
	if (_buffersize != bufsize) {
		//cerr << "setting buffer size to " << bufsize << endl;
		_buffersize = bufsize;

		if (_use_sync_buf == _our_syncin_buf) {
			_use_sync_buf = 0;
		}

		if (_our_syncin_buf) {
			delete [] _our_syncin_buf;
		}
		
		if (_our_syncout_buf)
			delete [] _our_syncout_buf;

		if (_dummy_buf)
			delete [] _dummy_buf;
	
		for (size_t i=0; i < _chan_count; ++i) {
			if (_tmp_io_bufs[i]) {
				delete [] _tmp_io_bufs[i];
			}

			_tmp_io_bufs[i] = new float[_buffersize];
		}
		
		_our_syncin_buf = new float[_buffersize];
		_our_syncout_buf = new float[_buffersize];
		// big enough for use with resampling too
		_dummy_buf = new float[(nframes_t) ceil (_buffersize * MaxResamplingRate)];
		
		if (_use_sync_buf == 0) {
			_use_sync_buf = _our_syncin_buf;
		}


		if (_src_sync_buffer) 
			delete [] _src_sync_buffer;
		if (_src_in_buffer) 
			delete [] _src_in_buffer;
		_src_buffer_len = (nframes_t) ceil (_buffersize * MaxResamplingRate);
		_src_sync_buffer = new float[_src_buffer_len];
		_src_in_buffer = new float[_src_buffer_len];

		_stretch_buffer = new float[_src_buffer_len * _chan_count];

		// set automatic latency values if appropriate
		recompute_latencies();
	}
}

void Looper::set_disable_latency_compensation(bool val)
{
	if (val != _disable_latency) {
		_disable_latency = val;
		if (val) {
			_last_trigger_latency = ports[TriggerLatency];
			_last_input_latency = ports[InputLatency];
			_last_output_latency = ports[OutputLatency] ;
		} else {
			ports[TriggerLatency] = _last_trigger_latency;
			ports[InputLatency] = _last_input_latency;
			ports[OutputLatency] = _last_output_latency;
		}

		recompute_latencies(); 
	}
}

void
Looper::recompute_latencies()
{
	// auto?
	if (_auto_latency)
	{
		ports[TriggerLatency] = _buffersize; // jitter correction


		ports[InputLatency] = _driver->get_input_port_latency(_input_ports[0]);
		if (_use_common_ins) {
			port_id_t comnport = 0;
			if (_driver->get_engine()->get_common_input (0, comnport)) {
				ports[InputLatency] = _driver->get_input_port_latency(comnport);
			}
		}
	
		ports[OutputLatency] = _driver->get_output_port_latency(_output_ports[0]);
		if (_use_common_outs) {
			port_id_t comnport = 0;
			if (_driver->get_engine()->get_common_output (0, comnport)) {
				ports[OutputLatency] = _driver->get_output_port_latency(comnport);
			}
		}

	}
	
	if (_disable_latency) {
		ports[TriggerLatency] = 0; // should we?
		ports[InputLatency] = 0;
		ports[OutputLatency] = 0;
	}

	// add any latency due to timestretch
	if (_stretch_ratio != 1.0) {
		//ports[OutputLatency] += _out_stretcher->getLatency();
		ports[SyncOffsetSamples] = _out_stretcher->getLatency();
	}
		

	//cerr << "input lat: " << ports[InputLatency] << endl;
	//cerr << "output lat: " << ports[OutputLatency] << endl;
}

bool Looper::has_loop() const
{
	return (_instances && _instances[0] && sl_has_loop(_instances[0]));
}

float
Looper::get_control_value (Event::control_t ctrl)
{
	int index = (int) ctrl;
	float pan_pos;
	
	if (ctrl == Event::DryLevel) {
		//return _curr_dry;
		return _target_dry;
	}
    else if (ctrl == Event::WetLevel) {
        return _target_wet;
    }
	else if (index >= 0 && index < LASTPORT) {
		return ports[index];
	}
	else if (ctrl == Event::OutPeakMeter) 
	{
		return _output_peak;
	}
	else if (ctrl == Event::InPeakMeter) 
	{
		return _input_peak;
	}
	else if (ctrl == Event::InputGain) {
		return _curr_input_gain;
	}
	else if (ctrl == Event::ReplaceQuantized) {
		return sl_get_replace_quantized(_instances[0]) ? 1.0f : 0.0f;
	}
	else if (ctrl == Event::RelativeSync) {
		return _relative_sync;
	}
	else if (ctrl == Event::UseCommonOuts) {
		return _use_common_outs;
	}
	else if (ctrl == Event::UseCommonIns) {
		return _use_common_ins;
	}
	else if (ctrl == Event::HasDiscreteIO) {
		return _have_discrete_io;
	}
    else if (ctrl == Event::DiscretePreFader) {
        return _discrete_prefader;
    }
	else if (ctrl == Event::AutosetLatency) {
		return _auto_latency;
	}
	else if (ctrl == Event::IsSoloed) {
		return _is_soloed ? 1.0f : 0.0f;
	}
	else if (ctrl == Event::StretchRatio) {
		return _stretch_ratio;
	}
	else if (ctrl == Event::PitchShift) {
		return _pitch_shift;
	}
	else if (ctrl == Event::TempoStretch) {
		return _tempo_stretch ? 1.0f: 0.0f;
	}
	// i wish i could do something better for this
	else if (ctrl == Event::PanChannel1) {
		if (_panner && _panner->size() > 0) {
			(*_panner)[0]->get_position (pan_pos);
			return pan_pos;
		}
	}
	else if (ctrl == Event::PanChannel2) {
		if (_panner && _panner->size() > 1) {
			(*_panner)[1]->get_position (pan_pos);
			return pan_pos;
		}
	}
	else if (ctrl == Event::PanChannel3) {
		if (_panner && _panner->size() > 2) {
			(*_panner)[2]->get_position (pan_pos);
			return pan_pos;
		}
	}
	else if (ctrl == Event::PanChannel4) {
		if (_panner && _panner->size() > 3) {
			(*_panner)[3]->get_position (pan_pos);
			return pan_pos;
		}
	}
	else if (ctrl == Event::ChannelCount) {
		return (float) _chan_count;
	}
	
	return 0.0f;
}

void Looper::set_port (ControlPort n, float val)
{
	switch ((int)n)
	{
		case DryLevel:
			_target_dry = val;
			break;
        case WetLevel:
            _target_wet = val;
            if (_have_discrete_io && _discrete_prefader) {
                ports[n] = 1.0f;
            } else {
                ports[n] = val;
            }
            break;
		case RelativeSync:
			_relative_sync = val;
			break;
		case Event::ReplaceQuantized:
			set_replace_quantized(val > 0.0f ? true : false);
			break;
		case TempoInput:
			if (_tempo_stretch && ports[CycleLength] != 0.0f) {
				// new ratio is origtempo/newtempo
				double tempo = (30.0 * ports[EighthPerCycleLoop] / ports[CycleLength]);
				//cerr << "tempo calc: " << tempo << " tempo input: " << val << endl;
				// clamp it if close to the same
				tempo = (abs(tempo-val) < 0.001) ? val: tempo;
				double newratio = min(4.0, max(0.5, tempo / (double) val)); 
				_pending_stretch_ratio = newratio;
				_pending_stretch = true;
			}
			// fallthrough intentional
		default:
			ports[n] = val;
			break;
	}
}


bool 
Looper::is_longpress (int cmd)
{
	if ((int) cmd >= 0 && (int) cmd < (int) Event::LAST_COMMAND) {

		return (_down_stamps[cmd] > 0 && _running_frames > (_down_stamps[cmd] + _longpress_frames));
	}
	return false;
}


void
Looper::do_event (Event *ev)
{
	if (ev->Type == Event::type_cmd_hit) {
		Event::command_t cmd = ev->Command;
		requested_cmd = cmd;
		request_pending = true;
		//fprintf(stderr, "Got HIT cmd: %d\n", cmd);

		// a few special commands have double-tap logic
		if (cmd == Event::RECORD_OR_OVERDUB || cmd == Event::RECORD_OR_OVERDUB_EXCL || cmd == Event::RECORD_OR_OVERDUB_SOLO) {
			if (_down_stamps[cmd] > 0 && _running_frames < (_down_stamps[cmd] + _doubletap_frames))
			{
				// we actually need to undo twice!
				requested_cmd = Event::UNDO_TWICE; 
			}
			_down_stamps[cmd] = _running_frames;
		}
	}
	else if (ev->Type == Event::type_cmd_down)
	{
		Event::command_t cmd = ev->Command;
		if ((int) cmd >= 0 && (int) cmd < (int) Event::LAST_COMMAND) {
			requested_cmd = cmd;
			request_pending = true;

			// fprintf(stderr, "Got DOWN cmd: %d\n", cmd);

			// a few special commands have double-tap logic
			if (cmd == Event::RECORD_OR_OVERDUB || cmd == Event::RECORD_OR_OVERDUB_EXCL || cmd == Event::RECORD_OR_OVERDUB_SOLO) {
				if (_down_stamps[cmd] > 0 && _running_frames < (_down_stamps[cmd] + _doubletap_frames))
				{
					// we actually need to undo twice!
					requested_cmd = Event::UNDO_TWICE; 
				}
			}
			
			_down_stamps[cmd] = _running_frames;
		}
	}
	else if (ev->Type == Event::type_cmd_up || ev->Type == Event::type_cmd_upforce)
	{
		// do if release after long press, but not if already in Play
		
		Event::command_t cmd = ev->Command;
		if ((int) cmd >= 0 && (int) cmd < (int) Event::LAST_COMMAND)
		{

			if (ports[State] != LooperStatePlaying
					|| cmd == Event::REVERSE || cmd == Event::DELAY || cmd == Event::UNDO || cmd == Event::REDO)
			{

				if (ev->Type == Event::type_cmd_upforce) {
					// special case if current state is mult or insert
					// and the cmd is mult or insert and we're not quantized
					// a SUS action here really means an unrounded action
					if (ports[Quantize] == 0.0f
							&& ((ports[State] == LooperStateMultiplying && cmd == Event::MULTIPLY)
								|| (ports[State] == LooperStateInserting && cmd == Event::INSERT)))
					{
						// this really should be handled down in the plugin
						cmd = Event::RECORD;
					}

					//cerr << "force up" << endl;

					requested_cmd = cmd;
					request_pending = true;

				}
				else if (_down_stamps[cmd] > 0 && _running_frames > (_down_stamps[cmd] + _longpress_frames))
				{
					//cerr << "long up" << endl;
					requested_cmd = cmd;
					request_pending = true;

					// long press undo and redo become their -all versions
					if (cmd == Event::UNDO) {
						requested_cmd = Event::UNDO_ALL;
					}
					else if (cmd == Event::REDO) {
						requested_cmd = Event::REDO_ALL;
					}
					else if (cmd == Event::RECORD_OR_OVERDUB || cmd == Event::RECORD_OR_OVERDUB_EXCL || cmd == Event::RECORD_OR_OVERDUB_SOLO || cmd == Event::RECORD_OVERDUB_END_SOLO) {
						// longpress of this turns into undo all for one-button goodness
						requested_cmd = Event::UNDO_ALL;
					}
				}
			}
			//fprintf(stderr, "Got UP cmd: %d  req: %d\n", cmd, requested_cmd);


			_down_stamps[cmd] = 0;
		}
	}
	else if (ev->Type == Event::type_control_change)
	{
		// todo: specially handle TriggerThreshold to work across all channels

		if ((int)ev->Control >= (int)Event::TriggerThreshold && (int)ev->Control < (int) Event::State) {

			if (ev->Control == Event::Rate) {
				// uses
				_src_in_ratio = (double) max (MinResamplingRate, min ((double)ev->Value, MaxResamplingRate));
				_src_out_ratio = (double) 1.0 / max (MinResamplingRate, min ((double) ev->Value, MaxResamplingRate));
				src_set_ratio (_insync_src_state, _src_in_ratio);
				src_set_ratio (_outsync_src_state, _src_out_ratio);

				for (unsigned int i=0; i < _chan_count; ++i)
				{
					src_set_ratio (_in_src_states[i], _src_in_ratio);
					src_set_ratio (_out_src_states[i], _src_out_ratio);

					// set lp cutoff at adjusted SR/2
					_lp_filter[i]->set_cutoff (_src_in_ratio * _lp_filter[i]->get_samplerate() * 0.48);
				}
			}
	
			switch (ev->Control) 
			{
			case Event::DryLevel:
				_target_dry = ev->Value;
				break;
            case Event::WetLevel:
                _target_wet = ev->Value;
                if (_have_discrete_io && _discrete_prefader) {
                    ports[ev->Control] = 1.0f;
                } else {
                    ports[ev->Control] = ev->Value;
                }
                break;
			case  Event::Quantize:
				ev->Value = roundf(ev->Value);
				// passthru is intentional
			default:
				ports[ev->Control] = ev->Value;
				//cerr << "set port " << ev->Control << "  to: " << ev->Value << endl;
				break;
			}
			
		}
		else if (ev->Control == Event::InputGain)
		{
			_targ_input_gain = ev->Value;
		}
		else if (ev->Control == Event::RelativeSync)
		{
			_relative_sync = ev->Value > 0.0f;
		}
		else if (ev->Control == Event::UseCommonIns) 
		{
			_use_common_ins = ev->Value > 0.0f;
		}
		else if (ev->Control == Event::UseCommonOuts) 
		{
			_use_common_outs = ev->Value > 0.0f;
		}
		else if (ev->Control == Event::AutosetLatency) 
		{
			_auto_latency = ev->Value > 0.0f;
			recompute_latencies();
		}
        else if (ev->Control == Event::DiscretePreFader)
        {
            set_discrete_outs_prefader(ev->Value > 0.0f);
        }
		else if (ev->Control == Event::PanChannel1) {
			if (_panner && _panner->size() > 0) {
				(*_panner)[0]->set_position (ev->Value);
			}
		}
		else if (ev->Control == Event::PanChannel2) {
			if (_panner && _panner->size() > 1) {
				(*_panner)[1]->set_position (ev->Value);
			}
		}
		else if (ev->Control == Event::PanChannel3) {
			if (_panner && _panner->size() > 2) {
				(*_panner)[2]->set_position (ev->Value);
			}
		}
		else if (ev->Control == Event::PanChannel4) {
			if (_panner && _panner->size() > 3) {
				(*_panner)[3]->set_position (ev->Value);
			}
		}
		else if (ev->Control == Event::ReplaceQuantized) {
			set_replace_quantized(ev->Value > 0.0f ? true : false);
		}
		else if (ev->Control == Event::PitchShift) {
			_pitch_shift = ev->Value; // in semitones
			_out_stretcher->setPitchScale(pow(2.0, _pitch_shift / 12.0));
		}
		else if (ev->Control == Event::StretchRatio) {
			_pending_stretch_ratio = min(4.0, max(0.25, (double) ev->Value)); 
			_pending_stretch = true;
		}
		else if (ev->Control == Event::TempoStretch) {
			_tempo_stretch = ev->Value > 0.0; 
			if (_tempo_stretch && ports[CycleLength] != 0.0f) {
				double tempo = (30.0 * ports[EighthPerCycleLoop] / ports[CycleLength]);
				//cerr << "tempo calc: " << tempo << endl;
				// clamp it if close to the same
				tempo = (abs(tempo-ports[TempoInput]) < 0.001) ? ports[TempoInput]: tempo;
				_pending_stretch_ratio = min(4.0, max(0.5, (double) tempo / ports[TempoInput])); 
				_pending_stretch = true;
			}
		}

	}

	
	// todo other stuff
}


void
Looper::run (nframes_t offset, nframes_t nframes)
{
	// this is the audio thread
	
	TentativeLockMonitor lm (_loop_lock, __LINE__, __FILE__);

	if (!lm.locked()) {

		// treat as bypassed
		if (_have_discrete_io) {
			for (unsigned int i=0; i < _chan_count; ++i)
			{
				sample_t * inbuf = _driver->get_input_port_buffer (_input_ports[i], _buffersize);
				sample_t * outbuf = _driver->get_output_port_buffer (_output_ports[i], _buffersize);
				if (inbuf && outbuf) {
					for (nframes_t n=0; n < nframes; ++n) {
						outbuf[n] = flush_to_zero (inbuf[n] * _curr_dry * _curr_input_gain);
					}
				}
			}
		}
		
		return;
	}

	_running_frames += nframes;
	
	if (request_pending) {
		
		if (ports[Multi] == requested_cmd) {
			/* defer till next call */
			ports[Multi] = -1;
                        //fprintf(stderr,"Deferred to next run\n");
		} else {
			ports[Multi] = requested_cmd;
			request_pending = false;
                        //fprintf(stderr,"Requested mode: %d\n", requested_cmd);
                         
			if (requested_cmd == Event::RECORD && ports[State] != LooperStateRecording) {
				// record cmd, lets reset stretch and pitch ratios to 1 always
				_pending_stretch_ratio = _stretch_ratio = 1.0;
				_pending_stretch = true;
				_pitch_shift = 0.0;
				_out_stretcher->setPitchScale(pow(2.0, _pitch_shift / 12.0));
			}
		}

	} else if (ports[Multi] >= 0) {
		ports[Multi] = -1;
                //fprintf(stderr,"Reset to -1\n");
		//cerr << "reset to -1\n";
	}

	// deal with any pending stretch ratio change from non-rt context
	if (_pending_stretch) {
		double newratio = _pending_stretch_ratio;
		if (_stretch_ratio == 1.0 && newratio != 1.0)
		{
			_in_stretcher->reset();
			_out_stretcher->reset();
		}
		_stretch_ratio = newratio;
		_in_stretcher->setTimeRatio(1.0/_stretch_ratio);
		_out_stretcher->setTimeRatio(_stretch_ratio);
		_pending_stretch = false;
		recompute_latencies();
	}

	LADSPA_Data oldsync = ports[Sync];
	// ignore sync if we are using our own syncin/outbuf
	if (_use_sync_buf == _our_syncin_buf || _use_sync_buf == _our_syncout_buf) {
		ports[Sync] = 0.0f;
		_slave_sync_port = 1.0f;
	}
	else if (_relative_sync && ports[Sync] > 0.0f) {
		// used for recSync relative mode
		ports[Sync] = 2.0f;
		_slave_sync_port = 2.0;
	}
	else {
		_slave_sync_port = 1.0;
	}

	// do fixed peak meter falloff
	_input_peak = flush_to_zero (f_clamp (DB_CO (CO_DB(_input_peak) - nframes * _falloff_per_sample), 0.0f, 20.0f));
	_output_peak = flush_to_zero (f_clamp (DB_CO (CO_DB(_output_peak) - nframes * _falloff_per_sample), 0.0f, 20.0f));
	
	
	
	run_loops (offset, nframes);
/*
	if (ports[Rate] == 1.0f) {
		run_loops (offset, nframes);
	}
	else {
#ifdef HAVE_SAMPLERATE
		run_loops_resampled (offset, nframes);
#else
		run_loops (offset, nframes);
#endif		
	}
*/	
	ports[Sync] = oldsync;
}


void
Looper::run_loops (nframes_t offset, nframes_t nframes)
{
	//LADSPA_Data * inbuf = 0 , *outbuf = 0, *real_inbuf = 0;
	nframes_t alt_frames = nframes;
	float currdry = _curr_dry;
    float currwet = _curr_wet;
	float curr_ing = _curr_input_gain;
	float ing_delta = flush_to_zero (_targ_input_gain - _curr_input_gain) / max((nframes_t) 1, (nframes - 1));
	float dry_delta = flush_to_zero (_target_dry - _curr_dry) / max((nframes_t) 1, (nframes - 1));
    float wet_delta = flush_to_zero (_target_wet - _curr_wet) / max((nframes_t) 1, (nframes - 1));
	bool  resampled = ports[Rate] != 1.0f;
	bool  stretched = _stretch_ratio != 1.0;
	bool  pitched = _pitch_shift != 0.0;

	if (resampled) {
		_src_data.end_of_input = 0;
		
		// resample input audio and sync using Rate
		_src_data.src_ratio = _src_in_ratio;
		_src_data.input_frames = nframes;
		_src_data.output_frames = (long) ceil (nframes * _src_in_ratio);
		
		// sync input
		_src_data.data_in = _use_sync_buf + offset;
		_src_data.data_out = _src_sync_buffer;
		src_process (_insync_src_state, &_src_data);
		
		alt_frames = _src_data.output_frames_gen;
	}

	// get common outputs
	size_t comnouts = _driver->get_engine()->get_common_output_count();
	sample_t* com_obufs[comnouts];
	for (size_t n=0; n < comnouts; ++n) {
		
		com_obufs[n] = _driver->get_engine()->get_common_output_buffer (n);
		if (com_obufs[n]) {
			com_obufs[n] += offset;
		}
	}

	// input bufs
	sample_t* inbufs[_chan_count];
	sample_t* real_inbufs[_chan_count];
	sample_t* outbufs[_chan_count];
	

	for (unsigned int i=0; i < _chan_count; ++i)
	{
		inbufs[i] = 0;
		real_inbufs[i] = 0;
		outbufs[i] = 0;

		/* (re)connect audio ports */
		if (_have_discrete_io) {
			real_inbufs[i] = (LADSPA_Data*) _driver->get_input_port_buffer (_input_ports[i], _buffersize);
			if (real_inbufs[i]) {
				real_inbufs[i] += offset;
			}
			inbufs[i] = real_inbufs[i];
			
			outbufs[i] = (LADSPA_Data*) _driver->get_output_port_buffer (_output_ports[i], _buffersize);
			if (outbufs[i]) {
				outbufs[i] += offset;
			} else {
				outbufs[i] = _tmp_io_bufs[i];
			}
		}
		else {
			inbufs[i] = 0;
			outbufs[i] = _tmp_io_bufs[i];
			real_inbufs[i] = 0;
		}

		if (_use_common_ins || !_have_discrete_io || !real_inbufs[i]) {
			// mix common input into this buffer
			sample_t * comin = _driver->get_engine()->get_common_input_buffer(i);			
			if (comin)
			{
				comin += offset;
				
				curr_ing = _curr_input_gain;

				if (_have_discrete_io && real_inbufs[i]) {
					for (nframes_t pos=0; pos < nframes; ++pos) {
						curr_ing += ing_delta;
						_tmp_io_bufs[i][pos] = curr_ing * (real_inbufs[i][pos] + comin[pos]);
					}
					inbufs[i] = _tmp_io_bufs[i];
				}
				else {
					for (nframes_t pos=0; pos < nframes; ++pos) {
						curr_ing += ing_delta;
						
						_tmp_io_bufs[i][pos] = curr_ing * (comin[pos]);
					}
					inbufs[i] = _tmp_io_bufs[i];
				}

			}
		}
		else {
			// we have discrete and not using common
			curr_ing = _curr_input_gain;
			for (nframes_t pos=0; pos < nframes; ++pos) {
				curr_ing += ing_delta;

				_tmp_io_bufs[i][pos] = curr_ing * (real_inbufs[i][pos]);
			}
			inbufs[i] = _tmp_io_bufs[i];
		}
		
		// no longer needed
		if (inbufs[i] == 0) continue;
		
		// calculate input peak
		compute_peak (inbufs[i], nframes, _input_peak);

	}

	if (resampled) {
		for (unsigned int i=0; i < _chan_count; ++i)
		{

			// resample input
			_src_data.src_ratio = _src_in_ratio;
			_src_data.input_frames = nframes;
			_src_data.output_frames = (long) ceil (nframes * _src_in_ratio);
			_src_data.data_in = (sample_t *) inbufs[i];
			_src_data.data_out = _src_in_buffer;
			src_process (_in_src_states[i], &_src_data);
			
			alt_frames = _src_data.output_frames_gen;

			descriptor->connect_port (_instances[i], AudioInputPort, (LADSPA_Data*) _src_in_buffer);
			descriptor->connect_port (_instances[i], AudioOutputPort, (LADSPA_Data*) _src_in_buffer);

			if (i == 0) {
				descriptor->connect_port (_instances[i], SyncInputPort, (LADSPA_Data*) _src_sync_buffer);
				descriptor->connect_port (_instances[i], SyncOutputPort, (LADSPA_Data*) _src_sync_buffer);
			} else {
				// all others get the first channel's sync-out as input
				descriptor->connect_port (_instances[i], SyncInputPort, (LADSPA_Data*) _src_sync_buffer);
				descriptor->connect_port (_instances[i], SyncOutputPort, (LADSPA_Data*) _dummy_buf);
			}
			
			/* do it */
			descriptor->run (_instances[i], alt_frames);
			
			// resample output
			_src_data.src_ratio = _src_out_ratio;
			_src_data.input_frames = alt_frames;
			if (_src_out_ratio <= 1.0) {
				_src_data.output_frames = nframes;
			}
			else {
				//_src_data.output_frames = (long) ceil (ceil(nframes * _src_in_ratio) * _src_out_ratio);
				_src_data.output_frames = nframes ;
			}
			_src_data.data_in = _src_in_buffer;
			_src_data.data_out = (sample_t *) outbufs[i];
			src_process (_out_src_states[i], &_src_data);
			
			//if (i==0 && _src_data.input_frames != _src_data.input_frames_used) {
			//	cerr << "3 out sup in: " << _src_data.input_frames << "  used: " << _src_data.input_frames_used << endl;
			//}
			
			if (_src_data.output_frames_gen != (long) nframes) {
				//if (i==0) {
				//	cerr << "4 oframes: " << _src_data.output_frames << "  gen: " << _src_data.output_frames_gen << " nframes: " << nframes<< endl;
				//}
				
				// reread
				long leftover = nframes - _src_data.output_frames_gen;
				_src_data.data_out = _src_data.data_out + _src_data.output_frames_gen;
				_src_data.data_in = _src_data.data_in + _src_data.input_frames_used - 1;
				_src_data.input_frames = alt_frames - _src_data.input_frames_used + 1;
				_src_data.output_frames = leftover;
				src_process (_out_src_states[i], &_src_data);
				
				//if (i==0) {
				//	cerr << "4.5 oframes: " << _src_data.output_frames << "  gen: " << _src_data.output_frames_gen << " leftover: " << leftover << endl;
				//}
			}
			
			//cerr << "out altframes: " << alt_frames << "  output: " <<  _src_data.output_frames << "  gen: " << _src_data.output_frames_gen << endl;
			// lowpass the output if rate < 1
			if (_src_in_ratio < 1.0) {
				// this is just problematic, lets not do it
				//_lp_filter[i]->run_lowpass (_src_data.data_out, nframes);
			}

			
		}
	}
	else if (stretched || pitched) 
	{
#if 0
		nframes_t needSamples = (nframes_t) floor(nframes / _stretch_ratio);
		//cerr << "in samps req: " << sampsReq << "  need: " << needSamples << endl;
		alt_frames = needSamples;
		
		// stretch input
		_in_stretcher->process(inbufs, (size_t) nframes, false);
		size_t avail_samps = _in_stretcher->available();			
		size_t got_samps = _in_stretcher->retrieve(&_src_in_buffer, avail_samps);
		if (got_samps < alt_frames) {
			// clear the remaining
			cerr << "clearing in " << alt_frames - got_samps << "  avail: " << avail_samps << "  got samps: " << got_samps << endl;
			memset(&_src_in_buffer[got_samps], 0, (alt_frames - got_samps) * sizeof(float));
		}
	
#endif

		// resample sync using Rate
		_src_data.end_of_input = 0;
		_src_data.src_ratio = _src_in_ratio;
		_src_data.input_frames = nframes;
		_src_data.output_frames = (long) ceil (nframes * _stretch_ratio);
		_src_data.data_in = _use_sync_buf + offset;
		_src_data.data_out = _src_sync_buffer;
		src_process (_insync_src_state, &_src_data);
		
		//alt_frames = _src_data.output_frames_gen;

                
		// stretch output by running the looper as much as we need
		size_t avail_samps = _out_stretcher->available();
		//nframes_t needSamples = (nframes_t) ceil(nframes * _stretch_ratio);

		while (avail_samps < nframes) {
			size_t sampsReq = _out_stretcher->getSamplesRequired();
			size_t sampsUse = min(sampsReq, (size_t) nframes);

			// run the looper(s)
			for (unsigned int i=0; i < _chan_count; ++i) {
				// zero any input, we're not allowing input while stretching for now
				memset(outbufs[i], 0, sampsUse * sizeof(float));			
				// todo sync buf
				if (i == 0) {
					descriptor->connect_port (_instances[i], SyncInputPort, (LADSPA_Data*) _src_sync_buffer);
					descriptor->connect_port (_instances[i], SyncOutputPort, (LADSPA_Data*) _src_sync_buffer);
				} else {
					// all others get the first channel's sync-out as input
					descriptor->connect_port (_instances[i], SyncInputPort, (LADSPA_Data*) _src_sync_buffer);
					descriptor->connect_port (_instances[i], SyncOutputPort, (LADSPA_Data*) _dummy_buf);
				}
				
				descriptor->connect_port (_instances[i], AudioInputPort, (LADSPA_Data*) outbufs[i]);
				descriptor->connect_port (_instances[i], AudioOutputPort, (LADSPA_Data*) outbufs[i]);
				descriptor->run (_instances[i], sampsUse);
				
			}

			// stretch
			_out_stretcher->process(outbufs, sampsUse, false);
				
			avail_samps = _out_stretcher->available();
		}
		
		_out_stretcher->retrieve(outbufs, nframes);			
		
	}
	else 
	{
		// normal operation

		for (unsigned int i=0; i < _chan_count; ++i)
		{
				
			descriptor->connect_port (_instances[i], AudioInputPort, inbufs[i]);
			descriptor->connect_port (_instances[i], AudioOutputPort, outbufs[i]);
				
			if (i == 0) {
				descriptor->connect_port (_instances[i], SyncInputPort, (LADSPA_Data*) _use_sync_buf + offset);
				descriptor->connect_port (_instances[i], SyncOutputPort, (LADSPA_Data*) _our_syncout_buf + offset);
			} else {
				// all others get the first channel's sync-out as input
				descriptor->connect_port (_instances[i], SyncInputPort, (LADSPA_Data*) _our_syncout_buf + offset);
				descriptor->connect_port (_instances[i], SyncOutputPort, (LADSPA_Data*) _dummy_buf + offset);
			}
				
			/* do it */
			descriptor->run (_instances[i], alt_frames);
		}
	}

		
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		if (_have_discrete_io && real_inbufs[i]) {
			// just mix the dry into the outputs
			currdry = _curr_dry;

			for (nframes_t pos=0; pos < nframes; ++pos) {
				currdry += dry_delta;
				
				outbufs[i][pos] += currdry * real_inbufs[i][pos];
			}
		}

		if (_panner && _use_common_outs) {
            float withgain = (_have_discrete_io && _discrete_prefader) ? _target_wet : 1.0f;
            // mix this output into common outputs
            (*_panner)[i]->distribute (outbufs[i], com_obufs, withgain, nframes);
		}

		// calculate output peak post mixing with dry
		compute_peak (outbufs[i], nframes, _output_peak);
		
		
	}

	if (resampled) {
		// resample out sync
		_src_data.src_ratio = _src_out_ratio;
		_src_data.input_frames = alt_frames;
		_src_data.output_frames = nframes;
		_src_data.data_in =  _src_sync_buffer;
		_src_data.data_out = _our_syncout_buf + offset;
		src_process (_outsync_src_state, &_src_data);
		_curr_dry = flush_to_zero (currdry);
		if (dry_delta <= 0.00003f) {
			_curr_dry = _target_dry;
		}
        _curr_wet = flush_to_zero (currwet);
        if (wet_delta <= 0.00003f) {
            _curr_wet = _target_wet;
        }
	}
	
	_curr_input_gain = flush_to_zero (curr_ing);
	if (ing_delta <= 0.00003f) {
		// force to == target
		_curr_input_gain = _targ_input_gain;
	}

	_curr_dry = flush_to_zero (currdry);
	if (dry_delta <= 0.00003f) {
		// force to == target
		_curr_dry = _target_dry;
	}
    _curr_wet = flush_to_zero (currwet);
    if (wet_delta <= 0.00003f) {
        // force to == target
        _curr_wet = _target_wet;
    }

}


bool
Looper::load_loop (string fname)
{
	bool ret = false;

#ifdef HAVE_SNDFILE
	// this is not called from the audio thread
	// so we take the loop_lock during the whole procedure
	LockMonitor lm (_loop_lock, __LINE__, __FILE__);

	SNDFILE * sfile = 0;
	SF_INFO   sinfo;

	memset (&sinfo, 0, sizeof(SF_INFO));

	if ((sfile = sf_open (fname.c_str(), SFM_READ, &sinfo)) == 0) {
		cerr << "error opening " << fname << endl;
		return false;
	}
	else {
		cerr << "opened " << fname << endl;
	}

	// verify that we have enough free loop space to load it

	
        nframes_t freesamps = (nframes_t) (ports[LoopFreeMemory] * _driver->get_samplerate());

	if (sinfo.frames > freesamps) {
		cerr << "file is too long for available space: file: " << sinfo.frames << "  free: " << freesamps << endl;
		sf_close (sfile);
		return false;
	}


	// make some temporary input buffers
	nframes_t bufsize = 65536;
	sample_t ** inbufs = new float*[_chan_count];
	for (unsigned int i=0; i < _chan_count; ++i) {
		inbufs[i] = new float[bufsize];
	}

	sample_t * dummyout = new float[bufsize];
	
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		/* connect audio ports */
		descriptor->connect_port (_instances[i], AudioInputPort, (LADSPA_Data*) inbufs[i]);
		descriptor->connect_port (_instances[i], AudioOutputPort, (LADSPA_Data*) dummyout);
		descriptor->connect_port (_instances[i], SyncInputPort, (LADSPA_Data*) dummyout);
		descriptor->connect_port (_instances[i], SyncOutputPort, (LADSPA_Data*) dummyout);
	}
	
	// ok, first we need to store some current values
	float old_recthresh = ports[TriggerThreshold];
	float old_syncmode = ports[Sync];
	float old_xfadesamples = ports[FadeSamples];
	float old_state  = ports[State];
	float old_in_latency = ports[InputLatency];
	float old_out_latency = ports[OutputLatency];
	float old_trig_latency = ports[TriggerLatency];
	float old_round_tempo = ports[RoundIntegerTempo];
	float old_quantize = ports[Quantize];

	ports[TriggerThreshold] = 0.0f;
	ports[Sync] = 0.0f;
	ports[FadeSamples] = 0.0f;
	ports[InputLatency] = 0.0f;
	ports[OutputLatency] = 0.0f;
	ports[TriggerLatency] = 0.0f;
	ports[RoundIntegerTempo] = 0.0f;
	ports[Quantize] = (float) QUANT_OFF;
	_slave_sync_port = 0.0f;
	
	// now set it to mute just to make sure we weren't already recording
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		// run it for 0 frames just to change state
		ports[Multi] = Event::MUTE_ON;
		descriptor->run (_instances[i], 0);
		ports[Multi] = Event::RECORD;
		descriptor->run (_instances[i], 0);
	}

	// now start recording and run for sinfo.frames total
	nframes_t nframes = bufsize;
	nframes_t frames_left = sinfo.frames;
	nframes_t filechans = sinfo.channels;
	nframes_t bpos;
	sample_t * databuf;
	sample_t * bigbuf  = new float[bufsize * filechans];
	
	while (frames_left > 0)
	{
		if (nframes > frames_left) {
			nframes = frames_left;
		}

		// fill input buffers
		nframes = sf_readf_float (sfile, bigbuf, nframes);

		// deinterleave
		unsigned int n;
		for (n=0; n < _chan_count && n < filechans; ++n) {
			databuf = inbufs[n];
			bpos = n;
			for (nframes_t m=0; m < nframes; ++m) {
				
				databuf[m] = bigbuf[bpos];
				bpos += filechans;
			}
		}
		for (; n < _chan_count; ++n) {
			// clear leftover channels (maybe we should duplicate last one, we'll see)
			//memset(inbufs[n], 0, sizeof(float) * nframes);

			// duplicate last one
			memcpy (inbufs[n], inbufs[filechans-1], sizeof(float) * nframes);
		}
		
		
		for (unsigned int i=0; i < _chan_count; ++i)
		{
			// run it for nframes
			descriptor->run (_instances[i], nframes);
		}

		

		frames_left -= nframes;
	}
	
	// change state to unknown, then the end record (with mute optionally)
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		// in the case of an empty file, run undo_all
		if (sinfo.frames == 0) {
			ports[Multi] = Event::UNDO_ALL;
			descriptor->run (_instances[i], 0);
			continue;
		}

		ports[Multi] = Event::UNKNOWN;
		descriptor->run (_instances[i], 0);

		if ((int)old_state == LooperStateMuted) {
			ports[Multi] = Event::MUTE_ON;
		}
		else if ((int)old_state == LooperStatePaused || (int)old_state == LooperStateOff) {
			ports[Multi] = Event::PAUSE_ON;
		}
		else {
			ports[Multi] = Event::RECORD;
		}
		descriptor->run (_instances[i], 0);

	}

	ports[TriggerThreshold] = old_recthresh;
	ports[Sync] = old_syncmode;
	ports[FadeSamples] = old_xfadesamples;
	ports[InputLatency] = old_in_latency;
	ports[OutputLatency] = old_out_latency;
	ports[TriggerLatency] = old_trig_latency;
	ports[RoundIntegerTempo] = old_round_tempo;
	ports[Quantize] = old_quantize;
	_slave_sync_port = _relative_sync ? 2.0f: 1.0f;
	
	ret = true;

	sf_close (sfile);

	for (unsigned int i=0; i < _chan_count; ++i) {
		delete [] inbufs[i];
	}
	delete [] inbufs;
	delete [] dummyout;
	delete [] bigbuf;
#endif

	return ret;
}


bool
Looper::save_loop (string fname, LoopFileEvent::FileFormat format)
{
	bool ret = false;
	char tmpname[200];

#ifdef HAVE_SNDFILE
	
	// if empty fname, generate name based on loop # and date
	if (fname.empty()) {
		struct tm * nowtime;
		char tmpdate[200];
		struct timeval tv = {0,0};
		gettimeofday (&tv, NULL);
		tmpdate[0] = '\0';
		nowtime = localtime ((time_t *) &tv.tv_sec);
		strftime (tmpdate, sizeof(tmpdate), "%Y%m%d-%H:%M:%S", nowtime);
		snprintf (tmpname, sizeof(tmpname), "sl_%s_loop%02d.wav", tmpdate, _index);
		
		fname = tmpname;
	}
	
	// this is called from the main work thread which controls
	// the allocation of loops and jack ports
	// thus, our readonly activity to the current loop does not
	// need a lock to operate safely (because we know it will be safe :)

	SNDFILE * sfile = 0;
	SF_INFO   sinfo;

	memset (&sinfo, 0, sizeof(SF_INFO));

	switch(format) {
	case LoopFileEvent::FormatFloat:
		sinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
		break;
	case LoopFileEvent::FormatPCM16:
		sinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
		break;

	default:
		sinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	}
	
	sinfo.channels = _chan_count;
	sinfo.samplerate = _driver->get_samplerate();
	
	if ((sfile = sf_open (fname.c_str(), SFM_WRITE, &sinfo)) == 0) {
		cerr << "error opening " << fname << endl;
		return false;
	}
	else {
		cerr << "opened for write: " << fname << endl;
	}

	// make some temporary buffers
	nframes_t bufsize = 65536;
	sample_t ** outbufs = new float*[_chan_count];
	for (unsigned int i=0; i < _chan_count; ++i) {
		outbufs[i] = new float[bufsize];
	}

	sample_t * bigbuf   = new float[bufsize * _chan_count];

	nframes_t nframes = bufsize;
	nframes_t frames_left = (nframes_t) lrintf(ports[LoopLength] * _driver->get_samplerate());

	nframes_t bpos;
	sample_t * databuf;
	nframes_t looppos = 0;

	
	while (frames_left > 0)
	{
		nframes = bufsize;
		
		if (nframes > frames_left) {
			nframes = frames_left;
		}

		for (unsigned int i=0; i < _chan_count; ++i)
		{
			// run it for nframes
			nframes = sl_read_current_loop_audio (_instances[i], outbufs[i], nframes, looppos);
		}

		if (nframes == 0) {
			// we're done, it shorted us somehow
			//cerr << "shorted" << endl;
			break;
		}
		
		// interleave
		unsigned int n;
		for (n=0; n < _chan_count; ++n) {
			databuf = outbufs[n];
			bpos = n;
			for (nframes_t m=0; m < nframes; ++m) {
				bigbuf[bpos] = databuf[m];
				bpos += _chan_count;
			}
		}

		// write out big buffer
		sf_writef_float (sfile, bigbuf, nframes);
		

		frames_left -= nframes;
		looppos += nframes;
	}

	
	ret = true;

	sf_close (sfile);

	for (unsigned int i=0; i < _chan_count; ++i) {
		delete [] outbufs[i];
	}
	delete [] outbufs;
	delete [] bigbuf;
	
#endif

	return ret;
}


XMLNode&
Looper::get_state () const
{
	CommandMap & cmap = CommandMap::instance();
	LocaleGuard lg ("POSIX");
		
	XMLNode *node = new XMLNode ("Looper");
	char buf[120];

	snprintf(buf, sizeof(buf), "%d", _index);
	node->add_property ("index", buf);

	node->add_property ("name", _name);

	snprintf(buf, sizeof(buf), "%d", _chan_count);
	node->add_property ("channels", buf);

	snprintf(buf, sizeof(buf), "%.10g", _loopsecs);
	node->add_property ("loop_secs", buf);
	
	snprintf(buf, sizeof(buf), "%s", _have_discrete_io ? "yes": "no");
	node->add_property ("discrete_io", buf);

    snprintf(buf, sizeof(buf), "%s", _discrete_prefader ? "yes": "no");
    node->add_property ("discrete_prefader", buf);

	snprintf(buf, sizeof(buf), "%s", _use_common_ins ? "yes": "no");
	node->add_property ("use_common_ins", buf);

	snprintf(buf, sizeof(buf), "%s", _use_common_outs ? "yes": "no");
	node->add_property ("use_common_outs", buf);

	snprintf(buf, sizeof(buf), "%s", _relative_sync ? "yes": "no");
	node->add_property ("relative_sync", buf);

	snprintf(buf, sizeof(buf), "%s", _auto_latency ? "yes": "no");
	node->add_property ("auto_latency", buf);

	snprintf(buf, sizeof(buf), "%s", _tempo_stretch ? "yes": "no");
	node->add_property ("tempo_stretch", buf);
	
	snprintf(buf, sizeof(buf), "%.10g", _stretch_ratio);
	node->add_property ("stretch_ratio", buf);
	
	snprintf(buf, sizeof(buf), "%.10g", _pitch_shift);
	node->add_property ("pitch_shift", buf);

	// panner
	if (_panner) {
		node->add_child_nocopy (_panner->state (true));
	}
	
	XMLNode *controls = new XMLNode ("Controls");
	
	for (int n=0; n < LASTCONTROLPORT; ++n)
	{

		XMLNode *child;
		string ctrlstr = cmap.to_control_str((Event::control_t)n);

		if (ctrlstr == "unknown")
			continue;
		
		snprintf(buf, sizeof(buf), "%s", ctrlstr.c_str());
		child = new XMLNode ("Control");
		child->add_property ("name", buf);

		float val = ports[n];
		if (n == DryLevel) {
			val = _curr_dry;
		}
        else if (n == WetLevel) {
            val = _target_wet;
        }
		
		snprintf(buf, sizeof(buf), "%.20g", val);
		child->add_property ("value", buf);

		controls->add_child_nocopy (*child);
	}
	
	node->add_child_nocopy (*controls);
	
	return *node;
}

int
Looper::set_state (const XMLNode& node)
{
	// assumes everything has already been cleaned up
	
	LocaleGuard lg ("POSIX");
	const XMLProperty* prop;
	XMLNodeConstIterator iter;
	XMLNodeList control_kids;
	CommandMap & cmap = CommandMap::instance();

	if (node.name() != "Looper") {
		cerr << "incorrect XML node passed to IO object: " << node.name() << endl;
		return -1;
	}

	if ((prop = node.property ("index")) != 0) {
		sscanf (prop->value().c_str(), "%u", &_index);
	}

	if ((prop = node.property ("channels")) != 0) {
		sscanf (prop->value().c_str(), "%u", &_chan_count);
	}
	
	if ((prop = node.property ("loop_secs")) != 0) {
		sscanf (prop->value().c_str(), "%g", &_loopsecs);
	}
	
	if ((prop = node.property ("discrete_io")) != 0) {
		_have_discrete_io = (prop->value() == "yes");
	}


	// initialize self
	initialize (_index, _chan_count, _loopsecs, _have_discrete_io);

	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
	}

    if ((prop = node.property ("discrete_prefader")) != 0) {
        _discrete_prefader = (prop->value() == "yes");
    }

	if ((prop = node.property ("use_common_ins")) != 0) {
		_use_common_ins = (prop->value() == "yes");
	}

	if ((prop = node.property ("use_common_outs")) != 0) {
		_use_common_outs = (prop->value() == "yes");
	}

	if ((prop = node.property ("relative_sync")) != 0) {
		_relative_sync = (prop->value() == "yes");
	}

	if ((prop = node.property ("auto_latency")) != 0) {
		_auto_latency = (prop->value() == "yes");
	}

	if ((prop = node.property ("stretch_ratio")) != 0) {
		sscanf (prop->value().c_str(), "%lg", &_stretch_ratio);
		_pending_stretch_ratio = _stretch_ratio;
		_pending_stretch = true;
	}

	if ((prop = node.property ("pitch_shift")) != 0) {
		sscanf (prop->value().c_str(), "%lg", &_pitch_shift);
		_out_stretcher->setPitchScale(pow(2.0, _pitch_shift / 12.0));
	}

	if ((prop = node.property ("tempo_stretch")) != 0) {
		_tempo_stretch = (prop->value() == "yes");
	}


	for (iter = node.children().begin(); iter != node.children().end(); ++iter) {
		if ((*iter)->name() == "Panner") {
			if (_panner) {
				_panner->set_state (**iter);
			}
		}
	}

	
	XMLNode * controls_node = node.find_named_node ("Controls");
	if (!controls_node) {
		cerr << "no controls found" << endl;
		return -1;
	}

	
	control_kids = controls_node->children ("Control");

	
	for (XMLNodeConstIterator niter = control_kids.begin(); niter != control_kids.end(); ++niter)
	{
		XMLNode *child;
		child = (*niter);
		string ctrlstr;
		
		Event::control_t ctrl = Event::Unknown;
		float val = 0.0f;
		
		if ((prop = child->property ("name")) != 0) {
			ctrl = cmap.to_control_t(prop->value());
		}
		
		if ((prop = child->property ("value")) != 0) {
			sscanf (prop->value().c_str(), "%g", &val);
		}

		if (ctrl == Event::DryLevel) {
			_curr_dry = _target_dry = val;
		}
        else if (ctrl == Event::WetLevel) {
            _curr_wet = _target_wet = val;
            if (_have_discrete_io && _discrete_prefader) {
                ports[WetLevel] = 1.0f;
            } else {
                ports[WetLevel] = val;
            }
            cerr << "set " << ctrl << " to " << val << endl;
        }
		else if (ctrl != Event::Unknown) {
			//cerr << "set " << ctrl << " to " << val << endl;
			ports[ctrl] = val;
		}
		
	}

	recompute_latencies();

	// load audio if we should
	if ((prop = node.property ("loop_audio")) != 0) {
		ports[State] = LooperStatePaused; // force this
		if (!load_loop(prop->value())) {
			// use the filename with the path of the session file
			string filename = prop->value().c_str();

			if ((prop = node.property("session_filename")) != 0) {
				//explicitly make a copy as dirname modifies it's input
				const char * sessfilename = prop->value().c_str();
				char * modifiable_copy = (char *)malloc(strlen(sessfilename) + 1);
				strcpy(modifiable_copy, sessfilename);
				char * directory = dirname(modifiable_copy);
				string newfilename = string(directory) + string("/") + filename;

				load_loop(newfilename);
				free (modifiable_copy);
			}
		}
	}

	return 0;
}
