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
#include <cmath>

#ifdef HAVE_SNDFILE
#include <sndfile.h>
#endif

#include "ladspa.h"
#include "plugin.hpp"

using namespace std;
using namespace SooperLooper;
using namespace PBD;

extern	const LADSPA_Descriptor* ladspa_descriptor (unsigned long);


const LADSPA_Descriptor* Looper::descriptor = 0;

static const double MinResamplingRate = 0.25f;
static const double MaxResamplingRate = 4.0f;
#ifdef HAVE_SAMPLERATE
static const int SrcAudioQuality = SRC_LINEAR;
#endif

Looper::Looper (AudioDriver * driver, unsigned int index, unsigned int chan_count, float loopsecs)
	: _driver (driver), _index(index), _chan_count(chan_count)
{
	char tmpstr[100];
	int dummyerror;
	
	_ok = false;
	requested_cmd = 0;
	last_requested_cmd = 0;
	request_pending = false;
	_input_ports = 0;
	_output_ports = 0;
	_instances = 0;
	_buffersize = 0;
	_use_sync_buf = 0;
	_our_syncin_buf = 0;
	_our_syncout_buf = 0;
	_dummy_buf = 0;
	_running_frames = 0;

	if (!descriptor) {
		descriptor = ladspa_descriptor (0);
	}


	_instances = new LADSPA_Handle[_chan_count];
	_input_ports = new port_id_t[_chan_count];
	_output_ports = new port_id_t[_chan_count];

	// SRC stuff
#ifdef HAVE_SAMPLERATE
	_in_src_states = new SRC_STATE*[_chan_count];
	_out_src_states = new SRC_STATE*[_chan_count];
	memset (_in_src_states, 0, sizeof(SRC_STATE*) * _chan_count);
	memset (_out_src_states, 0, sizeof(SRC_STATE*) * _chan_count);

	_insync_src_state = src_new (SRC_ZERO_ORDER_HOLD, 1, &dummyerror);
	_outsync_src_state = src_new (SRC_ZERO_ORDER_HOLD, 1, &dummyerror);

	_src_sync_buffer = 0;
	_src_in_buffer = 0;
	_src_buffer_len = 0;
	_src_in_ratio = 1.0;
	_src_out_ratio = 1.0;
#endif
	
	
	set_buffer_size(_driver->get_buffersize());
	
	memset (_instances, 0, sizeof(LADSPA_Handle) * _chan_count);
	memset (_input_ports, 0, sizeof(port_id_t) * _chan_count);
	memset (_output_ports, 0, sizeof(port_id_t) * _chan_count);
	memset (ports, 0, sizeof(float) * LASTPORT);

	memset(_down_stamps, 0, sizeof(nframes_t) * (Event::LAST_COMMAND+1));

	_longpress_frames = (nframes_t) lrint (_driver->get_samplerate() * 2.0); // more than 2 secs is SUS
	
	// set some rational defaults
	ports[DryLevel] = 1.0f;
	ports[WetLevel] = 1.0f;
	ports[Feedback] = 1.0f;
	ports[Rate] = 1.0f;
	ports[Multi] = -1.0f;
	ports[Sync] = 0.0f;
	ports[Quantize] = 0.0f;
	ports[UseRate] = 0.0f;
	ports[FadeSamples] = nearbyint(_driver->get_samplerate() * 0.002f); // 2ms
	ports[PlaybackSync] = 0.0f;
	
	_slave_sync_port = 1.0f;

	// TODO: fix hack to specify loop length
	char looptimestr[20];
	snprintf(looptimestr, sizeof(looptimestr), "%f", loopsecs);
	setenv("SL_SAMPLE_TIME", looptimestr, 1);

	
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
		
		for (unsigned long n = 0; n < LASTPORT; ++n) {
			descriptor->connect_port (_instances[i], n, &ports[n]);
		}

		// connect dedicated Sync port to all other channels
		if (i > 0) {
			descriptor->connect_port (_instances[i], Sync, &_slave_sync_port);

			descriptor->connect_port (_instances[i], State, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], LoopLength, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], LoopPosition, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], CycleLength, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], LoopFreeMemory, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], LoopMemory, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], Waiting, &_slave_dummy_port);
			descriptor->connect_port (_instances[i], TrueRate, &_slave_dummy_port);
		}
		
		descriptor->activate (_instances[i]);

#ifdef HAVE_SAMPLERATE
		// SRC stuff
		_in_src_states[i] = src_new (SrcAudioQuality, 1, &dummyerror);
		_out_src_states[i] = src_new (SrcAudioQuality, 1, &dummyerror);
#endif		
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

#ifdef HAVE_SAMPLERATE
		if (_out_src_states[i]) {
			src_delete (_out_src_states[i]);
		}
		if (_in_src_states[i]) {
			src_delete (_in_src_states[i]);
		}
#endif
	}

	delete [] _instances;
	delete [] _input_ports;
	delete [] _output_ports;

	if (_our_syncin_buf)
		delete [] _our_syncin_buf;
	
	if (_our_syncout_buf)
		delete [] _our_syncout_buf;
	
	if (_dummy_buf)
		delete [] _dummy_buf;

#ifdef HAVE_SAMPLERATE
	delete [] _in_src_states;
	delete [] _out_src_states;

	if (_insync_src_state)
		src_delete (_insync_src_state);
	if (_outsync_src_state)
		src_delete (_outsync_src_state);
#endif
}


void
Looper::use_sync_buf(float * buf)
{
	if (buf) {
		_use_sync_buf = buf;
	}
	else {
		_use_sync_buf = _our_syncin_buf;
	}
}

void
Looper::set_buffer_size (nframes_t bufsize)
{
	if (_buffersize != bufsize) {
		//cerr << "setting buffer size to " << bufsize << endl;
		if (_use_sync_buf == _our_syncin_buf) {
			_use_sync_buf = 0;
		}

		if (_our_syncin_buf)
			delete [] _our_syncin_buf;
		
		if (_our_syncout_buf)
			delete [] _our_syncout_buf;

		if (_dummy_buf)
			delete [] _dummy_buf;
		
		_buffersize = bufsize;
		
		_our_syncin_buf = new float[_buffersize];
		_our_syncout_buf = new float[_buffersize];
		// big enough for use with resampling too
		_dummy_buf = new float[(nframes_t) ceil (_buffersize * MaxResamplingRate)];
		
		if (_use_sync_buf == 0) {
			_use_sync_buf = _our_syncin_buf;
		}

#ifdef HAVE_SAMPLERATE
		if (_src_sync_buffer) 
			delete [] _src_sync_buffer;
		if (_src_in_buffer) 
			delete [] _src_in_buffer;
		_src_buffer_len = (nframes_t) ceil (_buffersize * MaxResamplingRate);
		_src_sync_buffer = new float[_src_buffer_len];
		_src_in_buffer = new float[_src_buffer_len];
#endif
		
	}
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
Looper::do_event (Event *ev)
{
	if (ev->Type == Event::type_cmd_hit) {
		requested_cmd = ev->Command;
		request_pending = true;
	}
	else if (ev->Type == Event::type_cmd_down)
	{
		Event::command_t cmd = ev->Command;
		if ((int) cmd >= 0 && (int) cmd < (int) Event::LAST_COMMAND) {
			requested_cmd = cmd;
			request_pending = true;

			_down_stamps[cmd] = _running_frames;
		}
	}
	else if (ev->Type == Event::type_cmd_up || ev->Type == Event::type_cmd_upforce)
	{
		// do if release after long press, but not if already in Play
		
		Event::command_t cmd = ev->Command;
		if ((int) cmd >= 0 && (int) cmd < (int) Event::LAST_COMMAND
		    && ports[State] != LooperStatePlaying
		    && (ev->Type == Event::type_cmd_upforce
			|| (_running_frames > (_down_stamps[cmd] + _longpress_frames))))
		{
			//cerr << "long up or force" << endl;
			requested_cmd = cmd;
			request_pending = true;

			_down_stamps[cmd] = 0;
		}
	}
	else if (ev->Type == Event::type_control_change)
	{
		// todo: specially handle TriggerThreshold to work across all channels

		if ((int)ev->Control >= (int)Event::TriggerThreshold && (int)ev->Control <= (int) Event::FadeSamples) {

			if (ev->Control == Event::Quantize) {
				ev->Value = roundf(ev->Value);
			}
			
			ports[ev->Control] = ev->Value;
			//cerr << "set port " << ev->Control << "  to: " << ev->Value << endl;

			
#ifdef HAVE_SAMPLERATE
			if (ev->Control == Event::Rate) {
				// uses
				_src_in_ratio = (double) max (MinResamplingRate, min ((double)ev->Value, MaxResamplingRate));
				_src_out_ratio = (double) 1.0 / max (MinResamplingRate, min ((double) ev->Value, MaxResamplingRate));
				src_set_ratio (_insync_src_state, _src_in_ratio);
				src_set_ratio (_outsync_src_state, _src_out_ratio);

				for (unsigned int i=0; i < _chan_count; ++i)
				{
					src_set_ratio (_in_src_states[i], _src_in_ratio);
					src_set_ratio (_out_src_states[i], _src_out_ratio);
				}
			}
#endif
		}
	}
	
	// todo other stuff
}


void
Looper::run (nframes_t offset, nframes_t nframes)
{
	// this is the audio thread
	
	TentativeLockMonitor lm (_loop_lock, __LINE__, __FILE__);

	if (!lm.locked()) {

		// treat as bypassed
		for (unsigned int i=0; i < _chan_count; ++i)
		{
			float * inbuf = _driver->get_input_port_buffer (_input_ports[i], _buffersize);
			float * outbuf = _driver->get_output_port_buffer (_output_ports[i], _buffersize);
			if (inbuf && outbuf) {
				for (nframes_t n=0; n < nframes; ++n) {
					outbuf[n] = inbuf[n] * ports[DryLevel];
				}
			}
		}
		
		return;
	}

	_running_frames += nframes;
	
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

	LADSPA_Data oldsync = ports[Sync];
	// ignore sync if we are using our own syncin/outbuf
	if (_use_sync_buf == _our_syncin_buf || _use_sync_buf == _our_syncout_buf) {
		ports[Sync] = 0.0f;
	}

	if (ports[Rate] == 1.0f) {
		run_loops (offset, nframes);
	}
	else {
#ifdef HAVE_SAMPLERATE
		run_loops_resampled (offset, nframes);
#else
		run_loops (offset, nframes);
#endif		
	}
	
	ports[Sync] = oldsync;
}


void
Looper::run_loops (nframes_t offset, nframes_t nframes)
{
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		/* (re)connect audio ports */
		
		descriptor->connect_port (_instances[i], AudioInputPort, (LADSPA_Data*) _driver->get_input_port_buffer (_input_ports[i], _buffersize) + offset);
		descriptor->connect_port (_instances[i], AudioOutputPort, (LADSPA_Data*) _driver->get_output_port_buffer (_output_ports[i], _buffersize) + offset);


		if (i == 0) {
			descriptor->connect_port (_instances[i], SyncInputPort, (LADSPA_Data*) _use_sync_buf + offset);
			descriptor->connect_port (_instances[i], SyncOutputPort, (LADSPA_Data*) _our_syncout_buf + offset);
		}
		else {
			// all others get the first channel's sync-out as input
			descriptor->connect_port (_instances[i], SyncInputPort, (LADSPA_Data*) _our_syncout_buf + offset);
			descriptor->connect_port (_instances[i], SyncOutputPort, (LADSPA_Data*) _dummy_buf + offset);
		}
		
		
		/* do it */
		descriptor->run (_instances[i], nframes);

	}


}

void
Looper::run_loops_resampled (nframes_t offset, nframes_t nframes)
{
#ifdef HAVE_SAMPLERATE
	nframes_t alt_frames;
	
	_src_data.end_of_input = 0;
	
	// resample input audio and sync using Rate
	_src_data.src_ratio = _src_in_ratio;
	_src_data.input_frames = nframes;
	_src_data.output_frames = (long) ceil (nframes * _src_in_ratio);
	
	// sync input
	_src_data.data_in = _use_sync_buf + offset;
	_src_data.data_out = _src_sync_buffer;
	src_process (_insync_src_state, &_src_data);

	alt_frames = _src_data.output_frames_gen;
	// cerr << "nframes: " << nframes << "  output: " <<  _src_data.output_frames << "  gen: " << _src_data.output_frames_gen << endl;
	
	// process
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		// resample input
		_src_data.src_ratio = _src_in_ratio;
		_src_data.input_frames = nframes;
		_src_data.output_frames = (long) ceil (nframes * _src_in_ratio) + 1;
		_src_data.data_in = (float *) _driver->get_input_port_buffer (_input_ports[i], _buffersize) + offset;
		_src_data.data_out = _src_in_buffer;
		src_process (_in_src_states[i], &_src_data);
		// cerr << "nframes: " << nframes << "  output: " <<  _src_data.output_frames << "  gen: " << _src_data.output_frames_gen << endl;

		alt_frames = _src_data.output_frames_gen;

// 		if (i==0 && _src_data.output_frames != alt_frames) {
// 			//cerr << "1 ---- sup out: " << _src_data.output_frames << "  gen: " << alt_frames << endl;
// 		}
// 		if (i==0 && _src_data.input_frames != _src_data.input_frames_used) {
// 			cerr << "2 sup in: " << _src_data.input_frames << "  used: " << _src_data.input_frames_used << endl;
// 		}
		
		/* (re)connect audio ports */
		
		descriptor->connect_port (_instances[i], AudioInputPort, (LADSPA_Data*) _src_in_buffer);
		descriptor->connect_port (_instances[i], AudioOutputPort, (LADSPA_Data*) _src_in_buffer);


		if (i == 0) {
			descriptor->connect_port (_instances[i], SyncInputPort, (LADSPA_Data*) _src_sync_buffer);
			descriptor->connect_port (_instances[i], SyncOutputPort, (LADSPA_Data*) _src_sync_buffer);
		}
		else {
			// all others get the first channel's sync-out as input
			descriptor->connect_port (_instances[i], SyncInputPort, (LADSPA_Data*) _src_sync_buffer);
			descriptor->connect_port (_instances[i], SyncOutputPort, (LADSPA_Data*) _dummy_buf);
		}
		
		
		/* do it */
		descriptor->run (_instances[i], alt_frames);


		// resample output
		_src_data.src_ratio = _src_out_ratio;
		_src_data.input_frames = alt_frames;
		if (_src_out_ratio <= 1.0) {
			_src_data.output_frames = nframes;
		}
		else {
			//_src_data.output_frames = (long) ceil (ceil(nframes * _src_in_ratio) * _src_out_ratio);
			_src_data.output_frames = nframes;
		}
		_src_data.data_in = _src_in_buffer;
		_src_data.data_out = (float *) _driver->get_output_port_buffer (_output_ports[i], _buffersize) + offset;
		src_process (_out_src_states[i], &_src_data);

// 		if (i==0 && _src_data.input_frames != _src_data.input_frames_used) {
// 			cerr << "3 out sup in: " << _src_data.input_frames << "  used: " << _src_data.input_frames_used << endl;
// 		}
		
		if (_src_data.output_frames_gen < (long) nframes) {
			//cerr << "4 oframes: " << _src_data.output_frames << "  gen: " << _src_data.output_frames_gen << " nframes: " << nframes<< endl;
			// reread
			long leftover = nframes - _src_data.output_frames_gen;
			_src_data.data_out = _src_data.data_out + _src_data.output_frames_gen;
			_src_data.data_in = _src_data.data_in + _src_data.input_frames_used - 1;
			_src_data.input_frames = 1;
			_src_data.output_frames = leftover;
			src_process (_out_src_states[i], &_src_data);

			//cerr << "4.5 oframes: " << _src_data.output_frames << "  gen: " << _src_data.output_frames_gen << " leftover: " << leftover << endl;
			
		}
		
		//cerr << "out altframes: " << alt_frames << "  output: " <<  _src_data.output_frames << "  gen: " << _src_data.output_frames_gen << endl;
		
	}


	// resample out sync
	_src_data.src_ratio = _src_out_ratio;
	_src_data.input_frames = alt_frames;
	_src_data.output_frames = nframes;
	_src_data.data_in =  _src_sync_buffer;
	_src_data.data_out = _use_sync_buf + offset;
	src_process (_outsync_src_state, &_src_data);
#endif	
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
	nframes_t bufsize = 65536;
	float ** inbufs = new float*[_chan_count];
	for (unsigned int i=0; i < _chan_count; ++i) {
		inbufs[i] = new float[bufsize];
	}

	float * dummyout = new float[bufsize];
	float * bigbuf   = new float[bufsize * _chan_count];
	
	for (unsigned int i=0; i < _chan_count; ++i)
	{
		/* connect audio ports */
		descriptor->connect_port (_instances[i], AudioInputPort, (LADSPA_Data*) inbufs[i]);
		descriptor->connect_port (_instances[i], AudioOutputPort, (LADSPA_Data*) dummyout);
		descriptor->connect_port (_instances[i], SyncInputPort, (LADSPA_Data*) dummyout);
		descriptor->connect_port (_instances[i], SyncOutputPort, (LADSPA_Data*) dummyout);
	}
	
	// ok, first we need to store some current values
	float old_recthresh = ports[TriggerThreshold];
	float old_syncmode = ports[Sync];
	
	ports[TriggerThreshold] = 0.0f;
	ports[Sync] = 0.0f;
	_slave_sync_port = 0.0;
	
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
			//memset(inbufs[n], 0, sizeof(float) * nframes);

			// duplicate last one
			memcpy (inbufs[n], inbufs[filechans-1], sizeof(float) * nframes);
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
	ports[Sync] = old_syncmode;
	_slave_sync_port = 1.0;
	
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

	// this is called from the man work thread which controls
	// the allocation of loops and jack ports
	// thus, our readonly activity to the current loop does not
	// need a lock to operate safely (because we know it will be safe :)

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
	nframes_t bufsize = 65536;
	float ** outbufs = new float*[_chan_count];
	for (unsigned int i=0; i < _chan_count; ++i) {
		outbufs[i] = new float[bufsize];
	}

	float * bigbuf   = new float[bufsize * _chan_count];

	// now start recording and run for loop length total
	nframes_t nframes = bufsize;
	nframes_t frames_left = (nframes_t) lrintf(ports[LoopLength] * _driver->get_samplerate());

	nframes_t bpos;
	float * databuf;
	nframes_t looppos = 0;
	
	while (frames_left > 0)
	{
		if (nframes > frames_left) {
			nframes = frames_left;
		}


		for (unsigned int i=0; i < _chan_count; ++i)
		{
			// run it for nframes
			nframes = sl_read_current_loop_audio (_instances[i], outbufs[i], nframes, looppos);
		}

		if (nframes == 0) {
			// we're done, it shorted us somehow
			break;
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
		looppos += nframes;
	}
	
	ret = true;

	sf_close (sfile);

	for (unsigned int i=0; i < _chan_count; ++i) {
		delete [] outbufs[i];
	}
	delete [] outbufs;
	delete [] bigbuf;
	
#endif

	return ret;
}
