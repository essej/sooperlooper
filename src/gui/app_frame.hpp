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

#ifndef __sooperlooper_app_frame__
#define __sooperlooper_app_frame__


#include <wx/wx.h>

#include <string>
#include <vector>

#include <sigc++/trackable.h>
#include <sigc++/signal.h>
#include <sigc++/connection.h>

class wxSpinCtrl;

namespace SooperLooperGui {

class LoopControl;
class LooperPanel;
class SliderBar;
class ChoiceBox;	
class CheckBox;
class SpinBox;
class PixButton;
class KeyboardTarget;
class HelpWindow;
class PrefsDialog;
class MainPanel;
	
class AppFrame
	: public wxFrame,  public sigc::trackable
{
public:
	
	// ctor(s)
	AppFrame(const wxString& title, const wxPoint& pos, const wxSize& size, bool stay_on_top=false, bool embedded=false);
	virtual ~AppFrame();
	
	// event handlers (these functions should _not_ be virtual)
	void OnQuit(wxCommandEvent& event);
	void OnHide(wxCommandEvent &event);
	void OnClose(wxCloseEvent &event);
	
	void OnActivate(wxActivateEvent &ev);
	
	MainPanel * get_main_panel() { return _mainpanel; }

protected:

	void init();

	void on_preferred_size(int w, int h);
	
	void on_add_loop (wxCommandEvent &ev);
	void on_add_custom_loop (wxCommandEvent &ev);
	void on_remove_loop (wxCommandEvent &ev);
	
	void on_view_menu (wxCommandEvent &ev);

	void on_about (wxCommandEvent &ev);
	void on_help (wxCommandEvent &ev);

	void on_load_session (wxCommandEvent &ev);
	void on_save_session (wxCommandEvent &ev);
	void on_save_session_audio (wxCommandEvent &ev);

	void on_timer_event(wxTimerEvent &ev);

	MainPanel * _mainpanel;
	
	wxBoxSizer * _topsizer;

	PrefsDialog * _prefs_dialog;
	HelpWindow *  _help_window;
	
    wxToolBar * _toolbar;
    
	wxString  _rcdir;
    
    bool _embedded;
	
private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


	
	
};

#endif
