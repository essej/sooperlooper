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

#include <sigc++/trackable.h>

#include "event.hpp"
#include "event_nonrt.hpp"

//define timing for auto updates in ms
//having STEP more often than 10ms and a different from the MIN may cause timing problems
#define AUTO_UPDATE_MIN 10
#define AUTO_UPDATE_STEP AUTO_UPDATE_MIN
#define AUTO_UPDATE_MAX 100
#define AUTO_UPDATE_RANGE (((AUTO_UPDATE_MAX - AUTO_UPDATE_MIN)/AUTO_UPDATE_STEP) + 1)

namespace SooperLooper {

class Engine;
class MidiBindings;
class CommandMap;
	
class ControlOSC
	: public sigc::trackable
{
  public:
	
	ControlOSC (Engine *, unsigned int port);
	virtual ~ControlOSC();

	std::string get_server_url();
	int get_server_port () { return _port; }

	std::string get_unix_server_url();
	
	bool is_ok() { return _ok; }

	void send_all_config ();
	void send_pingack (bool useudp, bool use_id, std::string returl, std::string retpath="/pingack");
	
	void send_all_midi_bindings (MidiBindings * bind, std::string returl, std::string retpath);

	void send_auto_updates (const std::list<short int> timeout_list);
	void send_error (std::string returl, std::string retpath, std::string mesg);
	
	void finish_get_event (GetParamEvent & event);
	void finish_update_event (ConfigUpdateEvent & event);
	void finish_register_event (RegisterConfigEvent &event);
	void finish_loop_config_event (ConfigLoopEvent &event);
	void finish_global_get_event (GlobalGetEvent & event);
	void finish_midi_binding_event (MidiBindingEvent & event);
	void finish_get_property_event (GetPropertyEvent & event);
	
	
  private:

	struct CommandInfo
	{
		CommandInfo (ControlOSC * os, int n, Event::type_t t)
			: osc(os), instance(n), type(t) {}
		
		ControlOSC * osc;
		int instance;
		Event::type_t type;
	};

	struct MidiBindCommand
	{
		enum Command {
			GetAllBinding = 1,
			RemoveBinding,
			AddBinding,
			ClearAllBindings,
			LoadBindings,
			SaveBindings,
			LearnBinding,
			GetNextMidi,
			CancelLearn,
			CancelGetNext
		};
		MidiBindCommand(ControlOSC * os, Command cmd)
			: osc(os), command(cmd) {}

		ControlOSC * osc;
		Command command;
	};

	void on_loop_added(int instance, bool sendupdate=true);
	void on_loop_removed();

	void register_callbacks();
	
	lo_address find_or_cache_addr(std::string returl);

	
	static int _quit_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _global_set_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _global_get_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _updown_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _set_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _get_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _set_prop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _get_prop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _dummy_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _register_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _register_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _unregister_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _ping_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _loop_add_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _loop_del_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _load_session_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _save_session_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _register_config_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _unregister_config_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _loadloop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _saveloop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _global_register_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _global_unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _global_register_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _global_unregister_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);

	
	static int _midi_start_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _midi_stop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
	static int _midi_tick_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);

	static int _midi_binding_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);


	bool init_osc_thread();
	void terminate_osc_thread();
	void poke_osc_thread();

	static void * _osc_receiver(void * arg);
	void osc_receiver();
	
	int quit_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int ping_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int global_get_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int global_set_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int loop_add_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int loop_del_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int load_session_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int save_session_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int register_config_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int unregister_config_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);

	int global_register_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int global_unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int global_register_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int global_unregister_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);

	
	int midi_start_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int midi_stop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int midi_tick_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg);
	int midi_binding_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, MidiBindCommand * info);

	
	int updown_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, CommandInfo * info);
	int set_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg,  CommandInfo * info);
	int get_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg,  CommandInfo * info);
	int set_prop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg,  CommandInfo * info);
	int get_prop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg,  CommandInfo * info);
	int register_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg,  CommandInfo * info);
	int unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg,  CommandInfo * info);
	int register_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg,  CommandInfo * info);
	int unregister_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg,  CommandInfo * info);
	int loadloop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg,  CommandInfo * info);
	int saveloop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg,  CommandInfo * info);

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
	volatile bool _ok;
	volatile bool _shutdown;
	int _max_instance;
	
	std::map<std::string, lo_address> _retaddr_map;

	CommandMap * _cmd_map;
	
	typedef std::pair<int, std::string> InstancePair;
	typedef std::pair<lo_address, std::string> UrlPair;
	typedef std::list<UrlPair> UrlList;
	typedef std::map<InstancePair, UrlList > ControlRegistrationMap;
	typedef std::map<InstancePair, float> LastValueMap;
	typedef std::map<UrlPair, LastValueMap> UrlMapToLastValueMap;

	typedef struct {
		UrlPair upair;
		short int timeout;
	} UrlPairAuto;

	typedef std::list<UrlPairAuto> UrlListAuto;
	typedef std::map<InstancePair, UrlListAuto > ControlRegistrationMapAuto;
	
	ControlRegistrationMap _registration_map;
	ControlRegistrationMapAuto _auto_registration_map;

	UrlMapToLastValueMap _last_value_map;

	int compare_auto(UrlPairAuto a, UrlPairAuto b);

	void send_registered_updates(std::string ctrl, float val, int instance, int source=-1);

	bool send_registered_updates(ControlRegistrationMap::iterator & iter,
				     std::string ctrl, float val, int instance, int source=-1);
	bool send_registered_auto_updates(ControlRegistrationMapAuto::iterator & iter,
				    const InstancePair & ipair, float val, const std::list<short int> timeout_list);
	

	
	typedef std::pair<std::string, std::string> AddrPathPair;
	typedef std::list<AddrPathPair> AddressList;
	AddressList _config_registrations;

	void validate_returl(std::string & returl);
};

};  // sooperlooper namespace

#endif
