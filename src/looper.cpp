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

#include "looper.hpp"

#include <iostream>
#include <cstring>

#include "ladspa.h"

using namespace std;
using namespace SooperLooper;

extern	const LADSPA_Descriptor* ladspa_descriptor (unsigned long);


const LADSPA_Descriptor* Looper::descriptor = 0;


Looper::Looper (AudioDriver * driver, unsigned int index, unsigned int chan_count)
	: _driver (driver), _index(index), _chan_count(chan_count)
{
	char tmpstr[100];
	
	_ok = false;
	requested_cmd = 0;
	last_requested_cmd = 0;
	request_pending = false;
	_input_ports = 0;
	_output_ports = 0;
	_instances = 0;
	
	if (!descriptor) {
		descriptor = ladspa_descriptor (0);
	}


	_instances = new LADSPA_Handle[_chan_count];
	_input_ports = new port_id_t[_chan_count];
	_output_ports = new port_id_t[_chan_count];

	memset (_instances, 0, sizeof(LADSPA_Handle) * _chan_count);
	memset (_input_ports, 0, sizeof(port_id_t) * _chan_count);
	memset (_output_ports, 0, sizeof(port_id_t) * _chan_count);
	memset (ports, 0, sizeof(float) * 18);
	
	// set some rational defaults
	ports[DryLevel] = 1.0f;
	ports[WetLevel] = 1.0f;
	ports[Feedback] = 1.0f;
	ports[Rate] = 1.0f;
	ports[Multi] = -1.0f;

	
	for (unsigned int i=0; i < _chan_count; ++i)
	{

		if ((_instances[i] = descriptor->instantiate (descriptor, _driver->get_samplerate())) == 0) {
			return;
		}

		snprintf(tmpstr, sizeof(tmpstr), "loop%d_in_%d", _index, i+1);

		if (!_driver->create_input_port (tmpstr, _input_ports[i])) {
					
			cerr << "cannot register loop input port\n";
			return;
		}
		
		snprintf(tmpstr, sizeof(tmpstr), "loop%d_out_%d", _index, i+1);

		if (!_driver->create_output_port (tmpstr, _output_ports[i]))
		{
			cerr << "cannot register loop output port\n";
			return;
		}

		/* connect all scalar ports to data values */
		
		for (unsigned long n = 0; n < 18; ++n) {
			descriptor->connect_port (_instances[i], n, &ports[n]);
		}

		descriptor->activate (_instances[i]);
	}

	
	
	_ok = true;
}

Looper::~Looper ()
{
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		if (_instances[i]) {
			if (descriptor->deactivate) {
				descriptor->deactivate (_instances[i]);
			}
			if (descriptor->cleanup) {
				descriptor->cleanup (_instances[i]);
			}
			_instances[i] = 0;
		}

		// TODO
// 		if (_input_ports[i]) {
// 			jack_port_unregister (_jack, _input_ports[i]);
// 			_input_ports[i] = 0;
// 		}
		
// 		if (_output_ports[i]) {
// 			jack_port_unregister (_jack, _output_ports[i]);
// 			_output_ports[i] = 0;
// 		}
	}

	delete [] _instances;
	delete [] _input_ports;
	delete [] _output_ports;
}

float
Looper::get_control_value (Event::control_t ctrl)
{
	int index = (int) ctrl;
	
	if (index >= 0 && index < LASTPORT) {
		return ports[index];
	}

	return 0.0f;
}


void
Looper::request_cmd (int cmd)
{
	requested_cmd = cmd;
	request_pending = true;
}

void
Looper::do_event (Event *ev)
{
	if (ev->Type == Event::type_cmd_down)
	{
		request_cmd ((int) ev->Command);
	}
	else if (ev->Type == Event::type_control_change)
	{
		// todo: specially handle TriggerThreshold to work across all channels
		
		if ((int)ev->Control >= (int)Event::TriggerThreshold && (int)ev->Control <= (int) Event::RedoTap) {
			
			ports[ev->Control] = ev->Value;
			//cerr << "set port " << ev->Control << "  to: " << ev->Value << endl;
		}
	}
	
	// todo other stuff
}


void
Looper::run (nframes_t offset, nframes_t nframes)
{
	/* maybe change modes */

	if (request_pending) {
		
		if (ports[Multi] == requested_cmd) {
			/* defer till next call */
			ports[Multi] = -1;
		} else {
			ports[Multi] = requested_cmd;
			request_pending = false;
			// cerr << "requested mode " << requested_cmd << endl;
		}

	} else if (ports[Multi] >= 0) {
		ports[Multi] = -1;
		//cerr << "reset to -1\n";
	}

	for (unsigned int i=0; i < _chan_count; ++i)
	{
		/* (re)connect audio ports */
		
		descriptor->connect_port (_instances[i], 18, (LADSPA_Data*) _driver->get_input_port_buffer (_input_ports[i], nframes) + offset);
		descriptor->connect_port (_instances[i], 19, (LADSPA_Data*) _driver->get_output_port_buffer (_output_ports[i], nframes) + offset);
		
		/* do it */
		descriptor->run (_instances[i], nframes);

	}
}
