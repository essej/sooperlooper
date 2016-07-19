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

#ifndef __sooperlooper_prefs_dialog__
#define __sooperlooper_prefs_dialog__


#include <wx/wx.h>
#include <wx/listbase.h>

#include <string>
#include <vector>
#include <sigc++/trackable.h>

class wxListCtrl;
class wxSpinCtrl;
class wxNotebook;

namespace SooperLooperGui {

class GuiFrame;
class KeyboardTarget;
class ConfigPanel;
class KeysPanel;
class MidiBindPanel;
class LatencyPanel;
	
class PrefsDialog
	: public wxFrame,  public sigc::trackable
{
  public:
	
	// ctor(s)
	PrefsDialog(MainPanel * parent, wxWindowID id, const wxString& title,
		   const wxPoint& pos = wxDefaultPosition,
		   const wxSize& size = wxSize(400,600),
		   long style = wxDEFAULT_FRAME_STYLE,
		   const wxString& name = wxT("PrefsDialog"));

	virtual ~PrefsDialog();

	void refresh_state();

	
   protected:

	void init();

	void on_close (wxCloseEvent &ev);

	wxNotebook * _notebook;
	
	ConfigPanel * _config_panel;
	KeysPanel    * _keys_panel;
	MidiBindPanel * _midi_panel;
	LatencyPanel  * _latency_panel;
	
	MainPanel * _parent;
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};

};

#endif
