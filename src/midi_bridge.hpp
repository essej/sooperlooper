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

#include "midi_bind.hpp"

namespace SooperLooper {

class MidiBridge
	: public SigC::Object
{
  public:

	MidiBridge (std::string name, std::string oscurl, MIDI::PortRequest & req);
	virtual ~MidiBridge();			

	MidiBindings & bindings() { return _midi_bindings; }

	virtual bool is_ok() { return _port != 0; }

	void start_learn (MidiBindInfo & info, bool exclus=false);
	void cancel_learn();

	SigC::Signal1<void, MidiBindInfo> BindingLearned;
	
  protected:
	bool init_thread();
	void terminate_midi_thread();
	void poke_midi_thread();
	
	void incoming_midi (MIDI::Parser &p, MIDI::byte *msg, size_t len);
	
	void queue_midi (MIDI::byte chcmd, MIDI::byte param, MIDI::byte val);


	static void * _midi_receiver (void * arg);
	void midi_receiver ();
	void stop_midireceiver ();

	void finish_learn(MIDI::byte chcmd, MIDI::byte param, MIDI::byte val);
	
	
	std::string _name;
	std::string _oscurl;

  private:

	void send_osc (const MidiBindInfo & info, float val);
	

	MidiBindings _midi_bindings;
	
	MIDI::Port * _port;
	
	lo_address _addr;

	int                _midi_request_pipe[2];
	pthread_t          _midi_thread;

	bool _done;
	bool _learning;
	MidiBindInfo _learninfo;
	
};

};


#endif
