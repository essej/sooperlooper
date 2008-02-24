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

#ifndef __sooperlooper_midi_bridge__
#define __sooperlooper_midi_bridge__

#include <stdint.h>
#include <lo/lo.h>

#include <cstdio>

#include <string>
#include <vector>
#include <map>

#include <sigc++/sigc++.h>

#include <midi++/types.h>
#include <midi++/port.h>
#include <midi++/port_request.h>

#include "event.hpp"
#include "midi_bind.hpp"
#include "lockmonitor.hpp"

namespace SooperLooper {

class MidiBridge
	: public SigC::Object
{
  public:

	MidiBridge (std::string name);
	MidiBridge (std::string name, MIDI::PortRequest & req);
	MidiBridge (std::string name, std::string oscurl, MIDI::PortRequest & req);
	virtual ~MidiBridge();			

	MidiBindings & bindings() { return _midi_bindings; }
	PBD::NonBlockingLock & bindings_lock() { return _bindings_lock; }
	
	virtual bool is_ok() { return _ok; }

	void start_learn (MidiBindInfo & info, bool exclus=false);
	void cancel_learn();

	void start_get_next ();
	void cancel_get_next ();
	
	SigC::Signal1<void, MidiBindInfo> BindingLearned;
	SigC::Signal1<void, MidiBindInfo> NextMidiReceived;

	// type, command, loop index, framepos (-1 if not set)
	SigC::Signal4<void, Event::type_t, Event::command_t, int8_t, long> MidiCommandEvent;
	SigC::Signal5<void, Event::type_t, Event::control_t, float, int8_t, long> MidiControlEvent;

	SigC::Signal2<void, Event::control_t, long> MidiSyncEvent;
	

	void inject_midi (MIDI::byte chcmd, MIDI::byte param, MIDI::byte val, long framepos=-1);

	// the tempo updated on a beat starting at timestamp
	void tempo_clock_update(double tempo, MIDI::timestamp_t timestamp, bool forcestart=false);

	MIDI::timestamp_t get_current_host_time();

  protected:
	bool init_thread();
	void terminate_midi_thread();
	void poke_midi_thread();
	
	void incoming_midi (MIDI::Parser &p, MIDI::byte *msg, size_t len);
	
	void queue_midi (MIDI::byte chcmd, MIDI::byte param, MIDI::byte val, long framepos=-1);


	static void * _midi_receiver (void * arg);
	void midi_receiver ();
	void stop_midireceiver ();

	void finish_learn(MIDI::byte chcmd, MIDI::byte param, MIDI::byte val);
	

	bool init_clock_thread();
	void terminate_clock_thread();
	static void * _clock_thread_entry (void * arg);
	void * clock_thread_entry();
	void poke_clock_thread();

	std::string _name;
	std::string _oscurl;

  private:

	void send_event (const MidiBindInfo & info, float val, long framepos=-1);
	

	MidiBindings _midi_bindings;
	
	MIDI::Port * _port;
	
	lo_address _addr;

	int                _midi_request_pipe[2];
	pthread_t          _midi_thread;

	int                _clock_request_pipe[2];
	pthread_t          _clock_thread;

	PBD::NonBlockingLock _bindings_lock;

	bool _use_osc;
	volatile bool _done;
	volatile bool _clockdone;

	volatile bool _tempo_updated;
	volatile double _tempo;
	volatile MIDI::timestamp_t _beatstamp;
	volatile bool _pending_start;

	bool _learning;
	bool _getnext;
	MidiBindInfo _learninfo;
	bool _ok;
	
};

};


#endif
