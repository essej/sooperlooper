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
#include <iostream>

#include <jack/jack.h>

#include "engine.hpp"
#include "looper.hpp"
#include "control_osc.hpp"

using namespace SooperLooper;
using namespace std;
using namespace PBD;

#define MAX_EVENTS 1024


Engine::Engine (string jack_name)
	: _jack_name(jack_name)
{
	_ok = false;
	_jack = 0;
	_osc = 0;
}

bool Engine::initialize(int port)
{
	if (connect_to_jack ()) {
		cerr << "cannot connect to jack" << endl;
		return false;
	}

	_samplerate = jack_get_sample_rate (_jack);

	_event_generator = new EventGenerator(_samplerate);
	_event_queue = new RingBuffer<Event> (MAX_EVENTS);

	_osc = new ControlOSC(this, port);

	if (!_osc->is_ok()) {
		return false;
	}

	
	if (jack_set_process_callback (_jack, _process_callback, this) != 0) {
		return false;
	}
	
	
	_ok = true;

	return true;
}

bool Engine::activate()
{
	if (_jack) {
		if (jack_activate (_jack)) {
			cerr << "cannot activate JACK" << endl;
			return false;
		}
		return true;
	}

	return false;
}

bool Engine::deactivate()
{
	if (_jack) {
		if (jack_deactivate (_jack)) {
			cerr << "cannot deactivate JACK" << endl;
			return false;
		}
		return true;
	}

	return false;
}

void
Engine::cleanup()
{
	if (_ok) {
		jack_client_close (_jack);
		
		delete _osc;
		
		delete _event_queue;
		_event_queue = 0;
		
		delete _event_generator;
		_event_generator = 0;

		_ok = false;
	}
}

Engine::~Engine ()
{
	cleanup ();
}

void Engine::quit()
{
	
	_ok = false;
}


bool
Engine::add_loop (unsigned int chans)
{
	int n;
	
	{
		LockMonitor lm (_instance_lock, __LINE__, __FILE__);

		Looper * instance;
		
		instance = new Looper (_jack, (unsigned int) _instances.size(), chans);
		n = _instances.size();
		
		if (!(*instance)()) {
			cerr << "can't create a new loop!\n";
			delete instance;
			return false;
		}
		
		_instances.push_back (instance);
	}
	
	LoopAdded (n); // emit
	
	return true;
}


bool
Engine::remove_loop (unsigned int index)
{
	LockMonitor lm (_instance_lock, __LINE__, __FILE__);

	if (index < _instances.size()) {

		Instances::iterator iter = _instances.begin();
		iter += index;
		
		Looper * loop = (*iter);
		_instances.erase (iter);

		delete loop;
		LoopRemoved(); // emit
	}
}


std::string
Engine::get_osc_url ()
{
	if (_osc && _osc->is_ok()) {
		return _osc->get_server_url();
	}

	return "";
}

int
Engine::get_osc_port ()
{
	if (_osc && _osc->is_ok()) {
		return _osc->get_server_port();
	}

	return 0;
}


static void
our_jack_error(const char * err)
{

}

int
Engine::connect_to_jack ()
{
	char namebuf[100];

	jack_set_error_function (our_jack_error);

	_jack = 0;
	
	/* try to become a client of the JACK server */
	if (_jack_name.empty()) {
		// find a name predictably
		for (int i=1; i < 10; i++) {
			snprintf(namebuf, sizeof(namebuf)-1, "sooperlooper_%d", i);
			if ((_jack = jack_client_new (namebuf)) != 0) {
				_jack_name = namebuf;
				break;
			}
		}
	}
	else {
		// try the passed name, or base a predictable name from it
		if ((_jack = jack_client_new (_jack_name.c_str())) == 0) {
			for (int i=1; i < 10; i++) {
				snprintf(namebuf, sizeof(namebuf)-1, "%s_%d", _jack_name.c_str(), i);
				if ((_jack = jack_client_new (namebuf)) != 0) {
					_jack_name = namebuf;
					break;
				}
			}
		}
	}

	if (!_jack) {
		return -1;
	}

	jack_set_xrun_callback (_jack, _xrun_callback, this);
	
	return 0;
}

int
Engine::_xrun_callback (void* arg)
{
	cerr << "got xrun" << endl;
}


int
Engine::_process_callback (jack_nframes_t nframes, void* arg)
{
	return static_cast<Engine*> (arg)->process_callback (nframes);
}

int
Engine::process_callback (jack_nframes_t nframes)
{
	TentativeLockMonitor lm (_instance_lock, __LINE__, __FILE__);

	Event * evt;
	RingBuffer<Event>::rw_vector vec;
	
	
	// get available events
	_event_queue->get_read_vector (&vec);

	// update event generator
	_event_generator->updateFragmentTime (nframes);
	

	
	if (!lm.locked()) {
		// todo pass silence
		cerr << "already locked!" << endl;
	}
	else
	{
		// process events
		jack_nframes_t usedframes = 0;
		jack_nframes_t doframes;
		size_t num = vec.len[0];
		size_t n = 0;
		size_t totn = 0;
		size_t vecn = 0;
		jack_nframes_t fragpos;
		
		if (num > 0) {
		
			while (n < num)
			{ 
				evt = vec.buf[vecn] + n;
				fragpos = (jack_nframes_t) evt->FragmentPos();

				++n;
				// to avoid code copying
				if (n == num) {
					if (vecn == 0) {
						++vecn;
						n = 0;
						num = vec.len[1];
					}
				}
				
				if (fragpos < usedframes || fragpos >= nframes) {
					// bad fragment pos
					cerr << "BAD FRAGMENT POS: " << fragpos << endl;
					continue;
				}
				
				doframes = fragpos - usedframes;
				int m = 0;
				for (Instances::iterator i = _instances.begin(); i != _instances.end(); ++i, ++m)
				{
					// run for the time before this event
					(*i)->run (usedframes, doframes);
					
					// process event
					if (evt->Instance == -1 || evt->Instance == m) {
						(*i)->do_event (evt);
					}
				}
				
				usedframes += doframes;
			}

			// advance events
			_event_queue->increment_read_ptr (vec.len[0] + vec.len[1]);
			
			// run the rest of the frames
			for (Instances::iterator i = _instances.begin(); i != _instances.end(); ++i) {
				(*i)->run (usedframes, nframes - usedframes);
			}

		}
		else {
			// no events
			for (Instances::iterator i = _instances.begin(); i != _instances.end(); ++i) {
				(*i)->run (0, nframes);
			}

		}

		
	}

	
	return 0;
}


bool
Engine::push_command_event (Event::type_t type, Event::command_t cmd, int8_t instance)
{
	// todo support more than one simulataneous pusher safely
	RingBuffer<Event>::rw_vector vec;

	get_event_queue().get_write_vector (&vec);

	if (vec.len[0] == 0) {
		cerr << "event queue full, dropping event" << endl;
		return false;
	}
	
	Event * evt = vec.buf[0];
	*evt = get_event_generator().createEvent();

	evt->Type = type;
	evt->Command = cmd;
	evt->Instance = instance;

	get_event_queue().increment_write_ptr (1);

	cerr << "posted event" << endl;
	
	return true;
}


bool
Engine::push_control_event (Event::type_t type, Event::control_t ctrl, float val, int8_t instance)
{
	// todo support more than one simulataneous pusher safely
	
	RingBuffer<Event>::rw_vector vec;

	get_event_queue().get_write_vector (&vec);

	if (vec.len[0] == 0) {
		cerr << "event queue full, dropping event" << endl;
		return false;
	}
	
	Event * evt = vec.buf[0];
	*evt = get_event_generator().createEvent();

	evt->Type = type;
	evt->Control = ctrl;
	evt->Value = val;
	evt->Instance = instance;

	get_event_queue().increment_write_ptr (1);

	return true;
}


float
Engine::get_control_value (Event::control_t ctrl, int8_t instance)
{
	// this should *really* be mutexed
	// it is a race waiting to happen
	
	if (instance >= 0 && instance < _instances.size()) {

		return _instances[instance]->get_control_value (ctrl);
	}

	return 0.0f;
}
