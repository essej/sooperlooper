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

#include <cstdio>
#include <iostream>
#include <cerrno>
#include "loop_control.hpp"

#include <wx/filename.h>

#include <sys/poll.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#include <midi_bind.hpp>
#include "plugin.hpp"

using namespace std;
using namespace SooperLooperGui;
using namespace SooperLooper;

#define DEFAULT_OSC_PORT 9951

LoopControl::SpawnConfig::SpawnConfig(const wxString & nm)
	: name(nm), host(wxT("")), port(DEFAULT_OSC_PORT), num_loops(1), num_channels(2),
	  mem_secs(40.0f), exec_name(wxT("sooperlooper")), midi_bind_path(wxT("")), force_spawn(false), never_spawn(false),
	  jack_name(wxT(""))
{
}


XMLNode& LoopControl::SpawnConfig::get_state () const
{
	XMLNode *node = new XMLNode ("context");

	node->add_property ("name", name.c_str());

	node->add_property ("host", host.c_str());
	node->add_property ("port", wxString::Format(wxT("%ld"), port).c_str());
	node->add_property ("num_loops", wxString::Format(wxT("%ld"), num_loops).c_str());
	node->add_property ("num_channels", wxString::Format(wxT("%ld"), num_channels).c_str());
	node->add_property ("mem_secs", wxString::Format(wxT("%f"), mem_secs).c_str());

	node->add_property ("exec_name", exec_name.c_str());
	node->add_property ("jack_name", jack_name.c_str());
	node->add_property ("jack_serv_name", jack_serv_name.c_str());
	node->add_property ("midi_bind_path", midi_bind_path.c_str());

	node->add_property ("force_spawn", force_spawn ? "yes": "no");
	
	return *node;
}

int LoopControl::SpawnConfig::set_state (const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNodeConstIterator niter;
	XMLNode *child_node;

	wxString tmpstr;
	
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		child_node = *niter;

		if (child_node->name() == "context") {
			XMLProperty *prop;
			
			if ((prop = child_node->property ("name")) != 0) {
				name.Printf(wxT("%s"), prop->value().c_str());
			}
			if ((prop = child_node->property ("host")) != 0) {
				host.Printf(wxT("%s"), prop->value().c_str());
			}
			if ((prop = child_node->property ("port")) != 0) {
				tmpstr.Printf(wxT("%s"), prop->value().c_str());
				tmpstr.ToLong(&port);
			}
			if ((prop = child_node->property ("num_loops")) != 0) {
				tmpstr.Printf(wxT("%s"), prop->value().c_str());
				tmpstr.ToLong(&num_loops);
			}
			if ((prop = child_node->property ("num_channels")) != 0) {
				tmpstr.Printf(wxT("%s"), prop->value().c_str());
				tmpstr.ToLong(&num_channels);
			}
			if ((prop = child_node->property ("mem_secs")) != 0) {
				tmpstr.Printf(wxT("%s"), prop->value().c_str());
				tmpstr.ToDouble(&mem_secs);
			}
			if ((prop = child_node->property ("exec_name")) != 0) {
				exec_name.Printf(wxT("%s"), prop->value().c_str());
			}
			if ((prop = child_node->property ("jack_name")) != 0) {
				jack_name.Printf(wxT("%s"), prop->value().c_str());
			}
			if ((prop = child_node->property ("jack_serv_name")) != 0) {
				jack_serv_name.Printf(wxT("%s"), prop->value().c_str());
			}
			if ((prop = child_node->property ("midi_bind_path")) != 0) {
			        midi_bind_path.Printf(wxT("%s"), prop->value().c_str());
			}
			if ((prop = child_node->property ("force_spawn")) != 0) {
				if (prop->value() == "yes") {
					force_spawn = true;
				}
				else {
					force_spawn = false;
				}
			}

			break;
		}
	}

	return 0;
}


LoopControl::LoopControl (const wxString & rcdir)
	: _spawn_config(wxT("current")), _default_spawn_config(wxT("default"))
{
	_osc_traffic_thread = 0;
	_osc_addr = 0;
	_midi_bindings = new MidiBindings();
	_rcdir = rcdir;
	
	setup_param_map();
	
	_osc_server = lo_server_new(NULL, NULL);
	if (!_osc_server) return;
	
	char * tmpstr;
	tmpstr = lo_server_get_url (_osc_server);
	_our_url = tmpstr;
	free (tmpstr);
	
	/* add handler for control param callbacks, first is loop index , 2nd arg ctrl string, 3nd arg value */
	lo_server_add_method(_osc_server, "/ctrl", "isf", LoopControl::_control_handler, this);

	// pingack expects: s:engine_url s:version i:loopcount
	lo_server_add_method(_osc_server, "/pingack", "ssi", LoopControl::_pingack_handler, this);

	/* add handler for recving midi bindings, s:status s:serialized binding */
	lo_server_add_method(_osc_server, "/recv_midi_bindings", "ss", LoopControl::_midi_binding_handler, this);
	
	_pingack = false;
	_waiting = 0;
	_failed = false;
	_engine_pid = 0;

	_updatetimer = new LoopUpdateTimer(this);

	init_traffic_thread();
}


LoopControl::~LoopControl()
{
	terminate_traffic_thread();

	disconnect(false);

	if (_osc_server) {
		lo_server_free (_osc_server);
	}
	
	delete _updatetimer;
	delete _midi_bindings;
}

bool
LoopControl::init_traffic_thread()
{
	// thread for watching for new osc traffic

	_traffic_done = false;
	
	if (pipe (_traffic_request_pipe)) {
		cerr << "Cannot create midi request signal pipe" <<  strerror (errno) << endl;
		return false;
	}

	if (fcntl (_traffic_request_pipe[0], F_SETFL, O_NONBLOCK)) {
		cerr << "UI: cannot set O_NONBLOCK on signal read pipe " << strerror (errno) << endl;
		return false;
	}

	if (fcntl (_traffic_request_pipe[1], F_SETFL, O_NONBLOCK)) {
		cerr << "UI: cannot set O_NONBLOCK on signal write pipe " << strerror (errno) << endl;
		return false;
	}
	
	pthread_create (&_osc_traffic_thread, NULL, &LoopControl::_osc_traffic, this);
	if (!_osc_traffic_thread) {
		return false;
	}

	return true;
}

void
LoopControl::terminate_traffic_thread ()
{
	void* status;
	char c = 0;

	_traffic_done = true;

	if (write (_traffic_request_pipe[1], &c, 1) != 1) {
		cerr << "cannot send signal to traffic thread! " <<  strerror (errno) << endl;
	}

	pthread_join (_osc_traffic_thread, &status);

	close(_traffic_request_pipe[0]);
	close(_traffic_request_pipe[1]);
}

void
LoopControl::setup_param_map()
{
	state_map[LooperStateOff] = "off";
	state_map[LooperStateWaitStart] = "waiting start";
	state_map[LooperStateRecording] = "recording";
	state_map[LooperStateWaitStop] = "waiting stop";
	state_map[LooperStatePlaying] = "playing";
	state_map[LooperStateOverdubbing] = "overdubbing";
	state_map[LooperStateMultiplying] = "multiplying";
	state_map[LooperStateInserting] = "inserting";
	state_map[LooperStateReplacing] = "replacing";
	state_map[LooperStateDelay] = "delay";
	state_map[LooperStateMuted] = "muted";
	state_map[LooperStateScratching] = "scratching";
	state_map[LooperStateOneShot] = "one shot";
	state_map[LooperStateSubstitute] = "substituting";
}


bool
LoopControl::connect(char **engine_argv)
{
	_engine_argv = engine_argv;

	if (_osc_addr) {
		lo_address_free (_osc_addr);
	}

	if (_spawn_config.host.empty()) {
		_osc_addr = lo_address_new(NULL, wxString::Format(wxT("%ld"), _spawn_config.port).c_str());
	}
	else {
		_osc_addr = lo_address_new(_spawn_config.host.c_str(), wxString::Format(wxT("%ld"), _spawn_config.port).c_str());
	}
	//cerr << "osc errstr: " << lo_address_errstr(_osc_addr) << endl;

	_pingack = false;
	
	if (!_spawn_config.force_spawn) {
		// send off a ping.  set a timer, if we don't have a response, we'll start our own locally
		_waiting = 0;
		lo_send(_osc_addr, "/ping", "ss", _our_url.c_str(), "/pingack");
		//cerr << "sending ping" << endl;
		_updatetimer->Start(700, true);
	}
	// spawn now
	else if (!_spawn_config.never_spawn && spawn_looper()) {
		//cerr << "immediate spawn" << endl;
		_waiting = 1;
		_updatetimer->Start(100, true);
	}
	else {
		return false;
	}
	
	return true;
}

bool
LoopControl::disconnect (bool killit)
{

	if (_osc_addr) {
		lo_send(_osc_addr, "/unregister", "ss", _our_url.c_str(), "/pingack");
		lo_send(_osc_addr, "/unregister_update", "sss", "tempo", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, "/unregister_update", "sss", "sync_source", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, "/unregister_update", "sss", "eighth_per_cycle", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, "/unregister_update", "sss", "tap_tempo", _our_url.c_str(), "/ctrl");

		if (killit) {
			send_quit();
		}
		
		lo_address_free (_osc_addr);
		_osc_addr = 0;
	}
	_osc_url = wxT("");

	Disconnected(); // emit
	
	return true;
}


void *
LoopControl::_osc_traffic (void *arg)
{
	LoopControl * lc = static_cast<LoopControl *>(arg);
	lc->osc_traffic();
	return 0;
}

void
LoopControl::osc_traffic()
{
	// our only job here in this thread is
	// to send updates when new data comes in on the osc port
	int oscfd = lo_server_get_socket_fd(_osc_server);
	struct pollfd pfd[2];
	int timeout = -1;
	int nfds = 2;
	struct timespec nsleep = { 0, 20000000 };

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	pfd[0].fd = _traffic_request_pipe[0];
	pfd[0].events = POLLIN|POLLHUP|POLLERR;
	pfd[1].fd = oscfd;
	pfd[1].events = POLLIN|POLLHUP|POLLERR;
	
	while (!_traffic_done)
	{
		pfd[0].fd = _traffic_request_pipe[0];
		pfd[0].events = POLLIN|POLLHUP|POLLERR;
		pfd[1].fd = oscfd;
		pfd[1].events = POLLIN|POLLHUP|POLLERR;

		pthread_testcancel();
		
		if (poll (pfd, nfds, timeout) < 0) {
			if (errno == EINTR) {
				/* gdb at work, perhaps */
				// goto again;
			}
			
			cerr << "OSC thread poll failed: " <<  strerror (errno) << endl;
			
			continue;
		}

		if (_traffic_done) {
			break;
		}
		
		if (nfds > 1 && pfd[1].revents & POLLIN) 
		{
			
			// emit signal
			//cerr << "got new data" << endl;
			NewDataReady(); // emit
		}

		// sleep for a bit to give someone the chance to read it
		nanosleep (&nsleep, NULL);

	}
}

bool
LoopControl::is_engine_local()
{
	// compare host parts of osc_url and our_url
	bool ret = false;
	char * remhost = lo_url_get_hostname(_osc_url.c_str());
	char * lochost = lo_url_get_hostname(_our_url.c_str());

	// cerr << "osc: " << _osc_url << "  remhost: " << remhost << "  lochost: " << lochost << endl;
	
	if (remhost && lochost && (strncmp(remhost, lochost, 30) == 0)) {
		ret = true;
	}

	free(remhost);
	free(lochost);

	return ret;
}


void
LoopControl::pingtimer_expired()
{ 
	update_values();

	// check state of pingack

	if (_pingack) {
		// cerr << "got ping response" << endl;
	}
	else if (_waiting > 0)
	{
		if (_waiting > 80) {
			// give up
			cerr << "gave up on spawned engine" << endl;
			if (_osc_addr) {
				lo_address_free(_osc_addr);
			}
			_osc_addr = 0;
			_failed = true;
		}
		else {
			// cerr << "waiting" << endl;
			_waiting++;
			// lo_send(_osc_addr, "/ping", "ss", _our_url.c_str(), "/pingack");
			_updatetimer->Start(100, true);
		}
	}
	else
	{
		// lets try to spawn our own
		cerr << "spawning now" << endl;
		if (!_spawn_config.never_spawn && spawn_looper()) {
			_updatetimer->Start(100, true);
			_waiting = 1;
		}
		else {
			//cerr << "execute failed" << endl;
			if (_osc_addr) {
				lo_address_free(_osc_addr);
			}
			_osc_addr = 0;
			_failed = true;
		}
			
	}
}

bool LoopControl::spawn_looper()
{
	// use wxExecute
	wxString cmdstr = _spawn_config.exec_name;
	char ** argv  = _engine_argv;

	if (cmdstr.empty()) {
		cmdstr = wxT("sooperlooper"); // always force something
	}
	
	cmdstr += wxString::Format(wxT(" -q -U %s -p %ld -l %ld -c %ld -t %d"),
				   _our_url.c_str(),
				   _spawn_config.port,
				   _spawn_config.num_loops,
				   _spawn_config.num_channels,
				   (int) _spawn_config.mem_secs
				   );

	if (!_spawn_config.midi_bind_path.empty()) {
		cmdstr += wxString::Format(wxT(" -m \"%s\""), _spawn_config.midi_bind_path.c_str());
	}
	else {
		// try default
		wxString defpath = (_rcdir + wxFileName::GetPathSeparator() + wxT("default_midi.slb"));
		cmdstr += wxString::Format(wxT(" -m \"%s\""), defpath.c_str());
	}
	
	if (!_spawn_config.jack_name.empty()) {
		cmdstr += wxString::Format(wxT(" -j \"%s\""), _spawn_config.jack_name.c_str());
	}

	if (!_spawn_config.jack_serv_name.empty()) {
		cmdstr += wxString::Format(wxT(" -S \"%s\""), _spawn_config.jack_serv_name.c_str());
	}
	
	if (argv) {
		while (*argv) {
			cmdstr += wxT(" ") + wxString(*argv);
			argv++;
		}
	}


#ifdef DEBUG
	cerr << "execing: '" << cmdstr << "'" << endl;
#endif
	_engine_pid = wxExecute(cmdstr, wxEXEC_ASYNC|wxEXEC_MAKE_GROUP_LEADER);

#ifdef DEBUG
	cerr << "pid is " << _engine_pid << endl;
#endif	
	return _engine_pid > 0;
}

int
LoopControl::_pingack_handler(const char *path, const char *types, lo_arg **argv, int argc,
			      void *data, void *user_data)
{
	LoopControl * lc = static_cast<LoopControl*> (user_data);
	return lc->pingack_handler (path, types, argv, argc, data);
}

int
LoopControl::pingack_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	// s:hosturl  s:version  i:loopcount

	wxString hosturl(&argv[0]->s);
	wxString version(&argv[1]->s);
	int loopcount = argv[2]->i;

	cerr << "remote looper is at " << hosturl << " version=" << version << "   loopcount=" << loopcount << endl;

	_osc_url = hosturl;
	char * remhost = lo_url_get_hostname(_osc_url.c_str());
	_host = remhost;
	free(remhost);
	_spawn_config.host = _host;
	
	char * remport = lo_url_get_port(_osc_url.c_str());
	wxString tmpstr(remport);
	tmpstr.ToLong(&_spawn_config.port);
	free(remport);

	_port = (int) _spawn_config.port;

	if (_osc_addr) {
		lo_address_free(_osc_addr);
	}
	_osc_addr = lo_address_new_from_url (hosturl.c_str());

	if (!_pingack) {
		// register future configs with it once
		lo_send(_osc_addr, "/register", "ss", _our_url.c_str(), "/pingack");

		// results will come back with instance = -2
		lo_send(_osc_addr, "/register_update", "sss", "tempo", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, "/register_update", "sss", "sync_source", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, "/register_update", "sss", "eighth_per_cycle", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, "/register_update", "sss", "tap_tempo", _our_url.c_str(), "/ctrl");

		lo_send(_osc_addr, "/get_all_midi_bindings", "ss", _our_url.c_str(), "/recv_midi_bindings");
		
		_pingack = true;
	}

	
	LooperConnected (loopcount); // emit

	return 0;
}


int
LoopControl::_control_handler(const char *path, const char *types, lo_arg **argv, int argc,
			      void *data, void *user_data)
{
	LoopControl * lc = static_cast<LoopControl*> (user_data);
	return lc->control_handler (path, types, argv, argc, data);
}


int
LoopControl::control_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	// loop instance is 1st arg, 2nd is ctrl string, 3rd is float value

	int  index = argv[0]->i;
	wxString ctrl(&argv[1]->s);
	float val  = argv[2]->f;

	
	if (index == -2)
	{
		// global ctrls
		if (_global_val_map.find(ctrl) == _global_val_map.end()
		    || _global_val_map[ctrl] != val)
		{
			_global_updated[ctrl] = true;
		}
	
		_global_val_map[ctrl] = val;

		return 0;
	}
	else if (index < 0) {
		return 0;
	}
	else if (index >= (int) _params_val_map.size()) {
		_params_val_map.resize(index + 1);
		_updated.resize(index + 1);
	}

	if (_params_val_map[index].find(ctrl) == _params_val_map[index].end()
	    || _params_val_map[index][ctrl] != val)
	{
		_updated[index][ctrl] = true;
	}
	
	_params_val_map[index][ctrl] = val;

	// cerr << "got " << ctrl << " = " << val << "  index=" << index << endl;
	
	return 0;
}

int
LoopControl::_midi_binding_handler(const char *path, const char *types, lo_arg **argv, int argc,
			      void *data, void *user_data)
{
	LoopControl * lc = static_cast<LoopControl*> (user_data);
	return lc->midi_binding_handler (path, types, argv, argc, data);
}

int
LoopControl::midi_binding_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	// s:status s:serialized binding
	string status(&argv[0]->s);
	string bindstr(&argv[1]->s);
	
	MidiBindInfo info;
	
	if (status == "add") {
		if (info.unserialize (bindstr)) {
			_midi_bindings->add_binding(info);
		}
	}
	else if (status == "done") {
		info.unserialize (bindstr);
		MidiBindingChanged(info); // emit
	}
	else if (status == "recv") {
		info.unserialize (bindstr);
		ReceivedNextMidi(info); // emit
	}
	else if (status == "learn_cancel") {
		MidiLearnCancelled(); // emit
	}
	else if (status == "next_cancel") {
		NextMidiCancelled(); // emit
	}
	
	return 0;
}

void
LoopControl::request_global_values()
{
	if (!_osc_addr) return;
	char buf[20];

	snprintf(buf, sizeof(buf), "/get");
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", "tempo", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "sync_source", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "eighth_per_cycle", _our_url.c_str(), "/ctrl");
	//lo_send(_osc_addr, buf, "sss", "tap_tempo", _our_url.c_str(), "/ctrl");

}

void
LoopControl::request_values(int index)
{
	if (!_osc_addr) return;
	char buf[20];

	snprintf(buf, sizeof(buf), "/sl/%d/get", index);
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", "state", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "loop_pos", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "loop_len", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "cycle_len", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "free_time", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "total_time", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "waiting", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "rate_output", _our_url.c_str(), "/ctrl");

}

void
LoopControl::request_all_values(int index)
{
	if (!_osc_addr) return;
	char buf[20];

	snprintf(buf, sizeof(buf), "/sl/%d/get", index);

	request_values(index);
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", "rec_thresh", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "feedback", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_feedback_play", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "dry", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "wet", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "rate", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "scratch_pos", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "delay_trigger", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "quantize", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "fade_samples", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "round", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "sync", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "redo_is_tap", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_rate", _our_url.c_str(), "/ctrl");

}

void
LoopControl::request_all_midi_bindings()
{
	if (!_osc_addr) return;

	_midi_bindings->clear_bindings();
	lo_send(_osc_addr, "/get_all_midi_bindings", "ss", _our_url.c_str(), "/recv_midi_bindings");
}

void
LoopControl::learn_midi_binding(const MidiBindInfo & info, bool exclusive)
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/learn_midi_binding", "ssss", info.serialize().c_str(), exclusive?"exclusive":"",_our_url.c_str(), "/recv_midi_bindings" );
}

void
LoopControl::request_next_midi_event ()
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/get_next_midi_event", "ss", _our_url.c_str(), "/recv_midi_bindings" );
}


void
LoopControl::add_midi_binding(const MidiBindInfo & info, bool exclusive)
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/add_midi_binding", "ss", info.serialize().c_str(), exclusive?"exclusive":"");
}

void
LoopControl::remove_midi_binding(const MidiBindInfo & info)
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/remove_midi_binding", "ss", info.serialize().c_str(),"");
}

void
LoopControl::clear_midi_bindings()
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/clear_midi_bindings", "");
	request_all_midi_bindings();
}

void
LoopControl::cancel_next_midi_event()
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/cancel_get_next_midi", "ss", _our_url.c_str(), "/recv_midi_bindings" );
}

void
LoopControl::cancel_midi_learn()
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/cancel_midi_learn", "ss", _our_url.c_str(), "/recv_midi_bindings" );
}

void
LoopControl::load_midi_bindings(const wxString & filename, bool append)
{
	lo_send(_osc_addr, "/load_midi_bindings", "ss", filename.c_str(), append ? "add": "");

	request_all_midi_bindings();
}

void
LoopControl::save_midi_bindings(const wxString & filename)
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/save_midi_bindings", "ss", filename.c_str(), "");
}

void
LoopControl::register_input_controls(int index, bool unreg)
{
	if (!_osc_addr) return;
	char buf[30];

	if ((int)_params_val_map.size() > index) {
		_params_val_map[index].clear();
		_updated[index].clear();
	}
	
	if (unreg) {
		snprintf(buf, sizeof(buf), "/sl/%d/unregister_update", index);

	} else {
		snprintf(buf, sizeof(buf), "/sl/%d/register_update", index);
	}
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", "rec_thresh", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "feedback", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_feedback_play", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "dry", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "wet", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "rate", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "scratch_pos", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "delay_trigger", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "quantize", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "fade_samples", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "round", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "sync", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "redo_is_tap", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_rate", _our_url.c_str(), "/ctrl");
}

void
LoopControl::register_control (int index, wxString ctrl, bool unreg)
{
	if (!_osc_addr) return;

	char buf[30];

	if ((int)_params_val_map.size() > index) {
		_params_val_map[index].clear();
		_updated[index].clear();
	}
	
	if (unreg) {
		snprintf(buf, sizeof(buf), "/sl/%d/unregister_update", index);

	} else {
		snprintf(buf, sizeof(buf), "/sl/%d/register_update", index);
	}
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", ctrl.c_str(), _our_url.c_str(), "/ctrl");

}


bool
LoopControl::post_down_event(int index, wxString cmd)
{
	if (!_osc_addr) return false;
	char buf[30];

	snprintf(buf, sizeof(buf), "/sl/%d/down", index);
	
	if (lo_send(_osc_addr, buf, "s", cmd.c_str()) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_up_event(int index, wxString cmd, bool force)
{
	if (!_osc_addr) return false;
	char buf[30];

	if (force) {
		snprintf(buf, sizeof(buf), "/sl/%d/upforce", index);
	}
	else {
		snprintf(buf, sizeof(buf), "/sl/%d/up", index);
	}
	
	if (lo_send(_osc_addr, buf, "s", cmd.c_str()) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_ctrl_change (int index, wxString ctrl, float val)
{
	if (!_osc_addr) return false;
	char buf[30];

	snprintf(buf, sizeof(buf), "/sl/%d/set", index);

	if (lo_send(_osc_addr, buf, "sf", ctrl.c_str(), val) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_global_ctrl_change (wxString ctrl, float val)
{
	if (!_osc_addr) return false;

	if (lo_send(_osc_addr, "/set", "sf", ctrl.c_str(), val) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_save_loop(int index, wxString fname, wxString format, wxString endian)
{
	if (!_osc_addr) return false;
	char buf[30];

	snprintf(buf, sizeof(buf), "/sl/%d/save_loop", index);

	// send request for updates
	if (lo_send(_osc_addr, buf, "sssss", fname.c_str(), format.c_str(), endian.c_str(), _our_url.c_str(), "/error") == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_load_loop(int index, wxString fname)
{
	if (!_osc_addr) return false;
	char buf[30];

	snprintf(buf, sizeof(buf), "/sl/%d/load_loop", index);

	// send request for updates
	if (lo_send(_osc_addr, buf, "sss", fname.c_str(), _our_url.c_str(), "/error") == -1) {
		return false;
	}

	return true;
}


void
LoopControl::send_quit()
{
	if (!_osc_addr) return;
	lo_send(_osc_addr, "/quit", NULL);
}


void
LoopControl::update_values()
{
	if (!_osc_server) return;
	// recv commands nonblocking, until none left

	while (lo_server_recv_noblock (_osc_server, 0) > 0)
	{
		// do nothing
	}

}


bool
LoopControl::is_updated (int index, wxString ctrl)
{
	if (index >= 0 && index < (int) _updated.size())
	{
		ControlValMap::iterator iter = _updated[index].find (ctrl);

		if (iter != _updated[index].end()) {
			return (*iter).second;
		}
	}

	return false;
}
	
bool
LoopControl::get_value (int index, wxString ctrl, float & retval)
{
	bool ret = false;
	
	if (index >= 0 && index < (int) _params_val_map.size())
	{
		ControlValMap::iterator iter = _params_val_map[index].find (ctrl);

		if (iter != _params_val_map[index].end()) {
			retval = (*iter).second;
			// set updated to false
			_updated[index][ctrl] = false;
			ret = true;
		}
	}

	return ret;
}


bool
LoopControl::is_global_updated (wxString ctrl)
{
        UpdatedCtrlMap::iterator iter = _global_updated.find (ctrl);
	
	if (iter != _global_updated.end()) {
		return (*iter).second;
	}

	return false;
}

bool
LoopControl::get_global_value (wxString ctrl, float & retval)
{
	bool ret = false;
	
	ControlValMap::iterator iter = _global_val_map.find (ctrl);
	
	if (iter != _global_val_map.end()) {
		retval = (*iter).second;
		// set updated to false
		_global_updated[ctrl] = false;
		ret = true;
	}

	return ret;
}


bool
LoopControl::get_state (int index, LooperState & state, wxString & statestr)
{
	bool ret = false;

	if (index >= 0 && index < (int) _params_val_map.size())
	{
		ControlValMap::iterator iter = _params_val_map[index].find ("state");

		if (iter != _params_val_map[index].end()) {
			state = (LooperState) (*iter).second;
			statestr = state_map[state];
			// set updated to false
			_updated[index]["state"] = false;
			ret = true;
		}
	}

	return ret;
}

bool
LoopControl::post_add_loop(int channels, float secs)
{
	if (!_osc_addr) return false;

	if (lo_send(_osc_addr, "/loop_add", "if", channels, secs) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_remove_loop()
{
	if (!_osc_addr) return false;
	// todo specify loop channels etc
	int index = -1;
	
	if (lo_send(_osc_addr, "/loop_del", "i", index) == -1) {
		return false;
	}
	return true;
}

