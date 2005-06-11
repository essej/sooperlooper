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


#ifndef __sooperlooper_jack_audio_driver__
#define __sooperlooper_jack_audio_driver__

#include <vector>
#include <string>

#include <jack/jack.h>

#include "audio_driver.hpp"
#include "lockmonitor.hpp"

namespace SooperLooper {

	
class JackAudioDriver
	: public AudioDriver
{
  public:
	JackAudioDriver(std::string client_name="", std::string serv_name="");
	virtual ~JackAudioDriver();

	bool initialize(std::string client_name="");
	bool activate();
	bool deactivate();

	bool  create_input_port (std::string name, port_id_t & portid);
	bool  create_output_port (std::string name, port_id_t & portid);

	bool destroy_input_port (port_id_t portid);
	bool destroy_output_port (port_id_t portid);
	
	sample_t * get_input_port_buffer (port_id_t port, nframes_t nframes);
	sample_t * get_output_port_buffer (port_id_t port, nframes_t nframes);

	unsigned int get_input_port_count () { return _input_ports.size(); }
	unsigned int get_output_port_count () { return _output_ports.size(); }

	bool get_transport_info (TransportInfo &info);

	
  protected:

	int connect_to_jack ();

	int process_callback (jack_nframes_t);
	static int _process_callback (jack_nframes_t, void*);
	static int _xrun_callback (void*);
	static void _shutdown_callback (void*);

	int buffersize_callback (jack_nframes_t);
	static int _buffersize_callback (jack_nframes_t, void*);

	jack_client_t *_jack;

	std::vector<jack_port_t *> _input_ports;
	std::vector<jack_port_t *> _output_ports;

	PBD::Lock  _port_lock;
};

};

#endif
