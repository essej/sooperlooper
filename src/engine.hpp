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

#ifndef __sooperlooper_engine__
#define __sooperlooper_engine__

#include <vector>
#include <string>

#include <sigc++/sigc++.h>

#include "lockmonitor.hpp"
#include "ringbuffer.hpp"
#include "event.hpp"
#include "event_nonrt.hpp"
#include "audio_driver.hpp"

namespace SooperLooper {

class Looper;
class ControlOSC;
	
class Engine
{
  public:
	
	Engine();
	virtual ~Engine();

	bool initialize(AudioDriver * driver, int port=9951, std::string pingurl="");

	AudioDriver * get_audio_driver () { return _driver; }
	ControlOSC  * get_control_osc () { return _osc; }

	void set_default_loop_secs (float secs) { _def_loop_secs = secs; }
	void set_default_channels (int chan) { _def_channel_cnt = chan; }
	
	
	bool is_ok() const { return _ok; }

	void quit();

	bool add_loop (unsigned int chans);
	bool remove_loop (unsigned int index);
	
	unsigned int loop_count() { PBD::LockMonitor lm(_instance_lock, __LINE__, __FILE__); return _instances.size(); }
	unsigned int loop_count_unsafe() { return _instances.size(); }

	int process (nframes_t);

	//RingBuffer<Event> & get_event_queue() { return *_event_queue; }
	

	EventGenerator & get_event_generator() { return *_event_generator;}

	bool push_command_event (Event::type_t type, Event::command_t cmd, int8_t instance);
	bool push_control_event (Event::type_t type, Event::control_t ctrl, float val, int8_t instance);

	bool push_sync_event (Event::control_t ctrl);
	
	std::string get_osc_url (bool udp=true);
	int get_osc_port ();

	float get_control_value (Event::control_t, int8_t instance);
	
	SigC::Signal1<void, int> LoopAdded;
	SigC::Signal0<void> LoopRemoved;

	// the main non-rt event processing loop
	void mainloop();
	
	bool push_nonrt_event (EventNonRT * event);
	
	
  protected:	

	void cleanup();

	bool process_nonrt_event (EventNonRT * event);

	void generate_sync (nframes_t offset, nframes_t nframes);
	
	void update_sync_source ();
	void calculate_tempo_frames ();
	void calculate_midi_tick ();

	void do_global_rt_event (Event * ev, nframes_t offset, nframes_t nframes);
	
	AudioDriver * _driver;
	
	ControlOSC * _osc;
	
	typedef std::vector<Looper*> Instances;
	Instances _instances;
	PBD::NonBlockingLock _instance_lock;

	volatile bool _ok;

	// RT event queue
	RingBuffer<Event> * _event_queue;
	RingBuffer<Event> * _sync_queue;

	EventGenerator * _event_generator;

	// non-rt event stuff

	RingBuffer<EventNonRT *> * _nonrt_event_queue;
	
	PBD::Lock _event_loop_lock;
	pthread_cond_t  _event_cond;

	int _def_channel_cnt;
	float _def_loop_secs;

	// global parameters
	enum SyncSourceType {
		FIRST_SYNC_SOURCE=-5, // must be first
		BrotherSync = -4,
		InternalTempoSync = -3,
		MidiClockSync = -2,
		JackSync = -1,
		NoSync = 0
		// anything > 0 is considered a loop number
	};

	float  *       _internal_sync_buf;
	SyncSourceType _sync_source;

	double    _tempo;        // bpm
	float    _eighth_cycle; // eighth notes per loop cycle

   private:

	double _tempo_counter;
	double _tempo_frames;

	unsigned int _midi_ticks;     // counts ticks as they're coming in
	unsigned int _midi_loop_tick; // tick number to loop (sync) on

	nframes_t _running_frames;
	nframes_t _last_tempo_frame;
	volatile bool _tempo_changed;
};

};  // sooperlooper namespace

#endif
