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


#include "midi_bridge.hpp"

#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <cstdio>
#include <cmath>
#include <cerrno>
#include <cstring>

#include <midi++/parser.h>
#include <midi++/factory.h>

#include "command_map.hpp"
#include "utils.hpp"
#include "engine.hpp"

using namespace SooperLooper;
using namespace std;
using namespace MIDI;
using namespace PBD;


static inline double 
gain_to_uniform_position (double g)
{
	if (g == 0) return 0;
	// this one maxes at 6 dB
	//return pow((6.0*log(g)/log(2.0)+192.0)/198.0, 8.0);

	// this one maxes out at 0 db
	return pow((6.0*log(g)/log(2.0)+198.0)/198.0, 8.0);

}

static inline double 
uniform_position_to_gain (double pos)
{
	if (pos == 0) {
		return 0.0;
	}
	/* XXX Marcus writes: this doesn't seem right to me. but i don't have a better answer ... */
	// this one maxes at 6 dB
	//return pow (2.0,(sqrt(sqrt(sqrt(pos)))*198.0-192.0)/6.0);

	// this one maxes out at 0 db
	return pow (2.0,(sqrt(sqrt(sqrt(pos)))*198.0-198.0)/6.0);
}


MidiBridge::MidiBridge (string name, string oscurl, PortRequest & req)
	: _name (name), _oscurl(oscurl)
{
	// talk directly to the engine via signals AND send osc 

	_port = 0;
	_done = false;
	_learning = false;
	_use_osc = true;
	_addr = 0;
	_midi_thread = 0;
	_clock_thread = 0;
	_clockdone = false;
	_output_clock = false;
	_getnext = false;
	_feedback_out = false;

	_addr = lo_address_new_from_url (_oscurl.c_str());
	if (lo_address_errno (_addr) < 0) {
		fprintf(stderr, "MidiBridge:: addr error %d: %s\n", lo_address_errno(_addr), lo_address_errstr(_addr));
		_use_osc = false;
	}


	PortFactory factory;
	
	if ((_port = factory.create_port (req)) == 0) {
		return;
	}

	// this is a callback that will be made from the parser
	_port->input()->any.connect (slot (*this, &MidiBridge::incoming_midi));

	init_thread();
	init_clock_thread();

	_ok = true;
}

MidiBridge::MidiBridge (string name,  PortRequest & req)
	: _name (name)
{
	// use only midi interface and talk directly to the engine via signals
	
	_port = 0;
	_done = false;
	_learning = false;
	_clockdone = false;
	_use_osc = false;
	_addr = 0;
	_midi_thread = 0;
	_clock_thread = 0;
	_output_clock = false;
	_getnext = false;
	_feedback_out = false;

	PortFactory factory;
	
	if ((_port = factory.create_port (req)) == 0) {
		cerr << "failed to create port" << endl;
		return;
	}

	// this is a callback that will be made from the parser
	_port->input()->any.connect (slot (*this, &MidiBridge::incoming_midi));

	init_thread();
	init_clock_thread();
	_ok = true;
	
}

MidiBridge::MidiBridge (string name)
	: _name (name)
{
	// only talk to engine via signals, midi will be injected
	// by someone else
	
	_port = 0;
	_done = false;
	_clockdone = false;
	_learning = false;
	_use_osc = false;
	_addr = 0;
	_midi_thread = 0;
	_clock_thread = 0;
	_ok = true;	
	_output_clock = false;
	_getnext = false;
	_feedback_out = false;

	init_clock_thread();
}


MidiBridge::~MidiBridge()
{
	if (_addr) {
		lo_address_free (_addr);
	}

	if (_midi_thread) {
		terminate_midi_thread();
	}
	if (_clock_thread) {
		terminate_clock_thread();
	}

	if (_port) {
		delete _port;
		_port = 0;
	}
}


bool
MidiBridge::init_thread()
{

	if (pipe (_midi_request_pipe)) {
		cerr << "Cannot create midi request signal pipe" <<  strerror (errno) << endl;
		return false;
	}

	if (fcntl (_midi_request_pipe[0], F_SETFL, O_NONBLOCK)) {
		cerr << "UI: cannot set O_NONBLOCK on signal read pipe " << strerror (errno) << endl;
		return false;
	}

	if (fcntl (_midi_request_pipe[1], F_SETFL, O_NONBLOCK)) {
		cerr << "UI: cannot set O_NONBLOCK on signal write pipe " << strerror (errno) << endl;
		return false;
	}
	
	pthread_create (&_midi_thread, NULL, &MidiBridge::_midi_receiver, this);
	if (!_midi_thread) {
		return false;
	}
	
	//pthread_detach(_midi_thread);
	//_port->input()->trace(true, &cerr);
	
	return true;
}

void
MidiBridge::terminate_midi_thread ()
{
	void* status;

	_done = true;

	poke_midi_thread ();

	pthread_join (_midi_thread, &status);
}

void
MidiBridge::poke_midi_thread ()
{
	char c = 0;

	if (write (_midi_request_pipe[1], &c, 1) != 1) {
		cerr << "cannot send signal to midi thread! " <<  strerror (errno) << endl;
	}
}

bool MidiBridge::init_clock_thread()
{
	_clock_thread = 0;

	if (pipe (_clock_request_pipe)) {
		cerr << "Cannot create midi request signal pipe" <<  strerror (errno) << endl;
		return false;
	}
	if (fcntl (_clock_request_pipe[0], F_SETFL, O_NONBLOCK)) {
		cerr << "UI: cannot set O_NONBLOCK on clock signal read pipe " << strerror (errno) << endl;
		return false;
	}
	if (fcntl (_clock_request_pipe[1], F_SETFL, O_NONBLOCK)) {
		cerr << "UI: cannot set O_NONBLOCK on clock signal write pipe " << strerror (errno) << endl;
		return false;
	}

	_tempo_updated = false;
	_tempo = 0.0;
	_beatstamp = 0.0;
	_pending_start = false;

	pthread_create (&_clock_thread, NULL, &MidiBridge::_clock_thread_entry, this);
	if (!_clock_thread) {
		return false;
	}

	return true;
}

void * MidiBridge::_clock_thread_entry (void * arg)
{
	return ((MidiBridge *)arg)->clock_thread_entry();
}

void MidiBridge::terminate_clock_thread()
{
	void* status;

	if (_clock_thread) {
		_clockdone = true;
		
		poke_clock_thread ();
		pthread_join (_clock_thread, &status);
	}
}

void MidiBridge::poke_clock_thread()
{
	char c = 0;

	if (write (_clock_request_pipe[1], &c, 1) != 1) {
		cerr << "cannot send signal to midi clock thread! " <<  strerror (errno) << endl;
	}
}


void
MidiBridge::start_learn (MidiBindInfo & info, bool exclus)
{
	//cerr << "starting learn" << endl;
	_learninfo = info;
	_learning = true;
}

void
MidiBridge::start_get_next ()
{
	//cerr << "starting getnext" << endl;
	_getnext = true;
}

void
MidiBridge::finish_learn(MIDI::byte chcmd, MIDI::byte param, MIDI::byte val)
{
	int chan;
	string type;

	TentativeLockMonitor lm (_bindings_lock, __LINE__, __FILE__);
	if (!lm.locked()) {
		// just drop it if we don't get the lock
		return;
	}

	
	if (_learning) {

		if (_midi_bindings.get_channel_and_type (chcmd, chan, type)) {

			_learninfo.channel = chan;
			_learninfo.type = type;
			_learninfo.param = param;

			// if type is n, then lets force the command to be note as well
			if (CommandMap::instance().is_command(_learninfo.control)) {
				if (_learninfo.type == "pc") {
					_learninfo.command = "hit";
				}
				else {
					_learninfo.command = "note";
				}
			}
			
			//_bindings.add_binding(_learninfo);
			// notify of new learn
			//cerr << "learned new one: " << _learninfo.serialize() << endl;

			BindingLearned(_learninfo); // emit

			_learning = false;
		}
		else {
			//cerr << "invalid event to learn: " << (int) chcmd  << endl;
		}
	}
	else if (_getnext) {

		if (_midi_bindings.get_channel_and_type (chcmd, chan, type)) {
			MidiBindInfo info;
			info.channel = chan;
			info.type = type;
			info.param = param;

			// notify of new learn
			//cerr << "recvd new one: " << info.serialize() << endl;
			NextMidiReceived (info); // emit

			_getnext = false;
		}
		else {
			//cerr << "invalid event to get: " << (int) chcmd  << endl;
		}

	}
}

void
MidiBridge::cancel_learn()
{
	//cerr << "cancel learn" << endl;
	_learning = false;
}

void
MidiBridge::cancel_get_next()
{
	//cerr << "cancel get next" << endl;
	_getnext = false;
}


void
MidiBridge::incoming_midi (Parser &p, byte *msg, size_t len, timestamp_t timestamp)
{
	/* we only respond to channel messages */

	byte b1 = msg[0];
	byte b2 = len>1 ? msg[1]: 0;
	byte b3 = len>2 ? msg[2]: 0;

// 	if ((msg[0] & 0xF0) < 0x80 || (msg[0] & 0xF0) > 0xE0) {
// 		return;
// 	}

	// convert noteoffs to noteons with val = 0
	if ((msg[0] & 0xF0) == MIDI::off) {
 		b1 = MIDI::on | (b1 & 0x0F);
		b3 = 0;
	}
	
	// fprintf(stderr, "incmoing: %02x  %02x  %02x\n", (int) b1,(int) b2, (int) b3);
	
	if (_learning || _getnext) {
		finish_learn(b1, b2, b3);
	}
	else {
		queue_midi (b1, b2, b3, -1, timestamp);
	}
}


void
MidiBridge::inject_midi (MIDI::byte chcmd, MIDI::byte param, MIDI::byte val, long framepos)
{
	// convert noteoffs to noteons with val = 0
	if ((chcmd & 0xF0) == MIDI::off) {
 		chcmd = MIDI::on | (chcmd & 0x0F);
	        val = 0;
	}
	
	
	if (_learning || _getnext) {
		finish_learn(chcmd, param, val);
	}
	else {
		queue_midi (chcmd, param, val, framepos);
	}
}


void
MidiBridge::queue_midi (MIDI::byte chcmd, MIDI::byte param, MIDI::byte val, long framepos, timestamp_t timestamp)
{
	TentativeLockMonitor lm (_bindings_lock, __LINE__, __FILE__);
	if (!lm.locked()) {
		// just drop it if we don't get the lock
		return;
	}

	// convert midi to lookup key
	// lookup key = (chcmd << 8) | param

	int key = (chcmd << 8) | param;

	switch(chcmd & 0xF0) 
	{
	case MIDI::chanpress:
		val = param;
		// fallthrough intentional
	case MIDI::pitchbend:
		key = chcmd << 8;
		break;
	default: break;// nothing
	}

	MidiBindings::BindingsMap::iterator iter = _midi_bindings.bindings_map().find(key);

	if (iter != _midi_bindings.bindings_map().end())
	{
		MidiBindings::BindingList & elist = (*iter).second;

		for (MidiBindings::BindingList::iterator eiter = elist.begin(); eiter != elist.end(); ++eiter) {
			MidiBindInfo & info = (*eiter);
			float scaled_val = 0.0;
			float val_ratio;
			int clamped_val;

			if ((((info.type == "off") || (info.type == "ccon")) && val > 0) || (((info.type == "on") || (info.type == "ccoff")) && val == 0)) {
				// binding was for note off or on only, skip this
				continue;
			}

			// clamp it
			if ((chcmd & 0xF0) == MIDI::pitchbend) {
				clamped_val = min( info.data_max, max(info.data_min, (param | (val << 7))));
				// cerr << "Pitchbend: " << (int) (param | (val << 7)) << "  clamped: " << clamped_val << endl;
			}
			else {
				clamped_val = min((MIDI::byte) info.data_max, max((MIDI::byte) info.data_min, val));
			}

			if (info.data_min == info.data_max) {
				val_ratio = 0.0f;
			}
			else {
				// calculate value as a ratio to map to the target range
				val_ratio = (clamped_val - info.data_min) / (float)(info.data_max - info.data_min);
			}

			if (info.style == MidiBindInfo::GainStyle) {
				scaled_val = (float) (val_ratio *  ( info.ubound - info.lbound)) + info.lbound;
				scaled_val = uniform_position_to_gain (scaled_val);
			}
			else if (info.style == MidiBindInfo::NormalStyle) {
				scaled_val = (float) (val_ratio *  ( info.ubound - info.lbound)) + info.lbound;
			}
			else if (info.style == MidiBindInfo::IntegerStyle) {
				// round to nearest integer value
				scaled_val = (float) nearbyintf((val_ratio *  ( info.ubound - info.lbound)) + info.lbound);
			}
			else {
				// toggle style is a bit of a hack, but here we go
				if (info.last_toggle_val != info.ubound) {
					scaled_val = info.ubound;
				} 
				else {
					scaled_val = info.lbound;
				}
				info.last_toggle_val = scaled_val;
			}

			//fprintf(stderr, "found binding: key: %x  val: %02x  scaled: %g  type: %s\n", (int) key, (int) val, scaled_val, info.type.c_str());
			//cerr << "ctrl: " << info.control << "  cmd: " << info.command << endl;

			send_event (info, scaled_val, framepos);
		}
	}
	else if (chcmd == MIDI::start || chcmd == MIDI::contineu) {  // MIDI start
		if (_use_osc) {
			lo_send(_addr, "/sl/midi_start", "");
		}
		MidiSyncEvent (Event::MidiStart, framepos, timestamp); // emit
		//cerr << "got start" << endl;
	}
	else if (chcmd == MIDI::stop) { // MIDI stop
		if (_use_osc) {
			lo_send(_addr, "/sl/midi_stop", "");
		}
		MidiSyncEvent (Event::MidiStop, framepos, timestamp); // emit
		//cerr << "got stop" << endl;
	}
	else if (chcmd == MIDI::timing) {  // MIDI clock tick
		if (_use_osc) {
			lo_send(_addr, "/sl/midi_tick", "");
		}
		MidiSyncEvent (Event::MidiTick, framepos, timestamp); // emit
	}
	else {
		// fprintf(stderr, "binding %x not found\n", key);
	}
}

void
MidiBridge::send_event (const MidiBindInfo & info, float val, long framepos)
{
	static char tmpbuf[100];


	string cmd = info.command;
	CommandMap & cmdmap = CommandMap::instance();
	
	Event::type_t optype = cmdmap.to_type_t (cmd);
	Event::control_t ctrltype = cmdmap.to_control_t (info.control);
	Event::command_t cmdtype = cmdmap.to_command_t (info.control);
	
	if (cmd == "set") {
		if (_use_osc) {
			snprintf (tmpbuf, sizeof(tmpbuf)-1, "/sl/%d/%s", info.instance, cmd.c_str());
			
			if (lo_send(_addr, tmpbuf, "sf", info.control.c_str(), val) < 0) {
				fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(_addr), lo_address_errstr(_addr));
			}
		}
		
		MidiControlEvent (optype, ctrltype, val, (int8_t) info.instance, framepos); // emit
	}
	else {
		if (cmd == "note") {
			if (val > 0.0f) {
				//cerr << "val is " << val << "  type is: " << info.type << endl;
				cmd = "down";
				optype = Event::type_cmd_down;
			}
			else {
				cmd = "up";
				optype = Event::type_cmd_up;
			}
		}
		else if (cmd == "susnote") {
			if (val > 0.0f) {
				cmd = "down";
				optype = Event::type_cmd_down;
			}
			else {
				cmd = "upforce";
				optype = Event::type_cmd_upforce;
			}
		}

		if (_use_osc) {
			snprintf (tmpbuf, sizeof(tmpbuf)-1, "/sl/%d/%s", info.instance, cmd.c_str());
			
			if (lo_send(_addr, tmpbuf, "s", info.control.c_str()) < 0) {
				fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(_addr), lo_address_errstr(_addr));
			}
		}
		
		MidiCommandEvent (optype, cmdtype, (int8_t) info.instance, framepos); // emit
	}
}



void * MidiBridge::_midi_receiver(void *arg)
{
	MidiBridge * bridge = static_cast<MidiBridge*> (arg);

	bridge->midi_receiver();
	return 0;
}

void  MidiBridge::midi_receiver()
{
	struct pollfd pfd[3];
	int nfds = 0;
	int timeout = -1;
	MIDI::byte buf[512];

	while (!_done) {
		nfds = 0;

		pfd[nfds].fd = _midi_request_pipe[0];
		pfd[nfds].events = POLLIN|POLLHUP|POLLERR;
		nfds++;

		if (_port && _port->selectable() >= 0) {
			pfd[nfds].fd = _port->selectable();
			pfd[nfds].events = POLLIN|POLLHUP|POLLERR;
			nfds++;
		}
		
	again:
		// cerr << "poll on " << nfds << " for " << timeout << endl;
		if (poll (pfd, nfds, timeout) < 0) {
			if (errno == EINTR) {
				/* gdb at work, perhaps */
				goto again;
			}
			
			cerr << "MIDI thread poll failed: " <<  strerror (errno) << endl;
			
			break;
		}

		if (_done) {
			break;
		}
		
		if ((pfd[0].revents & ~POLLIN)) {
			cerr << "Transport: error polling extra MIDI port" << endl;
			break;
		}
		
		if (nfds > 1 && pfd[1].revents & POLLIN) {

			/* reading from the MIDI port activates the Parser
			   that in turn generates signals that we care
			   about. the port is already set to NONBLOCK so that
			   can read freely here.
			*/
			
			while (1) {
				
				// cerr << "+++ READ ON " << port->name() << endl;
				
				int nread = _port->read (buf, sizeof (buf));
				
				// cerr << "-- READ (" << nread << " ON " << port->name() << endl;
				
				if (nread > 0) {
					if ((size_t) nread < sizeof (buf)) {
						break;
					} else {
						continue;
					}
				} else if (nread == 0) {
					break;
				} else if (errno == EAGAIN) {
					break;
				} else {
					cerr << "Error reading from midi port" << endl;
					_done = true;
					break;
				}
			}
			
		}
	}
}

void * MidiBridge::clock_thread_entry()
{
	//struct pollfd pfd[3];
	int nfds = 0;
	double ticktime = -1;
	int ret;
	MIDI::byte clockmsg = MIDI::timing;
	MIDI::byte startmsg = MIDI::start;
	MIDI::byte stopmsg = MIDI::stop;
	//MIDI::byte contmsg = MIDI::contineu;

	struct timeval timeoutval = {1, 0};
	double steady_timeout = 0.03333333333;
	struct timeval steady_timeout_val = { 0, 33333 }; // 1/30 sec
	struct timeval noout_timeoutval = {1, 0};
	struct timeval * timeoutp = 0;
	MIDI::timestamp_t nextstamp = 0;
	double nowtime;
	fd_set pfd;
	char buf[10];
	unsigned long ticks = 0;

	if (!_port) return 0;

	//cerr << "entering clock thread" << endl;
	
	while (!_clockdone) {
		nfds = 0;

		/*
		if (_port && _port->selectable() >= 0) {
			pfd[nfds].fd = _port->selectable();
			pfd[nfds].events = POLLIN|POLLHUP|POLLERR;
			nfds++;
		}
		*/
		

	again:
		FD_SET(_clock_request_pipe[0], &pfd);
		nfds = _clock_request_pipe[0] + 1;

		//cerr << "select on " << nfds << " for " << steady_timeout << endl;


		if ((ret = select (nfds, &pfd, NULL, NULL, timeoutp)) < 0) {
			if (errno == EINTR) {
				/* gdb at work, perhaps */
				goto again;
			}
			
			cerr << "MIDI thread select failed: " <<  strerror (errno) << endl;
			
			break;
		}

		if (_clockdone) {
			break;
		}
		
		// time to write out a clock message, the timer expired
		nowtime = _port->get_current_host_time();
		
		if (ret == 1) {
			::read(_clock_request_pipe[0], &buf, 1);
			//cerr << "poke event read" << endl;
		}
		else if (_output_clock && nextstamp > 0) {

			if (_pending_start) {
				_port->write(&startmsg, 1);
				_pending_start = false;
			}

			// write out clocks until the next limit in the future

			double nextlimit = nowtime + 2*steady_timeout;
			
			while (nextstamp <= nextlimit) 
			{
				_port->write_at(&clockmsg, 1, nextstamp);

				//cerr << "CLOCK output at" << nextstamp << endl;			
				nextstamp += ticktime; 
				++ticks;
			}

			
		}
		else {
			//cerr << "ret was : " << ret << " timeoutp: " << timeoutp << endl;
		}

		if (!_output_clock) {
			//cerr << "no clock output" << endl;
			noout_timeoutval.tv_sec = 1;
			noout_timeoutval.tv_usec = 0;
			timeoutp = &noout_timeoutval;
			continue;
		}

		// recalculate timeout
		if (_tempo_updated) 
		{
			// Example: At a tempo of 120 BPM, there are 120 quarter notes per minute.
			// There are 24 MIDI clocks in each quarter note.
			// Therefore, there should be 24 * 120 MIDI Clocks per minute. 
			// So, each MIDI Clock is sent at a rate of 60/(24 * 120) seconds)
			// timeout interval = 60/(24 * bpm);

			if (_tempo > 0.0) {
				ticktime = (60.0/(24 * _tempo));

				// first timeout will sync us up to the correct beat timestamp, 
				// then the steady state one will be used
				
				// wait just enough to get us to the next multiple of the beat stamp
				nextstamp = _beatstamp;
				ticks = 0;
				//fprintf(stderr, "beatstamp: %.14g  nowtime: %.14g\n", _beatstamp, nowtime);

				double delta =  fabs(nowtime - nextstamp);
				ticks = lrint(delta/ticktime) + 1;
				nextstamp += ticks * ticktime;

				// fprintf(stderr, "beatstamp: %.14g  nowtime: %.14g  nextstamp: %.14g\n", _beatstamp, nowtime, nextstamp);
				timeoutval = steady_timeout_val;
				timeoutp = &timeoutval;
			}
			else {
				timeoutp = NULL;
				_port->write(&stopmsg, 1);
			}

			_tempo_updated = false;
		}
		else if (_tempo > 0) {
			timeoutval = steady_timeout_val;
			timeoutp = &timeoutval;
		}
		else {
			// tempo is 0
			timeoutp = NULL;
		}
	}

	//cerr << "clock thread ending" << endl;
	return 0;
}

void MidiBridge::tempo_clock_update(double tempo, MIDI::timestamp_t timestamp, bool forcestart)
{
	//cerr << "set tempo to: " << tempo << "  at: " << timestamp << endl;
	while (tempo > 240.0) {
		tempo *= 0.5;
	}
	
	if (forcestart || (_tempo == 0.0 && tempo > 0.0)) {
		_pending_start = true;
	}
	_tempo = tempo;
	_beatstamp = timestamp;
	_tempo_updated = true;
	poke_clock_thread();
}

MIDI::timestamp_t MidiBridge::get_current_host_time()
{
	if (_port) {
		return _port->get_current_host_time();
	}
	else return 0;
}


void MidiBridge::parameter_changed(int ctrl_id, int instance, Engine *engine)
{
	if (!_feedback_out) return;

	int selected_loop = (int) engine->get_control_value(Event::SelectedLoopNum, -2);
	bool selected = (selected_loop == instance || selected_loop == -1 || engine->loop_count()==1);

	if (ctrl_id == SooperLooper::Event::State)
	{
		int currstate = (int) engine->get_control_value(Event::State, instance);
	}
	else {
		float value = engine->get_control_value((Event::control_t)ctrl_id, instance);
	
		if (instance == -1) {
			// all loops
		}
		else {
			// normal control

		}
	}
}
