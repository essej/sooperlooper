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

#ifndef __sooperlooper_engine__
#define __sooperlooper_engine__

#include <vector>
#include <string>

#include <jack/jack.h>
#include <sigc++/sigc++.h>

#include "lockmonitor.hpp"
#include "ringbuffer.hpp"
#include "event.hpp"

namespace SooperLooper {

class Looper;
class ControlOSC;
	
class Engine
{
  public:
	
	Engine(std::string jack_name="");
	virtual ~Engine();

	bool initialize(int port=9351);

	bool activate();
	bool deactivate();

	std::string get_name() { return _jack_name; }
	
	bool is_ok() const { return _ok; }

	void quit();

	bool add_loop (unsigned int chans);
	bool remove_loop (unsigned int index);
	
	unsigned int loop_count() { PBD::LockMonitor lm(_instance_lock, __LINE__, __FILE__); return _instances.size(); }

	RingBuffer<Event> & get_event_queue() { return *_event_queue; }

	EventGenerator & get_event_generator() { return *_event_generator;}

	bool push_command_event (Event::type_t type, Event::command_t cmd, int8_t instance);
	bool push_control_event (Event::type_t type, Event::control_t ctrl, float val, int8_t instance);
	
	std::string get_osc_url ();
	int get_osc_port ();

	float get_control_value (Event::control_t, int8_t instance);
	
	SigC::Signal1<void, int> LoopAdded;
	SigC::Signal0<void> LoopRemoved;

	
  protected:	

	void cleanup();
	
	int connect_to_jack ();

	int process_callback (jack_nframes_t);
	static int _process_callback (jack_nframes_t, void*);
	static int _xrun_callback (void*);

	jack_client_t *_jack;
	std::string _jack_name;

	ControlOSC * _osc;
	
	typedef std::vector<Looper*> Instances;
	Instances _instances;
	PBD::NonBlockingLock _instance_lock;

	volatile bool _ok;

	RingBuffer<Event> * _event_queue;

	EventGenerator * _event_generator;

	jack_nframes_t _samplerate;
};

};  // sooperlooper namespace

#endif
