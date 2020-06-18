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

#ifndef __sooperlooper_gui_app__
#define __sooperlooper_gui_app__


#include <wx/wx.h>


namespace SooperLooperGui {


class AppFrame;
class MainPanel;
	
class GuiApp : public wxApp
{
	
  public: 
	// override base class virtuals
	// ----------------------------
	GuiApp();

	virtual ~GuiApp();
	
	// this one is called on application startup and is a good place for the app
	// initialization (doing it here and not in the ctor allows to have an error
	// return: if OnInit() returns false, the application terminates)
	virtual bool OnInit();
	virtual int OnRun();
	
	wxString get_host() { return _host; }
	int get_port() { return _port; }

	bool get_force_spawn() { return _force_spawn; }
	wxString get_exec_name() { return _exec_name; }
	wxChar ** get_engine_args () { return _engine_argv; }
	



	virtual void MacOpenFile(const wxString &fileName);

  protected:
	
	bool  parse_options (int argc, wxChar **argv);

	void process_key_event (wxKeyEvent &ev);

	AppFrame * _frame;

	
	wxString _host;
	int      _port;
	int _show_usage;
	int _show_version;
	wxString _exec_name;
	bool  _force_spawn;
	bool  _never_spawn;
	bool  _never_timeout;
	int  _loop_count;
	int  _channels;
	float _mem_secs;
	wxString _midi_bind_file;
	wxString _server_name;
	wxString _client_name;
	bool  _stay_on_top;
        wxPoint   _screen_pos;
        bool _override_screenpos;
        
        wxString _load_session;
	bool _inited;
	
	wxChar ** _engine_argv;

	DECLARE_EVENT_TABLE()
	
};



};

DECLARE_APP(SooperLooperGui::GuiApp);


#endif
