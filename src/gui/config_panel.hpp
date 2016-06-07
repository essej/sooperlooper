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

#ifndef __sooperlooper_config_panel__
#define __sooperlooper_config_panel__


#include <wx/wx.h>
#include <wx/listbase.h>

#include <string>
#include <vector>
#include <sigc++/trackable.h>

class wxListCtrl;
class wxSpinCtrl;

namespace SooperLooperGui {

class MainPanel;
class KeyboardTarget;
	
class ConfigPanel
	: public wxPanel,  public sigc::trackable
{
  public:
	
	// ctor(s)
	ConfigPanel(MainPanel * mainpan, wxWindow * parent, wxWindowID id,
		   const wxPoint& pos = wxDefaultPosition,
		   const wxSize& size = wxSize(400,600),
		   long style = wxDEFAULT_FRAME_STYLE,
		   const wxString& name = wxT("ConfigPanel"));

	virtual ~ConfigPanel();

	void refresh_state();
	void refresh_defaults();

	void commit_changes();

	void looper_connected(int);
	
   protected:

	void init();

	void on_button (wxCommandEvent &ev);

	
	wxTextCtrl * _host_text;
	wxTextCtrl * _port_text;
	wxTextCtrl * _status_text;
	wxCheckBox * _force_spawn;
	wxCheckBox * _shutdown_check;
	
	wxButton * _disconnect_button;
	wxButton * _connect_button;

	
	
	wxTextCtrl * _def_host_text;
	wxTextCtrl * _def_port_text;
	wxCheckBox * _def_force_spawn;
	wxTextCtrl * _def_midi_bind_text;
	wxButton   * _midi_browse_button;
	wxTextCtrl * _def_session_text;
	wxButton   * _session_browse_button;
	wxTextCtrl * _def_jack_name_text;
	
	wxSpinCtrl * _num_loops_spin;
	wxSpinCtrl * _num_channels_spin;
	wxSpinCtrl * _secs_per_channel_spin;
	wxCheckBox * _discrete_io_check;
	
	wxButton   * _commit_button;
	
	MainPanel * _parent;
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};

};

#endif
