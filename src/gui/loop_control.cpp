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
#include "loop_control.hpp"

using namespace std;
using namespace SooperLooperGui;


LoopControl::LoopControl (wxString host, int port, bool force_spawn, wxString execname, char **engine_argv)
{
	_host = host;
	_port = port;
	_force_spawn = force_spawn;
	_exec_name = execname;
	_engine_argv = engine_argv;
	
	setup_param_map();
	
	_osc_server = lo_server_new(NULL, NULL);
	
	_our_url = lo_server_get_url (_osc_server);
	
	/* add handler for control param callbacks, first is loop index , 2nd arg ctrl string, 3nd arg value */
	lo_server_add_method(_osc_server, "/ctrl", "isf", LoopControl::_control_handler, this);

	// pingack expects: s:engine_url s:version i:loopcount
	lo_server_add_method(_osc_server, "/pingack", "ssi", LoopControl::_pingack_handler, this);
	    
	if (host.empty()) {
		_osc_addr = lo_address_new(NULL, wxString::Format(wxT("%d"), port).c_str());
	}
	else {
		_osc_addr = lo_address_new(host.c_str(), wxString::Format(wxT("%d"), port).c_str());
	}


	_pingack = false;
	_waiting = 0;
	_failed = false;
	_engine_pid = 0;

	_updatetimer = new LoopUpdateTimer(this);
	
	if (!_force_spawn) {
		// send off a ping.  set a timer, if we don't have a response, we'll start our own locally
		lo_send(_osc_addr, "/ping", "ss", _our_url.c_str(), "/pingack");
		_updatetimer->Start(100, true);
	}
	// spawn now
	else if (spawn_looper()) {
		_waiting = 1;
		_updatetimer->Start(100, true);
	}

	
}


LoopControl::~LoopControl()
{
	lo_server_free (_osc_server);
	lo_address_free (_osc_addr);
}

void
LoopControl::setup_param_map()
{
	state_map[0] = "off";
	state_map[1] = "waiting start";
	state_map[2] = "recording";
	state_map[3] = "waiting stop";
	state_map[4] = "playing";
	state_map[5] = "overdubbing";
	state_map[6] = "multiplying";
	state_map[7] = "inserting";
	state_map[8] = "replacing";
	state_map[9] = "delay";
	state_map[10] = "muted";
	state_map[11] = "scratching";
	state_map[12] = "one shot";
}


void
LoopControl::pingtimer_expired()
{ 
	update_values();

	// check state of pingack

	if (_pingack) {
		//cerr << "got ping response" << endl;
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

	lo_address_free(_osc_addr);
	_osc_addr = lo_address_new_from_url (hosturl.c_str());
	
	_pingack = true;
	
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

	if (index < 0) {
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
	lo_send(_osc_addr, buf, "sss", "dry", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "wet", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "rate", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "scratch_pos", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "tap_trigger", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "quantize", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "round", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "redo_is_tap", _our_url.c_str(), "/ctrl");

}


void
LoopControl::register_input_controls(int index, bool unreg)
{
	char buf[30];

	if (unreg) {
		snprintf(buf, sizeof(buf), "/sl/%d/unregister_update", index);
	} else {
		snprintf(buf, sizeof(buf), "/sl/%d/register_update", index);
	}
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", "rec_thresh", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "feedback", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "dry", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "wet", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "rate", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "scratch_pos", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "tap_trigger", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "quantize", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "round", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "redo_is_tap", _our_url.c_str(), "/ctrl");
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
LoopControl::get_state (int index, wxString & state)
{
	bool ret = false;

	if (index >= 0 && index < (int) _params_val_map.size())
	{
		ControlValMap::iterator iter = _params_val_map[index].find ("state");

		if (iter != _params_val_map[index].end()) {
			state = state_map[(int) (*iter).second];
			// set updated to false
			_updated[index]["state"] = false;
			ret = true;
		}
	}

	return ret;
}
