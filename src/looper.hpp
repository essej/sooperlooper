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

#ifndef __sooperlooper_looper__
#define __sooperlooper_looper__

#include <string>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <samplerate.h>



#include "audio_driver.hpp"
#include "lockmonitor.hpp"
#include "ladspa.h"

#include "plugin.hpp"
#include "event.hpp"
#include "event_nonrt.hpp"
#include "utils.hpp"

#include <pbd/xml++.h>

namespace RubberBand {
	class RubberBandStretcher;
}


namespace SooperLooper {

class OnePoleFilter;	
class Panner;

	
class Looper 
{
  public:
	Looper (AudioDriver * driver, unsigned int index, unsigned int channel_count=1, float loopsecs=40.0, bool discrete=true);
	Looper (AudioDriver * driver, XMLNode & node);
	~Looper ();

	bool initialize (unsigned int index, unsigned int channel_count=1, float loopsecs=40.0, bool discrete=true);
	void destroy();
	
	bool operator() () const { return _ok; }
	void run (nframes_t offset, nframes_t nframes);

	void do_event (Event *ev);

	float get_control_value (Event::control_t ctrl);
	
	void set_port (ControlPort n, LADSPA_Data val);

	bool load_loop (std::string fname);
	bool save_loop (std::string fname = "", LoopFileEvent::FileFormat format = LoopFileEvent::FormatFloat);

	void set_buffer_size (nframes_t bufsize);

	sample_t * get_sync_in_buf() { return _our_syncin_buf; }
	sample_t * get_sync_out_buf() { return _our_syncout_buf; }

	void use_sync_buf(sample_t * buf);

	unsigned int get_index() const { return _index; }
	unsigned int get_channel_count() const { return _chan_count; }
	
	void set_use_common_ins (bool val);
	bool get_use_common_ins () const { return _use_common_ins; }
	void set_use_common_outs (bool val);
	bool get_use_common_outs () const { return _use_common_outs; }

    void set_discrete_outs_prefader (bool val);
    bool get_discrete_outs_prefader () const { return _discrete_prefader; }

	bool get_have_discrete_io () const { return _have_discrete_io; }

	void set_auto_latency (bool val) { _auto_latency = val; }
	bool get_auto_latency () const { return _auto_latency; }

	// disables any compensation but maintains the current values
	void set_disable_latency_compensation (bool val);
	bool get_disable_latency_compensation () const { return _disable_latency; }

	void set_samples_since_sync(nframes_t ssync);
	void set_replace_quantized(bool flag);

	// called when some loop instance is being soloed, index says which instance (may not be us)
	void set_soloed (int index, bool value, bool retrigger=false);
	bool is_soloed() const { return _is_soloed; }

	bool is_muted() const { return ports[State] == LooperStateMuted || ports[State] == LooperStateOffMuted; }
	bool has_loop() const ;

	// finishes any active state that may be going (rec, overdub, etc)
	bool finish_state();
	
	bool is_longpress (int command);

	void set_name(const std::string & name) { _name = name; }
	const std::string & get_name() const { return _name; }

	XMLNode& get_state () const;
	int set_state (const XMLNode&);

	void recompute_latencies();
	
  protected:

	void run_loops (nframes_t offset, nframes_t nframes);
	void run_loops_resampled (nframes_t offset, nframes_t nframes);

	static void compute_peak (sample_t *buf, nframes_t nsamples, float& peak) {
		float p = peak;
		
		for (nframes_t n = 0; n < nsamples; ++n) {
			p = f_max (p, fabsf(buf[n]));
		}
		
		peak = p;
	}	
	
		
	int requested_cmd;
	int last_requested_cmd;
	
	AudioDriver *      _driver;

	port_id_t*       _input_ports;
	port_id_t*       _output_ports;

	unsigned int _index;
	unsigned int _chan_count;
	LADSPA_Handle *      _instances;
	float _loopsecs;
	
	LADSPA_Descriptor* descriptor;

	LADSPA_Data        ports[LASTPORT];

	float              _curr_dry;
	float              _target_dry;

	float              _curr_input_gain;
	float              _targ_input_gain;

    float              _curr_wet;
    float              _target_wet;

	bool               _relative_sync;

    bool               _discrete_prefader;

	// keeps track of down/up commands for SUS purposes
	nframes_t          _down_stamps[Event::LAST_COMMAND+1];
	nframes_t          _longpress_frames;
	nframes_t          _doubletap_frames;
	nframes_t          _running_frames;
	
	nframes_t          _buffersize;
	LADSPA_Data        * _our_syncin_buf;
	LADSPA_Data        * _our_syncout_buf;
	LADSPA_Data        * _use_sync_buf;
	LADSPA_Data        * _dummy_buf;

	LADSPA_Data        ** _tmp_io_bufs;

	Panner             * _panner; 

	float              _input_peak;
	float              _output_peak;
	float              _falloff_per_sample;
	
	LADSPA_Data         _slave_sync_port;
	LADSPA_Data         _slave_dummy_port;

	bool                _use_common_ins;
	bool                _use_common_outs;
	bool                _have_discrete_io;
	bool                _auto_latency;
	bool                _disable_latency;
	LADSPA_Data         _last_trigger_latency;
	LADSPA_Data         _last_input_latency;
	LADSPA_Data         _last_output_latency;

	bool                _pre_solo_muted;
	bool                _is_soloed;

	std::string         _name;

	// SRC stuff
	SRC_STATE**            _in_src_states;
	SRC_STATE**            _out_src_states;
	SRC_STATE*            _insync_src_state;
	SRC_STATE*            _outsync_src_state;
	float  *              _src_in_buffer;
	float  *              _src_sync_buffer;
	nframes_t             _src_buffer_len;

	double                _src_in_ratio;
	double                _src_out_ratio;
	SRC_DATA              _src_data;


	OnePoleFilter  **     _lp_filter;

	// rubberband stuff
	RubberBand::RubberBandStretcher * _in_stretcher;
	RubberBand::RubberBandStretcher * _out_stretcher;
	double                             _stretch_ratio;
	double                             _pitch_shift; // in semitones
	float *                            _stretch_buffer;

	bool                               _tempo_stretch;
	volatile bool                      _pending_stretch;
	volatile double                    _pending_stretch_ratio;

	bool _ok;
	volatile bool request_pending;

	PBD::NonBlockingLock _loop_lock;
};

};

#endif
