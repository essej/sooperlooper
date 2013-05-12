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

#include <jack/jack.h>
#include "ladspa.h"
#include "../plugin.hpp"
typedef uint32_t nframes_t;

namespace SooperLooper {

class TestLooper 
{
  public:
	TestLooper (jack_client_t*, unsigned int index, unsigned int channel_count=1);
	~TestLooper ();
	
	void run (jack_nframes_t offset, jack_nframes_t nframes);

	float get_control_value (int ctrl);
	
	void request_cmd (int cmd);
	
	void set_port (ControlPort n, LADSPA_Data val) {
		ports[n] = val;
	}

	void set_buffer_size (nframes_t bufsize);
	
	int requested_cmd;
	int last_requested_cmd;

	bool ok;
	bool request_pending;

  protected:
	
	jack_client_t*     _jack;
	jack_port_t**       _input_ports;
	jack_port_t**       _output_ports;

	unsigned int _index;
	unsigned int _chan_count;
	LADSPA_Handle *      _instances;

	static const LADSPA_Descriptor* descriptor;

	LADSPA_Data        ports[LASTPORT];

	nframes_t          _buffersize;
	LADSPA_Data        * _our_syncin_buf;
	LADSPA_Data        * _our_syncout_buf;
	LADSPA_Data        * _use_sync_buf;
	LADSPA_Data        * _dummy_buf;

	LADSPA_Data         _slave_sync_port;
	LADSPA_Data         _slave_dummy_port;
	
};

};

#endif
