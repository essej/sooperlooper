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

#include <iostream>
#include <cstdio>

#include "control_osc.hpp"
#include "engine.hpp"
#include "ringbuffer.hpp"

#include <lo/lo.h>
#include <sigc++/sigc++.h>
using namespace SigC;

using namespace SooperLooper;
using namespace std;

static void error_callback(int num, const char *m, const char *path)
{
	fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, m);
}



ControlOSC::ControlOSC(Engine * eng, unsigned int port)
	: _engine(eng), _port(port)
{
	char tmpstr[255];

	_ok = false;
	
	for (int j=0; j < 20; ++j) {
		snprintf(tmpstr, sizeof(tmpstr), "%d", _port);
		
		if ((_sthread = lo_server_thread_new(tmpstr, error_callback))) {
			break;
		}

		cerr << "can't get osc at port: " << _port << endl;
		_port++;
		continue;
	}

	if (!_sthread) {
		return;
	}


	_engine->LoopAdded.connect(slot (*this, &ControlOSC::on_loop_added));
	_engine->LoopRemoved.connect(slot (*this, &ControlOSC::on_loop_removed));

	on_loop_added (-1); // to match all

	/* add method that will match the path /quit with no args */
	lo_server_thread_add_method(_sthread, "/quit", "", ControlOSC::_quit_handler, this);

	// lo_server_thread_add_method(_sthread, NULL, NULL, ControlOSC::_dummy_handler, this);
			      
	
	
	lo_server_thread_start(_sthread);

	_ok = true;
}

ControlOSC::~ControlOSC()
{
	// stop server thread??  how?

// 	if (_sthread) {
// 		// this will block
// 		lo_server_thread_stop (_sthread);
// 	}
}

void
ControlOSC::on_loop_added (int instance)
{
	char tmpstr[255];
	cerr << "loop added: " << instance << endl;
	
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/down", instance);
	lo_server_thread_add_method(_sthread, tmpstr, "s", ControlOSC::_updown_handler, new CommandInfo(this, instance, Event::type_cmd_down));
	
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/up", instance);
	lo_server_thread_add_method(_sthread, tmpstr, "s", ControlOSC::_updown_handler, new CommandInfo(this, instance, Event::type_cmd_up));
	
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/set", instance);
	lo_server_thread_add_method(_sthread, tmpstr, "sf", ControlOSC::_set_handler, new CommandInfo(this, instance, Event::type_control_change));
	
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/get", instance);
	lo_server_thread_add_method(_sthread, tmpstr, "sss", ControlOSC::_get_handler, new CommandInfo(this, instance, Event::type_control_request));
	
}

void
ControlOSC::on_loop_removed ()
{
	

}


std::string
ControlOSC::get_server_url()
{
	string url;
	char * urlstr;

	if (_sthread) {
		urlstr = lo_server_thread_get_url (_sthread);
		url = urlstr;
		free (urlstr);
	}
	
	return url;
}



/* STATIC callbacks */

int ControlOSC::_dummy_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
	cerr << "got path: " << path << endl;
	return 0;
}


int ControlOSC::_quit_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->quit_handler (path, types, argv, argc, data);

}

int ControlOSC::_updown_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->updown_handler (path, types, argv, argc, data, cp);
}

int ControlOSC::_set_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->set_handler (path, types, argv, argc, data, cp);
}

int ControlOSC::_get_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->get_handler (path, types, argv, argc, data, cp);
}


/* real callbacks */

int ControlOSC::quit_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	cerr << "got quit!" << endl;
	_engine->quit();
	return 0;
}

int ControlOSC::updown_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, CommandInfo *info)
{
	cerr << "updown " << path << "  " << info->type << endl;

	// first arg is a string
	
	string cmd(&argv[0]->s);

	_engine->push_command_event(info->type, to_command_t(cmd), info->instance);
	
	return 0;
}


int ControlOSC::set_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, CommandInfo *info)
{
	cerr << "set " << path << endl;

	// first arg is a control string, 2nd is float val

	string ctrl(&argv[0]->s);
	float val  = argv[1]->f;

	_engine->push_control_event(info->type, to_control_t(ctrl), val, info->instance);
	
	return 0;

}

int ControlOSC::get_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, CommandInfo *info)
{
	// cerr << "get " << path << endl;

	// todo, push this onto a queue for another thread to return

	// first arg is control string, 2nd is return URL string 3rd is retpath
	string ctrl (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);
	lo_address addr;
		
	if (_retaddr_map.find(returl) == _retaddr_map.end()) {
		addr = lo_address_new_from_url (returl.c_str());
		if (lo_address_errno (addr) < 0) {
			fprintf(stderr, "addr error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
		}
		_retaddr_map[returl] = addr;
	}
	else {
		addr = _retaddr_map[returl];
	}
	

	
	float val = _engine->get_control_value (to_control_t(ctrl), info->instance);

	// cerr << "sending to " << returl << "  path: " << retpath << "  ctrl: " << ctrl << "  val: " <<  val << endl;
	if (lo_send(addr, retpath.c_str(), "sf", ctrl.c_str(), val) == -1) {
		fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
	}
	
	return 0;
}


Event::command_t  ControlOSC::to_command_t (std::string cmd)
{
	if (cmd == "record") {
	        return Event::RECORD;
	}
	else if (cmd == "overdub") {
		return Event::OVERDUB;
	}
	else if (cmd == "multiply") {
		return Event::MULTIPLY;
	}
	else if (cmd == "insert") {
		return Event::INSERT;
	}
	else if (cmd == "replace") {
		return Event::REPLACE;
	}
	else if (cmd == "reverse") {
		return Event::REVERSE;
	}
	else if (cmd == "mute") {
		return Event::MUTE;
	}
	else if (cmd == "undo") {
		return Event::UNDO;
	}
	else if (cmd == "redo") {
		return Event::REDO;
	}
	else if (cmd == "scratch") {
		return Event::SCRATCH;
	}
	else {
		return Event::UNKNOWN;
	}
}

std::string  ControlOSC::to_command_str (Event::command_t cmd)
{
	switch (cmd)
	{
	case Event::RECORD:
		return "record";
	case Event::OVERDUB:
		return "overdub";
	case Event::MULTIPLY:
		return "multiply";
	case Event::INSERT:
		return "insert";
	case Event::REPLACE:
		return "replace";
	case Event::REVERSE:
		return "reverse";
	case Event::MUTE:
		return "mute";
	case Event::UNDO:
		return "undo";
	case Event::REDO:
		return "redo";
	case Event::SCRATCH:
		return "scratch";

	default:
		return "unknown";
	}
}


Event::control_t
ControlOSC::to_control_t (std::string cmd)
{
	if (cmd == "rec_thresh") {
	        return Event::TriggerThreshold;
	}
	else if (cmd == "feedback") {
		return Event::Feedback;
	}
	else if (cmd == "dry") {
		return Event::DryLevel;
	}
	else if (cmd == "wet") {
		return Event::WetLevel;
	}
	else if (cmd == "rate") {
		return Event::Rate;
	}
	else if (cmd == "scratch_pos") {
		return Event::ScratchPosition;
	}
	else if (cmd == "tap_trigger") {
		return Event::TapDelayTrigger;
	}
	else if (cmd == "quantize") {
		return Event::Quantize;
	}
	else if (cmd == "round") {
		return Event::Round;
	}
	else if (cmd == "redo_is_tap") {
		return Event::RedoTap;
	}
	// the output ones
	else if (cmd == "state") {
		return Event::State;
	}
	else if (cmd == "loop_len") {
		return Event::LoopLength;
	}
	else if (cmd == "loop_pos") {
		return Event::LoopPosition;
	}
	else if (cmd == "cycle_len") {
		return Event::CycleLength;
	}
	else if (cmd == "free_time") {
		return Event::FreeTime;
	}
	else if (cmd == "total_time") {
		return Event::TotalTime;
	}
	
}

std::string
ControlOSC::to_control_str (Event::control_t cmd)
{
	switch (cmd)
	{
	case Event::TriggerThreshold:
		return "rec_thresh";
	case Event::DryLevel:
		return "dry";
	case Event::WetLevel:
		return "wet";
	case Event::Feedback:
		return "feedback";
	case Event::Rate:
		return "rate";
	case Event::ScratchPosition:
		return "scratch_pos";
	case Event::TapDelayTrigger:
		return "tap_trigger";
	case Event::Quantize:
		return "quantize";
	case Event::Round:
		return "round";
	case Event::RedoTap:
		return "redo_is_tap";

	case Event::State:
		return "state";
	case Event::LoopLength:
		return "loop_len";
	case Event::LoopPosition:
		return "loop_pos";
	case Event::CycleLength:
		return "cycle_len";
	case Event::FreeTime:
		return "free_time";
	case Event::TotalTime:
		return "total_time";

	default:
		return "unknown";
	}
}
