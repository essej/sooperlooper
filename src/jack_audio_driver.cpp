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
using namespace PBD;
using namespace std;

JackAudioDriver::JackAudioDriver(string client_name, string serv_name)
	: AudioDriver(client_name, serv_name)
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
JackAudioDriver::initialize(string client_name)
{
	if (!client_name.empty()) {
		_client_name = client_name;
	}
	
	if (connect_to_jack ()) {
		cerr << "cannot connect to jack" << endl;
		return false;
	}

	_samplerate = jack_get_sample_rate (_jack);
	_buffersize = jack_get_buffer_size (_jack);
	
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
	jack_set_error_function (our_jack_error);

	_jack = 0;

#ifdef HAVE_JACK_CLIENT_OPEN
	jack_options_t options = JackNullOption;
	jack_status_t status;
	const char *server_name = NULL;

	// support for server name
	if (!_server_name.empty()) {
		server_name = _server_name.c_str();
	}
	
	if (_client_name.empty()) {
		_client_name = "sooperlooper";
	}
	
	_jack = jack_client_open (_client_name.c_str(), options, &status, server_name);
	
	if (!_jack) {
		return -1;
	}
	
	if (status & JackServerStarted) {
		cerr << "JACK server started" << endl;
	}

	if (status & JackNameNotUnique) {
		_client_name = jack_get_client_name (_jack);
	}

#else
	char namebuf[100];
	
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
#endif
	
	jack_set_xrun_callback (_jack, _xrun_callback, this);
	jack_on_shutdown (_jack, _shutdown_callback, this);
	
	return 0;
}

int
JackAudioDriver::_xrun_callback (void* arg)
{
	cerr << "got xrun" << endl;
	return 0;
}

void
JackAudioDriver::_shutdown_callback (void* arg)
{
	cerr << "jack shut us down" << endl;
	return;
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

	{
		//LockMonitor mon(_port_lock, __LINE__, __FILE__);
		_input_ports.push_back (port);
		portid = _input_ports.size();
	}
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

	{
		//LockMonitor mon(_port_lock, __LINE__, __FILE__);
		_output_ports.push_back (port);
		portid = _output_ports.size();
	}
	
	return true;

}

bool
JackAudioDriver::destroy_output_port (port_id_t portid)
{
	jack_port_t * port = 0;

	//LockMonitor mon(_port_lock, __LINE__, __FILE__);
	
	if (portid <= _output_ports.size() && portid > 0) {
		port = _output_ports[portid-1];
		_output_ports[portid-1] = 0;
		return (jack_port_unregister (_jack, port) == 0);
	}
	
	return false;
}

bool
JackAudioDriver::destroy_input_port (port_id_t portid)
{
	jack_port_t * port = 0;

	//LockMonitor mon(_port_lock, __LINE__, __FILE__);
	if (portid <= _input_ports.size() && portid > 0) {
		port = _input_ports[portid-1];
		_input_ports[portid-1] = 0;
		return (jack_port_unregister (_jack, port) == 0);
	}
	
	return false;
}


sample_t *
JackAudioDriver::get_input_port_buffer (port_id_t port, nframes_t nframes)
{
	// not locked 
	if (!_jack || port > _input_ports.size() || port == 0) return 0;

	return (sample_t*) jack_port_get_buffer (_input_ports[port-1], nframes);	
}

sample_t *
JackAudioDriver::get_output_port_buffer (port_id_t port, nframes_t nframes)
{
	// not locked 
	if (!_jack || port > _output_ports.size() || port == 0) return 0;

	return (sample_t*) jack_port_get_buffer (_output_ports[port-1], nframes);	
}


bool
JackAudioDriver::get_transport_info (TransportInfo &info)
{
	if (!_jack) return false;
	
	jack_position_t tpos;
	jack_transport_state_t tstate;
	
	tstate = jack_transport_query( _jack, &tpos);

	switch ( tstate ) {
	case JackTransportStopped:
		info.state = TransportInfo::STOPPED;
		break;
	case JackTransportRolling:
		info.state = TransportInfo::ROLLING;
		break;
	case JackTransportStarting:
		info.state = TransportInfo::STOPPED;
		break;
	default:
		break;
		//errorLog( "[updateTransportInfo] Unknown jack transport state" );
	}


	if ( tpos.valid & JackPositionBBT ) {
		info.bpm = tpos.beats_per_minute;
	}

	info.framepos = tpos.frame;

	return true;
}
