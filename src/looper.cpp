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

#if HAVE_CONFIG_H
#include <config.h>
#endif


#include "looper.hpp"

#include <iostream>
#include <cstring>

#include <sndfile.h>

#include "ladspa.h"

using namespace std;
using namespace SooperLooper;
using namespace PBD;

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

		if (_input_ports[i]) {
			_driver->destroy_input_port (_input_ports[i]);
			_input_ports[i] = 0;
		}
		
		if (_output_ports[i]) {
			_driver->destroy_output_port (_output_ports[i]);
			_output_ports[i] = 0;
		}
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
	TentativeLockMonitor lm (_loop_lock, __LINE__, __FILE__);

	if (!lm.locked()) {

		// treat as bypassed
		for (unsigned int i=0; i < _chan_count; ++i)
		{
			float * inbuf = _driver->get_input_port_buffer (_input_ports[i], nframes);
			float * outbuf = _driver->get_output_port_buffer (_output_ports[i], nframes);
			if (inbuf && outbuf) {
				for (nframes_t n=0; n < nframes; ++n) {
					outbuf[n] = inbuf[n] * ports[DryLevel];
				}
			}
		}
		
		return;
	}
	
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


bool
Looper::load_loop (string fname)
{
	bool ret = false;

#ifdef HAVE_SNDFILE
	// this is not called from the audio thread
	// so we take the loop_lock during the whole procedure
	LockMonitor lm (_loop_lock, __LINE__, __FILE__);

	SNDFILE * sfile = 0;
	SF_INFO   sinfo;

	memset (&sinfo, 0, sizeof(SF_INFO));

	if ((sfile = sf_open (fname.c_str(), SFM_READ, &sinfo)) == 0) {
		cerr << "error opening " << fname << endl;
		return false;
	}
	else {
		cerr << "opened " << fname << endl;
	}

	// verify that we have enough free loop space to load it

	
        nframes_t freesamps = (nframes_t) (ports[LoopFreeMemory] * _driver->get_samplerate());

	if (sinfo.frames > freesamps) {
		cerr << "file is too long for available space: file: " << sinfo.frames << "  free: " << freesamps << endl;
		sf_close (sfile);
		return false;
	}

	// make some temporary input buffers
	nframes_t bufsize = 16384;
	float ** inbufs = new float*[_chan_count];
	for (unsigned int i=0; i < _chan_count; ++i) {
		inbufs[i] = new float[bufsize];
	}

	float * dummyout = new float[bufsize];
	float * bigbuf   = new float[bufsize * _chan_count];
	
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		/* connect audio ports */
		descriptor->connect_port (_instances[i], 18, (LADSPA_Data*) inbufs[i]);
		descriptor->connect_port (_instances[i], 19, (LADSPA_Data*) dummyout);
	}
	
	// ok, first we need to store some current values
	float old_recthresh = ports[TriggerThreshold];

	ports[TriggerThreshold] = 0.0f;

	// now set it to mute just to make sure we weren't already recording
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		// run it for 0 frames just to change state
		ports[Multi] = Event::MUTE;
		descriptor->run (_instances[i], 0);
		ports[Multi] = Event::RECORD;
		descriptor->run (_instances[i], 0);
	}

	// now start recording and run for sinfo.frames total
	nframes_t nframes = bufsize;
	nframes_t frames_left = sinfo.frames;
	nframes_t filechans = sinfo.channels;
	nframes_t bpos;
	float * databuf;
	
	while (frames_left > 0)
	{
		if (nframes > frames_left) {
			nframes = frames_left;
		}



		// fill input buffers
		nframes = sf_readf_float (sfile, bigbuf, nframes);

		// deinterleave
		unsigned int n;
		for (n=0; n < _chan_count && n < filechans; ++n) {
			databuf = inbufs[n];
			bpos = n;
			for (nframes_t m=0; m < nframes; ++m) {
				
				databuf[m] = bigbuf[bpos];
				bpos += filechans;
			}
		}
		for (; n < _chan_count; ++n) {
			// clear leftover channels (maybe we should duplicate last one, we'll see)
			memset(inbufs[n], 0, sizeof(float) * nframes);
		}
		
		
		for (unsigned int i=0; i < _chan_count; ++i)
		{
			// run it for nframes
			descriptor->run (_instances[i], nframes);
		}

		

		frames_left -= nframes;
	}
	

	// change state to unknown, then the end record
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		ports[Multi] = Event::UNKNOWN;
		descriptor->run (_instances[i], 0);
		ports[Multi] = Event::RECORD;
		descriptor->run (_instances[i], 0);
	}

	ports[TriggerThreshold] = old_recthresh;
	
	ret = true;

	sf_close (sfile);

	for (unsigned int i=0; i < _chan_count; ++i) {
		delete [] inbufs[i];
	}
	delete [] inbufs;
	delete [] dummyout;
	delete [] bigbuf;
#endif

	return ret;
}

bool
Looper::save_loop (string fname, LoopFileEvent::FileFormat format)
{
	bool ret = false;
	
#ifdef HAVE_SNDFILE

	// right now, this is a bit of a hack
	// we have to set the plugin(s) to mute state
	// then do a unmute one-shot and run it a total
	// of loop_length frames

	// this is not called from the audio thread
	// so we take the loop_lock during the whole procedure
	LockMonitor lm (_loop_lock, __LINE__, __FILE__);

	SNDFILE * sfile = 0;
	SF_INFO   sinfo;

	memset (&sinfo, 0, sizeof(SF_INFO));

	switch(format) {
	case LoopFileEvent::FormatFloat:
		sinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
		break;
	case LoopFileEvent::FormatPCM16:
		sinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
		break;

	default:
		sinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
	}
	
	sinfo.channels = _chan_count;
	sinfo.samplerate = _driver->get_samplerate();
	
	if ((sfile = sf_open (fname.c_str(), SFM_WRITE, &sinfo)) == 0) {
		cerr << "error opening " << fname << endl;
		return false;
	}
	else {
		cerr << "opened for write: " << fname << endl;
	}

	// make some temporary buffers
	nframes_t bufsize = 16384;
	float ** outbufs = new float*[_chan_count];
	for (unsigned int i=0; i < _chan_count; ++i) {
		outbufs[i] = new float[bufsize];
	}

	float * dummyin = new float[bufsize];
	float * bigbuf   = new float[bufsize * _chan_count];

	memset(dummyin, 0, sizeof(float) * bufsize);
	
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		/* connect audio ports */
		descriptor->connect_port (_instances[i], 18, (LADSPA_Data*) dummyin);
		descriptor->connect_port (_instances[i], 19, (LADSPA_Data*) outbufs[i]);
	}
	
	// ok, first we need to store some current values
	float old_wet = ports[WetLevel];
	float old_dry = ports[DryLevel];

	ports[WetLevel] = 1.0f;
	ports[DryLevel] = 0.0f;

	// now set it to mute then scratch (to start from beginning)
	//   just to make sure we weren't already recording
	
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		// run it for 0 frames just to change state
		ports[Multi] = Event::MUTE;
		descriptor->run (_instances[i], 0);
		ports[Multi] = Event::SCRATCH;
		descriptor->run (_instances[i], 0);
	}

	// now start recording and run for loop length total
	nframes_t nframes = bufsize;
	nframes_t frames_left = (nframes_t) (ports[LoopLength] * _driver->get_samplerate());

	nframes_t bpos;
	float * databuf;
	
	while (frames_left > 0)
	{
		if (nframes > frames_left) {
			nframes = frames_left;
		}


		for (unsigned int i=0; i < _chan_count; ++i)
		{
			// run it for nframes
			descriptor->run (_instances[i], nframes);
		}

		// interleave
		unsigned int n;
		for (n=0; n < _chan_count; ++n) {
			databuf = outbufs[n];
			bpos = n;
			for (nframes_t m=0; m < nframes; ++m) {
				bigbuf[bpos] = databuf[m];
				bpos += _chan_count;
			}
		}

		// write out big buffer
		sf_writef_float (sfile, bigbuf, nframes);
		

		frames_left -= nframes;
	}
	

	ports[DryLevel] = old_dry;
	ports[WetLevel] = old_wet;
	
	// change state to unknown, then the end record
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		ports[Multi] = Event::UNKNOWN;
		descriptor->run (_instances[i], 0);
	}

	
	
	ret = true;

	sf_close (sfile);

	for (unsigned int i=0; i < _chan_count; ++i) {
		delete [] outbufs[i];
	}
	delete [] outbufs;
	delete [] dummyin;
	delete [] bigbuf;
	
#endif

	return ret;
}

