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

#include <string>
#include <iostream>

#include <jack/jack.h>

#include "jack_audio_driver.hpp"
#include "engine.hpp"

using namespace SooperLooper;
using namespace std;

JackAudioDriver::JackAudioDriver(string client_name)
	: AudioDriver(client_name)
{

}

JackAudioDriver::~JackAudioDriver()
{
	if (_jack) {
		jack_deactivate (_jack);
		jack_client_close (_jack);
	}
}


bool
JackAudioDriver::initialize()
{

	if (connect_to_jack ()) {
		cerr << "cannot connect to jack" << endl;
		return false;
	}

	_samplerate = jack_get_sample_rate (_jack);

	if (jack_set_process_callback (_jack, _process_callback, this) != 0) {
		cerr << "cannot set process callback" << endl;
		return false;
	}

	return true;
}

bool
JackAudioDriver::activate()
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

bool
JackAudioDriver::deactivate()
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


static void
our_jack_error(const char * err)
{

}

int
JackAudioDriver::connect_to_jack ()
{
	char namebuf[100];

	jack_set_error_function (our_jack_error);

	_jack = 0;
	
	/* try to become a client of the JACK server */
	if (_client_name.empty()) {
		// find a name predictably
		for (int i=1; i < 10; i++) {
			snprintf(namebuf, sizeof(namebuf)-1, "sooperlooper_%d", i);
			if ((_jack = jack_client_new (namebuf)) != 0) {
				_client_name = namebuf;
				break;
			}
		}
	}
	else {
		// try the passed name, or base a predictable name from it
		if ((_jack = jack_client_new (_client_name.c_str())) == 0) {
			for (int i=1; i < 10; i++) {
				snprintf(namebuf, sizeof(namebuf)-1, "%s_%d", _client_name.c_str(), i);
				if ((_jack = jack_client_new (namebuf)) != 0) {
					_client_name = namebuf;
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
JackAudioDriver::_xrun_callback (void* arg)
{
	cerr << "got xrun" << endl;
}


int
JackAudioDriver::_process_callback (jack_nframes_t nframes, void* arg)
{
	return static_cast<JackAudioDriver*> (arg)->process_callback (nframes);
}


int
JackAudioDriver::process_callback (jack_nframes_t nframes)
{
	if (_engine) {
		_engine->process (nframes);
	}
	return 0;
}


bool
JackAudioDriver::create_input_port (std::string name, port_id_t & portid)
{
	if (!_jack) return false;
	
	jack_port_t * port;
	
	if ((port = jack_port_register (_jack, name.c_str(), JACK_DEFAULT_AUDIO_TYPE,
						   JackPortIsInput, 0)) == 0) {
		
		cerr << "JackAudioDriver: cannot register input port" << endl;
		return false;
	}

	portid = _input_ports.size();
	_input_ports.push_back (port);

	return true;
}

bool
JackAudioDriver::create_output_port (std::string name, port_id_t & portid)
{
	if (!_jack) return false;
	
	jack_port_t * port;
	
	if ((port = jack_port_register (_jack, name.c_str(), JACK_DEFAULT_AUDIO_TYPE,
						   JackPortIsOutput, 0)) == 0) {
		
		cerr << "JackAudioDriver: cannot register input port" << endl;
		return false;
	}

	portid = _output_ports.size();
	_output_ports.push_back (port);

	return true;

}

sample_t *
JackAudioDriver::get_input_port_buffer (port_id_t port, nframes_t nframes)
{
	if (!_jack || port >= _input_ports.size()) return 0;

	return (sample_t*) jack_port_get_buffer (_input_ports[port], nframes);	
}

sample_t *
JackAudioDriver::get_output_port_buffer (port_id_t port, nframes_t nframes)
{
	if (!_jack || port >= _output_ports.size()) return 0;

	return (sample_t*) jack_port_get_buffer (_output_ports[port], nframes);	
}
