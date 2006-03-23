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


#ifndef __sooperlooper_audio_driver__
#define __sooperlooper_audio_driver__

#include <vector>
#include <string>

#include <sigc++/sigc++.h>

#include <inttypes.h>

namespace SooperLooper {

typedef float sample_t;
typedef uint32_t nframes_t;
typedef uint32_t port_id_t;
	
	
class Engine;


struct TransportInfo
{
	enum State {
		ROLLING=1,
		STOPPED
	};
       
	nframes_t framepos;
	double bpm;
	State state;
};
	
class AudioDriver
{
  public:
	AudioDriver(std::string client_name="", std::string serv_name="");
	virtual ~AudioDriver();

	virtual bool initialize(std::string client_name="") = 0;
	virtual bool activate() = 0;
	virtual bool deactivate() = 0;

	virtual bool  create_input_port (std::string name, port_id_t & port_id) = 0;
	virtual bool  create_output_port (std::string name, port_id_t & port_id) = 0;

	virtual bool destroy_input_port (port_id_t portid) = 0;
	virtual bool destroy_output_port (port_id_t portid) = 0;
	
	virtual sample_t * get_input_port_buffer (port_id_t port, nframes_t nframes) = 0;
	virtual sample_t * get_output_port_buffer (port_id_t port, nframes_t nframes) = 0;

	virtual unsigned int get_input_port_count () = 0;
	virtual unsigned int get_output_port_count () = 0;

	virtual nframes_t get_input_port_latency (port_id_t portid) = 0;
	virtual nframes_t get_output_port_latency (port_id_t portid) = 0;
	
	virtual std::string get_name() { return _client_name; }

	virtual void set_engine (Engine * engine) { _engine = engine; }
	virtual Engine * get_engine () { return _engine; }

	virtual bool get_transport_info (TransportInfo &info) { return false; }

	
	virtual nframes_t get_samplerate() { return _samplerate; }
	virtual nframes_t get_buffersize() { return _buffersize; }

	SigC::Signal0<void> ConnectionsChanged;
	
  protected:

	std::string _client_name;
	std::string _server_name;

	Engine *    _engine;

	nframes_t _samplerate;
	nframes_t _buffersize;
	
};

};

#endif
