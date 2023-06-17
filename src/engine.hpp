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
#include "midi_bind.hpp"
#include "command_map.hpp"

namespace SooperLooper {

class Looper;
class ControlOSC;
class MidiBridge;
	
class Engine
	: public sigc::trackable
{
  public:

	static const int TEMPO_WINDOW_SIZE = 4;
	static const int TEMPO_WINDOW_SIZE_MASK = 3;
	
	Engine();
	virtual ~Engine();

	bool initialize(AudioDriver * driver, int buschans=2, int port=9951, std::string pingurl="");
	void cleanup();

	AudioDriver * get_audio_driver () { return _driver; }
	ControlOSC  * get_control_osc () { return _osc; }

	void set_default_loop_secs (float secs) { _def_loop_secs = secs; }
	void set_default_channels (int chan) { _def_channel_cnt = chan; }
	
	void set_midi_bridge (MidiBridge * bridge);
	MidiBridge * get_midi_bridge() { return _midi_bridge; }
	
	bool is_ok() const { return _ok; }

	void set_ignore_quit(bool val) { _ignore_quit = val; }
	bool get_ignore_quit() { return _ignore_quit; }

	void quit(bool force=false);

	bool add_loop (unsigned int chans, float loopsecs=40.0f, bool discrete = true);
	bool add_loop (Looper * instance);
	bool remove_loop (Looper * loop);
	
	void set_force_discrete(bool flag) { _force_discrete = flag; }
	bool get_force_discrete() const { return _force_discrete; }
	
	size_t loop_count() { return _instances.size(); }

	size_t get_loop_channel_count(size_t instance, bool realtime);
	
	int process (nframes_t);

	void buffersize_changed (nframes_t);
	
	//RingBuffer<Event> & get_event_queue() { return *_event_queue; }

	bool get_common_input (unsigned int chan, port_id_t & port);
	bool get_common_output (unsigned int chan, port_id_t & port);
	sample_t * get_common_input_buffer (unsigned int chan);
	sample_t * get_common_output_buffer (unsigned int chan);

	size_t  get_common_output_count () { return _common_outputs.size(); }
	size_t  get_common_input_count () { return _common_outputs.size(); }
	
	EventGenerator & get_event_generator() { return *_event_generator;}

	bool push_command_event (Event::type_t type, Event::command_t cmd, int8_t instance);
	void push_control_event (Event::type_t type, Event::control_t ctrl, float val, int8_t instance, int src=0);

	void push_midi_command_event (Event::type_t type, Event::command_t cmd, int8_t instance, long framepos=-1);
	void push_midi_control_event (Event::type_t type, Event::control_t ctrl, float val, int8_t instance, long framepos=-1);
	
	void push_sync_event (Event::control_t ctrl, long framepos=-1, MIDI::timestamp_t timestamp=0);
	
	std::string get_osc_url (bool udp=true);
	int get_osc_port ();

	bool load_midi_bindings (std::string filename, bool append, CommandMap & cmdmap);
	bool load_midi_bindings (std::istream & instream, bool append, CommandMap & cmdmap);

	float get_control_value (Event::control_t, int8_t instance);
	std::string get_property_value (std::string prop, int8_t instance);
	void set_property_value (std::string prop, int8_t instance, std::string value);

	sigc::signal2<void, int, bool> LoopAdded;
	sigc::signal0<void> LoopRemoved;

	sigc::signal2<void, int , int> ParamChanged;

	// the main non-rt event processing loop
	void mainloop();
	
	bool push_nonrt_event (EventNonRT * event);
	
	void binding_learned(MidiBindInfo info);
	void next_midi_received(MidiBindInfo info);

	// session state
	bool load_session (std::string fname, std::string * readstr=0);
	bool save_session (std::string fname, bool write_audio = false, std::string * writestr=0);
	
	int get_id() const { return _unique_id; }

	bool get_transport_always_rolls() const { return _transport_always_rolls; }
	void set_transport_always_rolls();

  protected:	

	struct LoopManageEvent
	{
		enum EventType {
			AddLoop = 0,
			RemoveLoop,
			LoadSession
		};

		LoopManageEvent () {}
		LoopManageEvent (EventType et, Looper *loop) : etype(et), looper(loop) {}

		EventType etype;
		Looper * looper;
	};
	

	bool process_nonrt_event (EventNonRT * event);
	void process_rt_loop_manage_events ();
	
	// returns >= 0 offset position on tempo beats
	int generate_sync (nframes_t offset, nframes_t nframes);
	
	void update_sync_source ();
	void calculate_tempo_frames ();
	void calculate_midi_tick (bool rt=true);

	void do_global_rt_event (Event * ev, nframes_t offset, nframes_t nframes);

	bool do_push_command_event (RingBuffer<Event> * rb, Event::type_t type, Event::command_t cmd, int8_t instance, long framepos=-1);
	bool do_push_control_event (RingBuffer<Event> * rb, Event::type_t type, Event::control_t ctrl, float val, int8_t instance, long framepos=-1, int src=0);

	bool push_loop_manage_to_rt (LoopManageEvent & lme);
	bool push_loop_manage_to_main (LoopManageEvent & lme);
	
	void set_tempo (double tempo, bool rt=true);

	inline double avg_tempo(double tempo);
	inline void reset_avg_tempo(double tempo=0.0);

	void fill_common_outs(nframes_t nframes);
	void prepare_buffers(nframes_t nframes);

	void connections_changed();

	void handle_load_session_event();

	
	AudioDriver * _driver;
	
	ControlOSC * _osc;

	MidiBridge * _midi_bridge;
	
	typedef std::vector<Looper*> Instances;
	// the rt thread keeps this one
	Instances _rt_instances;

	// the non-rt keeps this copy
	Instances _instances;
	PBD::NonBlockingLock _instance_lock;

	// looper (de)allocation event
	RingBuffer<LoopManageEvent> * _loop_manage_to_rt_queue;
	RingBuffer<LoopManageEvent> * _loop_manage_to_main_queue;
	
	bool _ignore_quit;
	volatile bool _ok;

	volatile bool _learn_done;
	volatile bool _received_done;
	MidiBindInfo  _learninfo;
	MidiBindingEvent _learn_event;
	
	// RT event queue
	RingBuffer<Event> * _event_queue;
	RingBuffer<Event> * _midi_event_queue;
	RingBuffer<Event> * _sync_queue;
	RingBuffer<Event> * _nonrt_update_event_queue;

	EventGenerator * _event_generator;

	// non-rt event stuff

	RingBuffer<EventNonRT *> * _nonrt_event_queue;
	
	PBD::NonBlockingLock _event_loop_lock;
	pthread_cond_t  _event_cond;

	int _def_channel_cnt;
	float _def_loop_secs;
	nframes_t _buffersize;
	
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
	int _sync_source;

	volatile double    _tempo;        // bpm
	volatile MIDI::timestamp_t _beatstamp; // timestamp at the beat of the last tempo change
	volatile MIDI::timestamp_t _prev_beatstamp; 
	bool _force_next_clock_start;
	volatile bool _send_midi_start_after_next_hit;
	bool _send_midi_start_on_trigger;

	float    _eighth_cycle; // eighth notes per loop cycle

	std::vector<port_id_t>  _common_inputs;
	std::vector<port_id_t>  _common_outputs;

	std::vector<sample_t *>    _common_input_buffers;
	std::vector<sample_t *>    _common_output_buffers;
	std::vector<sample_t *>    _temp_input_buffers;              
	std::vector<sample_t *>    _temp_output_buffers;              
	bool                    _use_temp_input;
	
	float              _curr_common_dry;
	float              _target_common_dry;

	float              _curr_common_wet;
	float              _target_common_wet;

	float              _curr_input_gain;
	float              _target_input_gain;
	
	float              _common_input_peak;
	float              _common_output_peak;
	float              _falloff_per_sample;
	
	bool               _auto_disable_latency;
	int                _selected_loop;
	bool               _jack_timebase_master;

	bool               _output_midi_clock;
	bool               _smart_eighths;
	bool               _force_discrete;
	
	int                _unique_id;
	bool               _transport_always_rolls;

   private:

	double _tempo_counter;
	double _tempo_frames;
	double _quarter_note_frames;
	double _quarter_counter;
	
	unsigned int _midi_ticks;     // counts ticks as they're coming in
	unsigned int _midi_loop_tick; // tick number to loop (sync) on

	nframes_t _running_frames;
	nframes_t _last_tempo_frame;
	volatile bool _tempo_changed;
	volatile bool _beat_occurred;
	volatile bool _conns_changed;
	volatile bool _sel_loop_changed;
	volatile bool _timebase_changed;

	double _tempo_averages[TEMPO_WINDOW_SIZE];
	double _running_tempo_sum;
	unsigned int    _avgindex;

	nframes_t       _longpress_frames;
	nframes_t       _solo_down_stamp;

	bool _use_sync_start;
	bool _use_sync_stop;

	bool _loading;

	SessionEvent* _load_sess_event;
};



inline double Engine::avg_tempo(double tempo)
{
	//double alpha = 0.8;
	//tempo =  (_tempo*alpha + tempo*(1-alpha));
	_running_tempo_sum += tempo;
	_running_tempo_sum -= _tempo_averages[_avgindex];
	_tempo_averages[_avgindex] = tempo;
	_avgindex = (_avgindex + 1) & TEMPO_WINDOW_SIZE_MASK;
	return _running_tempo_sum / (double) TEMPO_WINDOW_SIZE;

}

inline void Engine::reset_avg_tempo(double tempo)
{
	for (int i=0; i < TEMPO_WINDOW_SIZE; ++i) {
		_tempo_averages[i] = tempo;
	}

	_running_tempo_sum = tempo * TEMPO_WINDOW_SIZE;
	_avgindex = TEMPO_WINDOW_SIZE_MASK;
}

	
};  // sooperlooper namespace

#endif
