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
#include <algorithm>

#include "control_osc.hpp"
#include "event_nonrt.hpp"
#include "engine.hpp"
#include "ringbuffer.hpp"
#include "version.h"

#include <lo/lo.h>
#include <sigc++/sigc++.h>
using namespace SigC;

using namespace SooperLooper;
using namespace std;

static void error_callback(int num, const char *m, const char *path)
{
#ifdef DEBUG
	fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, m);
#endif
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
#ifdef DEBUG		
		cerr << "can't get osc at port: " << _port << endl;
#endif
		_port++;
		continue;
	}

	// create new thread to run server
	
	pthread_create (&_osc_thread, NULL, &ControlOSC::_osc_receiver, this);
	if (!_osc_thread) {
		return;
	}

	pthread_detach (_osc_thread);
	

	_engine->LoopAdded.connect(slot (*this, &ControlOSC::on_loop_added));
	_engine->LoopRemoved.connect(slot (*this, &ControlOSC::on_loop_removed));

	on_loop_added (-1); // to match all

	/* add method that will match the path /quit with no args */
	lo_server_add_method(_osc_server, "/quit", "", ControlOSC::_quit_handler, this);

	// add ping handler:  s:returl s:retpath
	lo_server_add_method(_osc_server, "/ping", "ss", ControlOSC::_ping_handler, this);

	// add loop add handler:  i:channels  i:bytes_per_channel
	lo_server_add_method(_osc_server, "/loop_add", "if", ControlOSC::_loop_add_handler, this);

	// add loop del handler:  i:index 
	lo_server_add_method(_osc_server, "/loop_del", "i", ControlOSC::_loop_del_handler, this);

	// un/register config handler:  s:returl  s:retpath
	lo_server_add_method(_osc_server, "/register", "ss", ControlOSC::_register_config_handler, this);
	lo_server_add_method(_osc_server, "/unregister", "ss", ControlOSC::_unregister_config_handler, this);

	
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
}

void
ControlOSC::on_loop_added (int instance)
{
	// will be called from main event loop

	char tmpstr[255];
#ifdef DEBUG
	cerr << "loop added: " << instance << endl;
#endif	
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/down", instance);
	lo_server_add_method(_osc_server, tmpstr, "s", ControlOSC::_updown_handler, new CommandInfo(this, instance, Event::type_cmd_down));
	
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/up", instance);
	lo_server_add_method(_osc_server, tmpstr, "s", ControlOSC::_updown_handler, new CommandInfo(this, instance, Event::type_cmd_up));
	
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/set", instance);
	lo_server_add_method(_osc_server, tmpstr, "sf", ControlOSC::_set_handler, new CommandInfo(this, instance, Event::type_control_change));
	
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/get", instance);
	lo_server_add_method(_osc_server, tmpstr, "sss", ControlOSC::_get_handler, new CommandInfo(this, instance, Event::type_control_request));

	// register_update args= s:ctrl s:returl s:retpath
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/register_update", instance);
	lo_server_add_method(_osc_server, tmpstr, "sss", ControlOSC::_register_update_handler, new CommandInfo(this, instance, Event::type_control_request));

	// unregister_update args= s:ctrl s:returl s:retpath
	snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/unregister_update", instance);
	lo_server_add_method(_osc_server, tmpstr, "sss", ControlOSC::_unregister_update_handler, new CommandInfo(this, instance, Event::type_control_request));

	send_all_config();
}

void
ControlOSC::on_loop_removed ()
{
	// will be called from main event loop
	send_all_config();
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
	lo_server_free (_osc_server);
	_osc_server = 0;
}



/* STATIC callbacks */


int ControlOSC::_dummy_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
#ifdef DEBUG
	cerr << "got path: " << path << endl;
#endif
	return 0;
}


int ControlOSC::_quit_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->quit_handler (path, types, argv, argc, data);

}

int ControlOSC::_ping_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->ping_handler (path, types, argv, argc, data);

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

int ControlOSC::_register_update_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->register_update_handler (path, types, argv, argc, data, cp);
}

int ControlOSC::_unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->unregister_update_handler (path, types, argv, argc, data, cp);
}

int ControlOSC::_loop_add_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->loop_add_handler (path, types, argv, argc, data);
}

int ControlOSC::_loop_del_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->loop_del_handler (path, types, argv, argc, data);
}

int ControlOSC::_register_config_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->register_config_handler (path, types, argv, argc, data);
}

int ControlOSC::_unregister_config_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->unregister_config_handler (path, types, argv, argc, data);
}

/* real callbacks */

int ControlOSC::quit_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	_engine->quit();
	return 0;
}


int ControlOSC::ping_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	string returl (&argv[0]->s);
	string retpath (&argv[1]->s);

	_engine->push_nonrt_event ( new PingEvent (returl, retpath));
	
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


	// send out updates to registered in main event loop
	_engine->push_nonrt_event ( new ConfigUpdateEvent (ConfigUpdateEvent::Send, info->instance, to_control_t(ctrl), "", "", val));
	
	return 0;

}

int ControlOSC::loop_add_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	// 1st is an int #channels
	// 2nd is a float #bytes per channel (if 0, use engine default) 
	
	int channels = argv[0]->i;
	float secs = argv[1]->f;

	_engine->push_nonrt_event ( new ConfigLoopEvent (ConfigLoopEvent::Add, channels, secs, 0));
	
	return 0;
}

int ControlOSC::loop_del_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	// 1st is index of loop to delete
	
	int index = argv[0]->i;

	_engine->push_nonrt_event ( new ConfigLoopEvent (ConfigLoopEvent::Remove, 0, 0.0f, index));

	return 0;
}


lo_address
ControlOSC::find_or_cache_addr(string returl)
{
	lo_address addr = 0;
	
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
	
	return addr;
}

int ControlOSC::get_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, CommandInfo *info)
{
	// cerr << "get " << path << endl;

	// first arg is control string, 2nd is return URL string 3rd is retpath
	string ctrl (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new GetParamEvent (info->instance, to_control_t(ctrl), returl, retpath));
	
	return 0;
}

int ControlOSC::register_update_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, CommandInfo *info)
{
	// first arg is control string, 2nd is return URL string 3rd is retpath
	string ctrl (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new ConfigUpdateEvent (ConfigUpdateEvent::Register, info->instance, to_control_t(ctrl), returl, retpath));
	
	return 0;
}

int ControlOSC::unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, CommandInfo *info)
{

	// first arg is control string, 2nd is return URL string 3rd is retpath
	string ctrl (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new ConfigUpdateEvent (ConfigUpdateEvent::Unregister, info->instance, to_control_t(ctrl), returl, retpath));
	
	return 0;
}

int
ControlOSC::register_config_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	// 1st is return URL string 2nd is retpath
	string returl (&argv[0]->s);
	string retpath (&argv[1]->s);

	_engine->push_nonrt_event ( new RegisterConfigEvent (RegisterConfigEvent::Register, returl, retpath));
	
	return 0;
}

int
ControlOSC::unregister_config_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	// 1st is return URL string 2nd is retpath
	string returl (&argv[0]->s);
	string retpath (&argv[1]->s);

	_engine->push_nonrt_event ( new RegisterConfigEvent (RegisterConfigEvent::Unregister, returl, retpath));
	return 0;
}


void
ControlOSC::finish_get_event (GetParamEvent & event)
{
	// called from the main event loop (not osc thread)
	string ctrl (to_control_str(event.control));
	string returl (event.ret_url);
	string retpath (event.ret_path);
	lo_address addr;

	addr = find_or_cache_addr (returl);
	if (!addr) {
		return;
	}
	
//	 cerr << "sending to " << returl << "  path: " << retpath << "  ctrl: " << ctrl << "  val: " <<  event.ret_value << endl;

	if (lo_send(addr, retpath.c_str(), "isf", event.instance, ctrl.c_str(), event.ret_value) == -1) {
		fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
	}
	
}

void ControlOSC::finish_update_event (ConfigUpdateEvent & event)
{
	// called from the main event loop (not osc thread)

	lo_address addr;
	string retpath = event.ret_path;
	string returl  = event.ret_url;
	string ctrl    = to_control_str (event.control);
	
	if (event.type == ConfigUpdateEvent::Send)
	{
		if (event.instance == -1) {
			for (unsigned int i = 0; i < _engine->loop_count_unsafe(); ++i) {
				send_registered_updates (ctrl, event.value, (int) i);
			}
		} else {
			send_registered_updates (ctrl, event.value, event.instance);
		}

	}
	else if (event.type == ConfigUpdateEvent::Register)
	{
		if ((addr = find_or_cache_addr (returl)) == 0) {
			return;
		}
				
		// add this to register_ctrl map
		InstancePair ipair(event.instance, ctrl);
		ControlRegistrationMap::iterator iter = _registration_map.find (ipair);
		
		if (iter == _registration_map.end()) {
			_registration_map[ipair] = UrlList();
			iter = _registration_map.find (ipair);
		}
		
		UrlList & ulist = (*iter).second;
		UrlPair upair(addr, retpath);
		
		if (find(ulist.begin(), ulist.end(), upair) == ulist.end()) {
#ifdef DEBUG
			cerr << "registered " << ctrl << "  " << returl << endl;
#endif
			ulist.push_back (upair);
		}
		
		
	}
	else if (event.type == ConfigUpdateEvent::Unregister) {

		if ((addr = find_or_cache_addr (returl)) == 0) {
			return;
		}
		
		// add this to register_ctrl map
		InstancePair ipair(event.instance, ctrl);
		ControlRegistrationMap::iterator iter = _registration_map.find (ipair);
		
		if (iter != _registration_map.end()) {
			UrlList & ulist = (*iter).second;
			UrlPair upair(addr, retpath);
			UrlList::iterator uiter = find(ulist.begin(), ulist.end(), upair);
			
			if (uiter != ulist.end()) {
#ifdef DEBUG
				cerr << "unregistered " << ctrl << "  " << returl << endl;
#endif
				ulist.erase (uiter);
			}
		}
		

	}
}


void
ControlOSC::send_registered_updates(string ctrl, float val, int instance)
{
	InstancePair ipair(instance, ctrl);
	ControlRegistrationMap::iterator iter = _registration_map.find (ipair);

	if (iter != _registration_map.end()) {
		UrlList & ulist = (*iter).second;

		for (UrlList::iterator url = ulist.begin(); url != ulist.end(); ++url)
		{
		        lo_address addr = (*url).first;
			
			if (lo_send(addr, (*url).second.c_str(), "isf", instance, ctrl.c_str(), val) == -1) {
				fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
			}
		}
	}
	else {
#ifdef DEBUG
		cerr << "not in map" << endl;
#endif
	}
}


void
ControlOSC::finish_register_event (RegisterConfigEvent &event)
{
	AddrPathPair apair(event.ret_url, event.ret_path);
	AddressList::iterator iter = find (_config_registrations.begin(), _config_registrations.end(), apair);

	if (iter == _config_registrations.end()) {
		_config_registrations.push_back (apair);
	}
}

void ControlOSC::send_all_config ()
{
	// for now just send pingacks to all registered addresses
	for (AddressList::iterator iter = _config_registrations.begin(); iter != _config_registrations.end(); ++iter)
	{
		send_pingack ((*iter).first, (*iter).second);
	}
}


void ControlOSC::send_pingack (string returl, string retpath)
{
	lo_address addr;

	addr = find_or_cache_addr (returl);
	if (!addr) {
		return;
	}
	
	// cerr << "sending to " << returl << "  path: " << retpath << "  ctrl: " << ctrl << "  val: " <<  val << endl;
	if (lo_send(addr, retpath.c_str(), "ssi", get_server_url().c_str(), sooperlooper_version, _engine->loop_count_unsafe()) == -1) {
		fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
	}
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


