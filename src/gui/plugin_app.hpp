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

#ifndef __sooperlooper_plugin_app__
#define __sooperlooper_plugin_app__


#include <wx/wx.h>


namespace SooperLooperGui {


class MainPanel;
	
class PluginApp : public wxApp
{
	
  public: 
	// override base class virtuals
	// ----------------------------
	PluginApp();

	virtual ~PluginApp();
	
	// this one is called on application startup and is a good place for the app
	// initialization (doing it here and not in the ctor allows to have an error
	// return: if OnInit() returns false, the application terminates)
	virtual bool OnInit();
	
	virtual int OnRun();
	
    void cleanup_stuff();
    
	MainPanel * get_main_panel() {return _mainpanel; }
	wxPanel * get_top_panel() {return _toppanel; }

	wxFrame * get_main_frame() { return _frame; }
	

    wxString get_host() { return _host; }
    void set_host(wxString host) { _host = host;}
    
	int get_port() const { return _port; }
    void set_port(int port) { _port = port; }
    
    void set_stay_on_top(bool flag) { _stay_on_top = flag; }
    bool get_stay_on_top() const { return _stay_on_top; }
    
	bool get_force_spawn() { return _force_spawn; }
	wxString get_exec_name() { return _exec_name; }
	wxChar ** get_engine_args () { return _engine_argv; }
	
  protected:
	
	bool  parse_options (int argc, wxChar **argv);

	void process_key_event (wxKeyEvent &ev);

    void * _externalView;
    
	MainPanel * _mainpanel;
	wxFrame * _frame;
	wxPanel * _toppanel;
    
	wxString _host;
	int      _port;
	int _show_usage;
	int _show_version;
	wxString _exec_name;
	bool  _force_spawn;
	bool  _never_spawn;
	int  _loop_count;
	int  _channels;
	float _mem_secs;
	wxString _midi_bind_file;
	wxString _server_name;
	wxString _client_name;
	
    bool _stay_on_top;
    
	wxChar ** _engine_argv;

	// DECLARE_EVENT_TABLE()
	
};



};

//DECLARE_APP(SooperLooperGui::PluginApp);


#endif
