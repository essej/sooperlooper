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

#include "audio_driver.hpp"
#include "lockmonitor.hpp"
#include "ladspa.h"

#include "event.hpp"
#include "event_nonrt.hpp"

namespace SooperLooper {

enum ControlPort {
	TriggerThreshold = 0,
	DryLevel,
	WetLevel,
	Feedback,
	Rate,
	ScratchPosition,
	Multi,
	TapDelayTrigger,
	MultiTens,
	Quantize,
	Round,
	RedoTap,
	Sync,
	UseRate,
	FadeSamples,
};

enum OutputPort {
	State = 15,
	LoopLength,
	LoopPosition,
	CycleLength,
	LoopFreeMemory,
	LoopMemory,
	LASTPORT
};

enum AudioPort {
	AudioInputPort=21,
	AudioOutputPort,
	SyncInputPort,
	SyncOutputPort
};

class Looper 
{
  public:
	Looper (AudioDriver * driver, unsigned int index, unsigned int channel_count=1);
	~Looper ();
	
	bool operator() () const { return _ok; }
	void run (nframes_t offset, nframes_t nframes);

	void do_event (Event *ev);

	float get_control_value (Event::control_t ctrl);
	
	void request_cmd (int cmd);
	
	void set_port (ControlPort n, LADSPA_Data val) {
		ports[n] = val;
	}

	bool load_loop (std::string fname);
	bool save_loop (std::string fname, LoopFileEvent::FileFormat format = LoopFileEvent::FormatFloat);

	void set_buffer_size (nframes_t bufsize);

	float * get_sync_in_buf() { return _our_syncin_buf; }
	float * get_sync_out_buf() { return _our_syncout_buf; }

	void use_sync_buf(float * buf);
	
  protected:

	int requested_cmd;
	int last_requested_cmd;
	
	AudioDriver *      _driver;

	port_id_t*       _input_ports;
	port_id_t*       _output_ports;

	unsigned int _index;
	unsigned int _chan_count;
	LADSPA_Handle *      _instances;

	static const LADSPA_Descriptor* descriptor;

	LADSPA_Data        ports[18];

	nframes_t          _buffersize;
	LADSPA_Data        * _our_syncin_buf;
	LADSPA_Data        * _our_syncout_buf;
	LADSPA_Data        * _use_sync_buf;
	LADSPA_Data        * _dummy_buf;

	LADSPA_Data         _slave_sync_port;
	LADSPA_Data         _slave_dummy_port;
	
	
	bool _ok;
	bool request_pending;

	PBD::NonBlockingLock _loop_lock;
};

};

#endif
