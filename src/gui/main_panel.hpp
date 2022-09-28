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

#ifndef __sooperlooper_mainpanel__
#define __sooperlooper_mainpanel__


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
	
class MainPanel
	: public wxPanel,  public sigc::trackable
{
public:
	
	// ctor(s)
	MainPanel(wxWindow * parent, wxWindowID id, const wxPoint& pos, const wxSize& size);
	virtual ~MainPanel();
	
	// event handlers (these functions should _not_ be virtual)
	//void OnQuit(wxCommandEvent& event);
	void OnHide(wxCommandEvent &event);
	//void OnClose(wxCloseEvent &event);
	
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

	void do_add_loop(const std::string & type="");
	void do_add_custom_loop();
	void do_remove_loop();

	void do_close(bool quitengine=false);
	void do_load_session ();
	void do_save_session (bool write_audio=false);
	
	void set_force_local(bool flag) { _force_local = flag; }
    bool get_force_local() const { return _force_local; }
    
	void init_loopers (int count);

	bool load_rc();
	bool save_rc();

        wxPoint get_default_position() const { return _default_position; }
        void set_default_position(wxPoint pos) { _default_position = pos; }
    
	void set_never_timeout(bool flag);
	bool get_never_timeout() const { return _never_timeout; }

	void save_default_midibindings ();

	wxString do_file_selector(const wxString & message, const wxString & ext, const wxString & wc, int style);
	
	sigc::signal2<void,int, int> PreferredSizeChange;

    void set_sliders_allow_mousewheel (bool flag);
    bool get_sliders_allow_mousewheel () const { return _sliders_allow_mousewheel; }


protected:

	void init();

	
	void intialize_keybindings ();

	void osc_data_ready();
	
	void on_taptempo_timer(wxTimerEvent &ev);
	
	void on_tempo_change (float value);
	void on_eighth_change (float value);
	void on_xfade_change (float value);
	void on_syncto_change (int index, wxString val);

	void on_quantize_change (int index, wxString val);
	void on_dry_change (float value);
	void on_ingain_change (float value);
	void on_wet_change (float value);
	void on_round_check (bool val);
	void on_relsync_check (bool val);
	void on_mute_quant_check (bool val);
	void on_odub_quant_check (bool val);
	void on_repl_quant_check (bool val);
	void on_smart_eighths_check (bool val);

	void on_bind_request (wxString val);
	
	void on_view_menu (wxCommandEvent &ev);

	void on_about (wxCommandEvent &ev);
	void on_help (wxCommandEvent &ev);

    void on_modal_dialog_close (wxCommandEvent & ev);

    
	void on_taptempo_press (int button);
	void on_taptempo_release (int button);
	
	void init_syncto_choice();
	void update_syncto_choice();
	
	void update_controls();

	void on_connect_failed (const std::string & msg);
	void on_connection_lost (const std::string & msg);
	void on_engine_alive ();
	void on_error_received (const std::string & msg);

	LoopControl * _loop_control;
	sigc::connection  _loop_update_connection;
	sigc::connection  _loop_connect_connection;
	sigc::connection  _loop_disconnect_connection;

    sigc::connection  _connect_failed_connection;
    sigc::connection  _lost_connect_connection;
    sigc::connection  _isalive_connection;
    sigc::connection  _error_recvd_connection;

    
	std::vector<LooperPanel *> _looper_panels;
	
	wxTimer * _update_timer;
	wxTimer * _taptempo_button_timer;

	wxScrolledWindow * _scroller;
	wxBoxSizer * _main_sizer;
	wxBoxSizer * _topsizer;
	wxPanel    * _top_panel;

	SpinBox * _tempo_bar;
	ChoiceBox * _sync_choice;
	SpinBox * _eighth_cycle_bar;
	ChoiceBox * _quantize_choice;
	SliderBar * _common_ingain_bar;
	SliderBar * _common_dry_bar;
	SliderBar * _common_wet_bar;
	SpinBox * _xfade_bar;
	CheckBox * _round_check;
	CheckBox * _relsync_check;
	PixButton * _taptempo_button;
	CheckBox *  _mute_quant_check;
	CheckBox *  _odub_quant_check;
	CheckBox *  _repl_quant_check;
	CheckBox *  _smart_eighths_check;
	float _tapdelay_val;

    bool _force_local;
    
    bool _got_add_custom;
    int _add_num_channels;
    int _add_num_loops;
    float _add_secs_channel;
    bool _add_discrete;
    
    wxPoint _default_position;
    
	// keybindings

	PrefsDialog * _prefs_dialog;
	KeyboardTarget * _keyboard;
	int              _curr_loop;

	HelpWindow *  _help_window;
	
	int     _got_new_data;
	wxString  _rcdir;
	volatile bool _engine_alive;
	bool          _never_timeout;
	int           _update_timer_time;

	wxString      _last_used_path;

    bool _sliders_allow_mousewheel;

private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


	
class AddCustomLoopDialog
	: public wxDialog
{
  public:
	AddCustomLoopDialog (MainPanel * parent=NULL, wxWindowID id=-1, const wxString& title=wxT("Add Custom Loop(s)"),
		       const wxPoint& pos=wxDefaultPosition, const wxSize& size=wxDefaultSize);


	// called by wxOK
	bool TransferDataFromWindow ();

	static int num_loops;
	static int num_channels;
	static float secs_channel;
	static bool discrete;
protected:

	MainPanel * _parent;
	
	wxSpinCtrl * _num_loops_spin;
	wxSpinCtrl * _num_channels_spin;
	wxSpinCtrl * _secs_per_channel_spin;
	wxCheckBox * _discrete_check;
};

	
};

#endif
