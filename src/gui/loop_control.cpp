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

#include <sys/poll.h>
#include <unistd.h>
#include <time.h>

#include <midi_bind.hpp>

using namespace std;
using namespace SooperLooperGui;
using namespace SooperLooper;


LoopControl::LoopControl (wxString host, int port, bool force_spawn, wxString execname, char **engine_argv)
{
	_host = host;
	_port = port;
	_force_spawn = force_spawn;
	_exec_name = execname;
	_engine_argv = engine_argv;

	_midi_bindings = new MidiBindings();
	
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

	/* add handler for recving midi bindings, s:serialized binding */
	lo_server_add_method(_osc_server, "/recv_midi_bindings", "s", LoopControl::_midi_binding_handler, this);
	
	if (host.empty()) {
		_osc_addr = lo_address_new(NULL, wxString::Format(wxT("%d"), port).c_str());
	}
	else {
		_osc_addr = lo_address_new(host.c_str(), wxString::Format(wxT("%d"), port).c_str());
	}
	//cerr << "osc errstr: " << lo_address_errstr(_osc_addr) << endl;

	_pingack = false;
	_waiting = 0;
	_failed = false;
	_engine_pid = 0;

	_updatetimer = new LoopUpdateTimer(this);
	
	if (!_force_spawn) {
		// send off a ping.  set a timer, if we don't have a response, we'll start our own locally
		lo_send(_osc_addr, "/ping", "ss", _our_url.c_str(), "/pingack");
		_updatetimer->Start(200, true);
	}
	// spawn now
	else if (spawn_looper()) {
		cerr << "immediate spawn" << endl;
		_waiting = 1;
		_updatetimer->Start(100, true);
	}

	// thread for watching for new osc traffic
	pthread_create (&_osc_traffic_thread, NULL, &LoopControl::_osc_traffic, this);
	if (!_osc_traffic_thread) {
		return;
	}
	pthread_detach(_osc_traffic_thread);
	
}


LoopControl::~LoopControl()
{
	lo_send(_osc_addr, "/unregister", "ss", _our_url.c_str(), "/pingack");
	lo_send(_osc_addr, "/unregister_update", "sss", "tempo", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, "/unregister_update", "sss", "sync_source", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, "/unregister_update", "sss", "eighth_per_cycle", _our_url.c_str(), "/ctrl");

	lo_server_free (_osc_server);
	lo_address_free (_osc_addr);

	delete _updatetimer;
	delete _midi_bindings;
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
	int nfds = 1;
	struct timespec nsleep = { 0, 20000000 };
	
	pfd[0].fd = oscfd;
	pfd[0].events = POLLIN|POLLHUP|POLLERR;
	
	while (1)
	{
		pfd[0].fd = oscfd;
		pfd[0].events = POLLIN|POLLHUP|POLLERR;

		if (poll (pfd, nfds, timeout) < 0) {
			if (errno == EINTR) {
				/* gdb at work, perhaps */
				// goto again;
			}
			
			cerr << "OSC thread poll failed: " <<  strerror (errno) << endl;
			
			continue;
		}
		else {
			// emit signal
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
		if (_waiting > 40) {
			// give up
			cerr << "gave up" << endl;
			_failed = true;
		}
		else {
			_waiting++;
			// lo_send(_osc_addr, "/ping", "ss", _our_url.c_str(), "/pingack");
			_updatetimer->Start(100, true);
		}
	}
	else
	{
		// lets try to spawn our own
		if (spawn_looper()) {
			_updatetimer->Start(100, true);
			_waiting = 1;
		}
		else {
			cerr << "execute failed" << endl;
			_failed = true;
		}
			
	}
}

bool LoopControl::spawn_looper()
{
	// use wxExecute
	wxString cmdstr = _exec_name;
	char ** argv  = _engine_argv;

	cmdstr += wxString::Format(" -q -U %s", _our_url.c_str());
	
	while (*argv) {
		cmdstr += wxT(" ") + wxString(*argv);
		argv++;
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

	lo_address_free(_osc_addr);
	_osc_addr = lo_address_new_from_url (hosturl.c_str());

	if (!_pingack) {
		// register future configs with it once
		lo_send(_osc_addr, "/register", "ss", _our_url.c_str(), "/pingack");

		// results will come back with instance = -2
		lo_send(_osc_addr, "/register_update", "sss", "tempo", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, "/register_update", "sss", "sync_source", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, "/register_update", "sss", "eighth_per_cycle", _our_url.c_str(), "/ctrl");

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
	
	//cerr << "got " << ctrl << " = " << val << "  index=" << index << endl;
	
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
	// s:serialized binding
	string bindstr(&argv[0]->s);
	
	MidiBindInfo info;

	if (info.unserialize (bindstr)) {
		_midi_bindings->add_binding(info);
	}
	
	return 0;
}

void
LoopControl::request_global_values()
{
	char buf[20];

	snprintf(buf, sizeof(buf), "/get");
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", "tempo", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "sync_source", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "eighth_per_cycle", _our_url.c_str(), "/ctrl");

}

void
LoopControl::request_values(int index)
{
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

}

void
LoopControl::request_all_values(int index)
{
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
	lo_send(_osc_addr, buf, "sss", "tap_trigger", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "quantize", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "round", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "sync", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "redo_is_tap", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_rate", _our_url.c_str(), "/ctrl");

}

void
LoopControl::request_all_midi_bindings()
{
	lo_send(_osc_addr, "/get_all_midi_bindings", "ss", _our_url.c_str(), "/recv_midi_bindings");
}

void
LoopControl::add_midi_binding(const MidiBindInfo & info, bool exclusive)
{
	lo_send(_osc_addr, "/add_midi_binding", "ss", info.serialize().c_str(), exclusive?"exclusive":"");
}

void
LoopControl::remove_midi_binding(const MidiBindInfo & info)
{
	lo_send(_osc_addr, "/remove_midi_binding", "ss", info.serialize().c_str(),"");
}

void
LoopControl::register_input_controls(int index, bool unreg)
{
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
	lo_send(_osc_addr, buf, "sss", "tap_trigger", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "quantize", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "round", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "sync", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "redo_is_tap", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_rate", _our_url.c_str(), "/ctrl");
}

void
LoopControl::register_control (int index, wxString ctrl, bool unreg)
{
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
	char buf[30];

	snprintf(buf, sizeof(buf), "/sl/%d/down", index);
	
	if (lo_send(_osc_addr, buf, "s", cmd.c_str()) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_up_event(int index, wxString cmd)
{
	char buf[30];

	snprintf(buf, sizeof(buf), "/sl/%d/up", index);
	
	if (lo_send(_osc_addr, buf, "s", cmd.c_str()) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_ctrl_change (int index, wxString ctrl, float val)
{
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

	if (lo_send(_osc_addr, "/set", "sf", ctrl.c_str(), val) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_save_loop(int index, wxString fname, wxString format, wxString endian)
{
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
	lo_send(_osc_addr, "/quit", NULL);
}


void
LoopControl::update_values()
{
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
LoopControl::post_add_loop()
{
	// todo specify loop channels etc
	int channels = 0;
	float secs = 0.0;
	
	if (lo_send(_osc_addr, "/loop_add", "if", channels, secs) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_remove_loop()
{
	// todo specify loop channels etc
	int index = -1;
	
	if (lo_send(_osc_addr, "/loop_del", "i", index) == -1) {
		return false;
	}
	return true;
}
