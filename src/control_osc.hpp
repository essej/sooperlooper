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

#ifndef __sooperlooper_control_osc__
#define __sooperlooper_control_osc__


#include <lo/lo.h>
#include <string>
#include <map>
#include <list>
#include <utility>

#include <sigc++/object.h>

#include "event.hpp"
#include "event_nonrt.hpp"

namespace SooperLooper {

class Engine;
	
class ControlOSC
	: public SigC::Object
{
  public:
	
	ControlOSC (Engine *, unsigned int port);
	virtual ~ControlOSC();

	std::string get_server_url();
	int get_server_port () { return _port; }

	std::string get_unix_server_url();
	
	bool is_ok() { return _ok; }

	void send_all_config ();
	void send_pingack (bool useudp, std::string returl, std::string retpath="/pingack");
	

	void finish_get_event (GetParamEvent & event);
	void finish_update_event (ConfigUpdateEvent & event);
	void finish_register_event (RegisterConfigEvent &event);
	void finish_loop_config_event (ConfigLoopEvent &event);
	void finish_global_get_event (GlobalGetEvent & event);
	
	
  private:

	struct CommandInfo
	{
		CommandInfo (ControlOSC * os, int n, Event::type_t t)
			: osc(os), instance(n), type(t) {}
		
		ControlOSC * osc;
		int instance;
		Event::type_t type;
	};


	void on_loop_added(int instance);
	void on_loop_removed();

	void register_callbacks();
	
	lo_address find_or_cache_addr(std::string returl);

	
	void send_registered_updates(std::string ctrl, float val, int instance);
	
	static int _quit_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _global_set_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _global_get_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _updown_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _set_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _get_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _dummy_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _register_update_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _ping_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _loop_add_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _loop_del_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _register_config_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _unregister_config_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _loadloop_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _saveloop_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _global_register_update_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _global_unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	
	static int _midi_start_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _midi_stop_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);
	static int _midi_tick_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, void *user_data);


	bool init_osc_thread();
	void terminate_osc_thread();
	void poke_osc_thread();

	static void * _osc_receiver(void * arg);
	void osc_receiver();
	
	int quit_handler(const char *path, const char *types, lo_arg **argv, int argc,void *data);
	int ping_handler(const char *path, const char *types, lo_arg **argv, int argc,void *data);
	int global_get_handler(const char *path, const char *types, lo_arg **argv, int argc,void *data);
	int global_set_handler(const char *path, const char *types, lo_arg **argv, int argc,void *data);
	int loop_add_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data);
	int loop_del_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data);
	int register_config_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data);
	int unregister_config_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data);
	int global_register_update_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data);
	int global_unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data);
	
	int midi_start_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data);
	int midi_stop_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data);
	int midi_tick_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data);

	int updown_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data, CommandInfo * info);
	int set_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data,  CommandInfo * info);
	int get_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data,  CommandInfo * info);
	int register_update_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data,  CommandInfo * info);
	int unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data,  CommandInfo * info);
	int loadloop_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data,  CommandInfo * info);
	int saveloop_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data,  CommandInfo * info);

	Event::command_t  to_command_t (std::string cmd);
	std::string       to_command_str (Event::command_t cmd);
	
	Event::control_t  to_control_t (std::string cmd);
	std::string       to_control_str (Event::control_t cmd);
	
	Engine * _engine;

	pthread_t _osc_thread;
	int       _request_pipe[2];
	
	lo_server _osc_server;
	lo_server _osc_unix_server;
	std::string _osc_unix_socket_path;
	
	int _port;
	bool _ok;
	bool _shutdown;

	std::map<std::string, lo_address> _retaddr_map;

	std::map<std::string, Event::command_t> _str_cmd_map;
	std::map<Event::command_t, std::string> _cmd_str_map;

	std::map<std::string, Event::control_t> _str_ctrl_map;
	std::map<Event::control_t, std::string> _ctrl_str_map;

	typedef std::pair<int, std::string> InstancePair;
	typedef std::pair<lo_address, std::string> UrlPair;
	typedef std::list<UrlPair> UrlList;
	typedef std::map<InstancePair, UrlList > ControlRegistrationMap;
	ControlRegistrationMap _registration_map;

	typedef std::pair<std::string, std::string> AddrPathPair;
	typedef std::list<AddrPathPair> AddressList;
	AddressList _config_registrations;
};

};  // sooperlooper namespace

#endif
