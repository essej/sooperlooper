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

#ifndef __sooperlooper_alsa_midi_bridge__
#define __sooperlooper_alsa_midi_bridge__

#include "midi_bridge.hpp"

#include <string>
#include <alsa/asoundlib.h>
#include <pthread.h>


namespace SooperLooper {

class AlsaMidiBridge
	: public MidiBridge
{
  public:

	AlsaMidiBridge (std::string name, std::string oscurl);
	virtual ~AlsaMidiBridge();			

	virtual bool is_ok() {return _seq != 0;}
	
  protected:
	
	static void * _midi_receiver (void * arg);

	void midi_receiver ();
	void stop_midireceiver ();

	snd_seq_t * create_sequencer (std::string client_name, bool isinput);
	
		
	snd_seq_t *         _seq;
	pthread_t          _midi_thread;

	bool _done;
};

};


#endif
