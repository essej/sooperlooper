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
	_shutdown = false;
	_osc_server = 0;
	_osc_thread = 0;
	
	for (int j=0; j < 20; ++j) {
		snprintf(tmpstr, sizeof(tmpstr), "%d", _port);

		if ((_osc_server = lo_server_new (tmpstr, error_callback))) {
			break;
		}
		
		cerr << "can't get osc at port: " << _port << endl;
		_port++;
		continue;
	}

	// create new thread to run server
	
	pthread_create (&_osc_thread, NULL, &ControlOSC::_osc_receiver, this);
	
	if (!_osc_thread) {
		return;
	}


	_engine->LoopAdded.connect(slot (*this, &ControlOSC::on_loop_added));
	_engine->LoopRemoved.connect(slot (*this, &ControlOSC::on_loop_removed));

	on_loop_added (-1); // to match all

	/* add method that will match the path /quit with no args */
	lo_server_add_method(_osc_server, "/quit", "", ControlOSC::_quit_handler, this);

	// lo_server_thread_add_method(_sthread, NULL, NULL, ControlOSC::_dummy_handler, this);

	// initialize string maps
	_str_cmd_map["record"]  = Event::RECORD;
	_str_cmd_map["overdub"]  = Event::OVERDUB;
	_str_cmd_map["multiply"]  = Event::MULTIPLY;
	_str_cmd_map["insert"]  = Event::INSERT;
	_str_cmd_map["replace"]  = Event::REPLACE;
	_str_cmd_map["reverse"]  = Event::REVERSE;
	_str_cmd_map["mute"]  = Event::MUTE;
	_str_cmd_map["undo"]  = Event::UNDO;
	_str_cmd_map["redo"]  = Event::REDO;
	_str_cmd_map["scratch"]  = Event::SCRATCH;

	for (map<string, Event::command_t>::iterator iter = _str_cmd_map.begin(); iter != _str_cmd_map.end(); ++iter) {
		_cmd_str_map[(*iter).second] = (*iter).first;
	}

	_str_ctrl_map["rec_thresh"]  = Event::TriggerThreshold;
	_str_ctrl_map["feedback"]  = Event::Feedback;
	_str_ctrl_map["dry"]  = Event::DryLevel;
	_str_ctrl_map["wet"]  = Event::WetLevel;
	_str_ctrl_map["rate"]  = Event::Rate;
	_str_ctrl_map["scratch_pos"]  = Event::ScratchPosition;
	_str_ctrl_map["tap_trigger"]  = Event::TapDelayTrigger;
	_str_ctrl_map["quantize"]  = Event::Quantize;
	_str_ctrl_map["round"]  = Event::Round;
	_str_ctrl_map["redo_is_tap"]  = Event::RedoTap;
	_str_ctrl_map["state"]  = Event::State;
	_str_ctrl_map["loop_len"]  = Event::LoopLength;
	_str_ctrl_map["loop_pos"]  = Event::LoopPosition;
	_str_ctrl_map["cycle_len"]  = Event::CycleLength;
	_str_ctrl_map["free_time"]  = Event::FreeTime;
	_str_ctrl_map["total_time"]  = Event::TotalTime;
	
	for (map<string, Event::control_t>::iterator iter = _str_ctrl_map.begin(); iter != _str_ctrl_map.end(); ++iter) {
		_ctrl_str_map[(*iter).second] = (*iter).first;
	}
	

	_ok = true;
}

ControlOSC::~ControlOSC()
{
	// stop server thread

	_shutdown = true;
	
	// send an event to self
	lo_address addr = lo_address_new_from_url (get_server_url().c_str());
	lo_send(addr, "/ping", "");
	lo_address_free (addr);

	pthread_join (_osc_thread, NULL);
	lo_server_free (_osc_server);
}

void
ControlOSC::on_loop_added (int instance)
{
	char tmpstr[255];
	cerr << "loop added: " << instance << endl;
	
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/down", instance);
	lo_server_add_method(_osc_server, tmpstr, "s", ControlOSC::_updown_handler, new CommandInfo(this, instance, Event::type_cmd_down));
	
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/up", instance);
	lo_server_add_method(_osc_server, tmpstr, "s", ControlOSC::_updown_handler, new CommandInfo(this, instance, Event::type_cmd_up));
	
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/set", instance);
	lo_server_add_method(_osc_server, tmpstr, "sf", ControlOSC::_set_handler, new CommandInfo(this, instance, Event::type_control_change));
	
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/get", instance);
	lo_server_add_method(_osc_server, tmpstr, "sss", ControlOSC::_get_handler, new CommandInfo(this, instance, Event::type_control_request));
	
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

	if (_osc_server) {
		urlstr = lo_server_get_url (_osc_server);
		url = urlstr;
		free (urlstr);
	}
	
	return url;
}


/* server thread */

void *
ControlOSC::_osc_receiver(void * arg)
{
	static_cast<ControlOSC*> (arg)->osc_receiver();
	return 0;
}

void
ControlOSC::osc_receiver()
{
	while (!_shutdown)
	{
		// blocks receiving requests that will be serviced
		// by registered handlers
		lo_server_recv (_osc_server);
	}
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
	_engine->quit();
	return 0;
}

int ControlOSC::updown_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, CommandInfo *info)
{
	// first arg is a string
	
	string cmd(&argv[0]->s);

	_engine->push_command_event(info->type, to_command_t(cmd), info->instance);
	
	return 0;
}


int ControlOSC::set_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, CommandInfo *info)
{

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
	map<string,Event::command_t>::iterator result = _str_cmd_map.find(cmd);

	if (result == _str_cmd_map.end()) {
		return Event::UNKNOWN;
	}

	return (*result).second;
}

std::string  ControlOSC::to_command_str (Event::command_t cmd)
{
	map<Event::command_t, string>::iterator result = _cmd_str_map.find(cmd);

	if (result == _cmd_str_map.end()) {
		return "unknown";
	}

	return (*result).second;
}


Event::control_t
ControlOSC::to_control_t (std::string cmd)
{
	map<string, Event::control_t>::iterator result = _str_ctrl_map.find(cmd);

	if (result == _str_ctrl_map.end()) {
		return Event::Unknown;
	}

	return (*result).second;

	
}

std::string
ControlOSC::to_control_str (Event::control_t cmd)
{
	map<Event::control_t,string>::iterator result = _ctrl_str_map.find(cmd);

	if (result == _ctrl_str_map.end()) {
		return "unknown";
	}

	return (*result).second;
}
