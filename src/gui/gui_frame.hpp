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

#ifndef __sooperlooper_gui_frame__
#define __sooperlooper_gui_frame__


#include <wx/wx.h>

#include <string>
#include <vector>

#include <sigc++/object.h>
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
	
class GuiFrame
	: public wxFrame,  public SigC::Object
{
public:
	
	// ctor(s)
	GuiFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	virtual ~GuiFrame();
	
	// event handlers (these functions should _not_ be virtual)
	void OnQuit(wxCommandEvent& event);
	void OnHide(wxCommandEvent &event);
	void OnClose(wxCloseEvent &event);
	
	void OnSize(wxSizeEvent & event);
	void OnPaint(wxPaintEvent & event);
	
	void OnIdle(wxIdleEvent& event);
	void OnUpdateTimer(wxTimerEvent &ev);
	void OnActivate(wxActivateEvent &ev);
	
	void process_key_event (wxKeyEvent &ev);


	KeyboardTarget & get_keyboard() { return *_keyboard; }

	LoopControl & get_loop_control() { return *_loop_control; }
	
	void command_action (bool release, wxString cmd);
	void misc_action (bool release, wxString cmd);
	void select_loop_action (bool release, int index);

	void set_curr_loop (int index);


	void init_loopers (int count);

	bool load_rc();
	bool save_rc();
	
protected:

	void init();

	
	void intialize_keybindings ();
	void save_default_midibindings ();

	void osc_data_ready();
	
	void on_add_loop (wxCommandEvent &ev);
	void on_add_custom_loop (wxCommandEvent &ev);
	void on_remove_loop (wxCommandEvent &ev);

	void on_taptempo_timer(wxTimerEvent &ev);
	
	void on_tempo_change (float value);
	void on_eighth_change (float value);
	void on_xfade_change (float value);
	void on_syncto_change (int index, wxString val);

	void on_quantize_change (int index, wxString val);
	//void on_round_check (wxCommandEvent &ev);
	void on_round_check (bool val);

	void on_bind_request (wxString val);
	
	void on_view_menu (wxCommandEvent &ev);

	void on_about (wxCommandEvent &ev);
	void on_help (wxCommandEvent &ev);
	
	void on_taptempo_press (int button);
	void on_taptempo_release (int button);
	
	void init_syncto_choice();
	void update_syncto_choice();
	
	void update_controls();

	
	LoopControl * _loop_control;
	SigC::Connection  _loop_update_connection;
	
	std::vector<LooperPanel *> _looper_panels;
	
	wxTimer * _update_timer;
	wxTimer * _taptempo_button_timer;

	wxScrolledWindow * _scroller;
	wxBoxSizer * _main_sizer;
	wxBoxSizer * _topsizer;

	SpinBox * _tempo_bar;
	ChoiceBox * _sync_choice;
	SpinBox * _eighth_cycle_bar;
	ChoiceBox * _quantize_choice;
	//wxCheckBox * _round_check;
	SpinBox * _xfade_bar;
	CheckBox * _round_check;
	PixButton * _taptempo_button;
	float _tapdelay_val;

	// keybindings

	PrefsDialog * _prefs_dialog;
	KeyboardTarget * _keyboard;
	int              _curr_loop;

	HelpWindow *  _help_window;
	
	int     _got_new_data;
	wxString  _rcdir;
	
private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


	
class AddCustomLoopDialog
	: public wxDialog
{
  public:
	AddCustomLoopDialog (GuiFrame * parent=NULL, wxWindowID id=-1, const wxString& title=wxT("Add Custom Loop(s)"),
		       const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize);


	// called by wxOK
	bool TransferDataFromWindow ();

	static int num_loops;
	static int num_channels;
	static float secs_channel;
	
protected:

	GuiFrame * _parent;
	
	wxSpinCtrl * _num_loops_spin;
	wxSpinCtrl * _num_channels_spin;
	wxSpinCtrl * _secs_per_channel_spin;
	
};

	
};

#endif
