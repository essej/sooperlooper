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

#ifndef __sooperlooper_gui_loop_control__
#define __sooperlooper_gui_loop_control__

#include <wx/string.h>
#include <wx/timer.h>
#include <sigc++/sigc++.h>
#include <map>
#include <vector>
#include <string>

#include <lo/lo.h>

#include <pbd/xml++.h>
#include "plugin.hpp"

namespace SooperLooper {
	class MidiBindings;
	class MidiBindInfo;
};


namespace SooperLooperGui {

class LoopUpdateTimer;	


class LoopControl
	: public SigC::Object
{
  public:

	struct SpawnConfig
	{
		SpawnConfig(const wxString & nm = wxT("default"));

		XMLNode& get_state () const;
		int set_state (const XMLNode&);

		wxString name;
		
		wxString   host;
		long        port;
		long        num_loops; 
		long        num_channels;
		double      mem_secs;
		bool        discrete_io;
		
		wxString   exec_name;
		wxString   midi_bind_path;
		bool       force_spawn;
		bool       never_spawn;
		wxString   jack_name;
		wxString   jack_serv_name;
	};
	
	// ctor(s)
	LoopControl (const wxString & rcdir=wxT(""));
	virtual ~LoopControl();

	SpawnConfig & get_spawn_config() { return _spawn_config; }
	SpawnConfig & get_default_spawn_config() { return _default_spawn_config; }

	bool connect();
	bool disconnect(bool killit=false);
	
	bool post_down_event (int index, wxString cmd);
	bool post_up_event (int index, wxString cmd, bool force=false);

	bool post_ctrl_change (int index, wxString ctrl, float val);
	bool post_global_ctrl_change (wxString ctrl, float val);

	bool post_add_loop(int channels=0, float secs=0.0f, bool discrete=true);
	bool post_remove_loop();

	bool post_save_loop(int index, wxString fname, wxString format=wxT("float"), wxString endian=wxT("little"));
	bool post_load_loop(int index, wxString fname);
	
	bool is_engine_local();
	wxString get_engine_host() { return _host; }
	int get_engine_port() { return _port; }
	bool connected() { return _osc_addr != 0; }
	
	void request_values (int index);
	void request_all_values (int index);
	void request_global_values ();

	void add_midi_binding(const SooperLooper::MidiBindInfo & info, bool exclusive=false);
	void learn_midi_binding(const SooperLooper::MidiBindInfo & info, bool exclusive=false);
	void remove_midi_binding(const SooperLooper::MidiBindInfo & info);
	void request_next_midi_event ();
	void clear_midi_bindings();
	void request_all_midi_bindings();
	void cancel_next_midi_event();
	void cancel_midi_learn();

	void load_midi_bindings(const wxString & filename, bool append=false);
	void save_midi_bindings(const wxString & filename);

	// for read only
	const SooperLooper::MidiBindings & midi_bindings() { return *_midi_bindings; }
	
	void update_values();

	void register_auto_updates(int index, bool unreg=false);
	void register_input_controls(int index, bool unreg=false);
	void register_control (int index, wxString ctrl, bool unreg=false);
	void register_auto_update(int index, wxString ctrl, bool unreg=false);
	
	void send_quit();
	
	bool is_updated (int index, wxString ctrl);
	bool is_global_updated (wxString ctrl);
	
	bool get_global_value (wxString ctrl, float &retval);
	bool get_value (int index, wxString ctrl, float &retval);
	bool get_state (int index, SooperLooper::LooperState & state, wxString & statestr);

	void pingtimer_expired();

	SigC::Signal1<void,int> LooperConnected;
	SigC::Signal0<void> Disconnected;
	SigC::Signal0<void> NewDataReady;
	SigC::Signal1<void, SooperLooper::MidiBindInfo&> MidiBindingChanged;
	SigC::Signal1<void, SooperLooper::MidiBindInfo&> ReceivedNextMidi;
	SigC::Signal0<void> MidiLearnCancelled;
	SigC::Signal0<void> NextMidiCancelled;
	
  protected:
	
	static int _control_handler(const char *path, const char *types, lo_arg **argv, int argc,
				    void *data, void *user_data);

	int control_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data);

	static int _pingack_handler(const char *path, const char *types, lo_arg **argv, int argc,
				    void *data, void *user_data);

	int pingack_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data);

	static int _midi_binding_handler(const char *path, const char *types, lo_arg **argv, int argc,
				    void *data, void *user_data);

	int midi_binding_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data);
	
	static void * _osc_traffic(void * arg);
	void osc_traffic();
	
	void setup_param_map();

	bool spawn_looper();
	
	wxString   _osc_url;
	lo_address _osc_addr;

	lo_server  _osc_server;
	wxString  _our_url;

	typedef std::map<wxString, float> ControlValMap;
	typedef std::vector<ControlValMap> ControlValMapList;
        ControlValMapList _params_val_map;
	ControlValMap     _global_val_map;
	
	std::map<SooperLooper::LooperState, wxString> state_map;

	typedef std::map<wxString, bool> UpdatedCtrlMap;
	typedef std::vector<ControlValMap> UpdatedCtrlMapList;
	UpdatedCtrlMapList _updated;
	UpdatedCtrlMap     _global_updated;

	wxString _host;
	int      _port;
	bool     _force_spawn;
	wxString _exec_name;
	wxString _rcdir;
	
	LoopUpdateTimer * _updatetimer;
	bool _pingack;
	int  _waiting;
	bool _failed;

	long _engine_pid;

	bool init_traffic_thread();
	void terminate_traffic_thread();
	pthread_t _osc_traffic_thread;
	int       _traffic_request_pipe[2];
	bool      _traffic_done;
	
	// midi bindings
	SooperLooper::MidiBindings * _midi_bindings;

	SpawnConfig  _spawn_config;
	SpawnConfig  _default_spawn_config;
	
};

class LoopUpdateTimer : public wxTimer
{
public:
	LoopUpdateTimer(LoopControl *loopctrl): wxTimer(), _loopctrl(loopctrl) {}
	
	LoopControl * _loopctrl;
		
	void Notify() {
		_loopctrl->pingtimer_expired();
	}
};


	

	
};

#endif
