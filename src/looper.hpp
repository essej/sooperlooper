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

#ifdef HAVE_SAMPLERATE
#include <samplerate.h>
#endif

#include "audio_driver.hpp"
#include "lockmonitor.hpp"
#include "ladspa.h"

#include "plugin.hpp"
#include "event.hpp"
#include "event_nonrt.hpp"

namespace SooperLooper {

class OnePoleFilter;	
	
class Looper 
{
  public:
	Looper (AudioDriver * driver, unsigned int index, unsigned int channel_count=1, float loopsecs=40.0, bool discrete=true);
	~Looper ();
	
	bool operator() () const { return _ok; }
	void run (nframes_t offset, nframes_t nframes);

	void do_event (Event *ev);

	float get_control_value (Event::control_t ctrl);
	
	void set_port (ControlPort n, LADSPA_Data val);

	bool load_loop (std::string fname);
	bool save_loop (std::string fname = "", LoopFileEvent::FileFormat format = LoopFileEvent::FormatFloat);

	void set_buffer_size (nframes_t bufsize);

	float * get_sync_in_buf() { return _our_syncin_buf; }
	float * get_sync_out_buf() { return _our_syncout_buf; }

	void use_sync_buf(float * buf);

	void set_use_common_io (bool val);
	bool get_use_common_io () { return _use_common_io; }

	bool get_have_discrete_io () { return _have_discrete_io; }
	
  protected:

	void run_loops (nframes_t offset, nframes_t nframes);
	void run_loops_resampled (nframes_t offset, nframes_t nframes);

	
	int requested_cmd;
	int last_requested_cmd;
	
	AudioDriver *      _driver;

	port_id_t*       _input_ports;
	port_id_t*       _output_ports;

	unsigned int _index;
	unsigned int _chan_count;
	LADSPA_Handle *      _instances;

	static const LADSPA_Descriptor* descriptor;

	LADSPA_Data        ports[LASTPORT];

	float              _curr_dry;
	float              _target_dry;

	// keeps track of down/up commands for SUS purposes
	nframes_t          _down_stamps[Event::LAST_COMMAND+1];
	nframes_t          _longpress_frames;
	nframes_t          _running_frames;
	
	nframes_t          _buffersize;
	LADSPA_Data        * _our_syncin_buf;
	LADSPA_Data        * _our_syncout_buf;
	LADSPA_Data        * _use_sync_buf;
	LADSPA_Data        * _dummy_buf;

	LADSPA_Data        * _tmp_io_buf;


	LADSPA_Data         _slave_sync_port;
	LADSPA_Data         _slave_dummy_port;

	bool                _use_common_io;
	bool                _have_discrete_io;

	// SRC stuff
#ifdef HAVE_SAMPLERATE
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
#endif

	OnePoleFilter  **     _lp_filter;
	
	bool _ok;
	bool request_pending;

	PBD::NonBlockingLock _loop_lock;
};

};

#endif
