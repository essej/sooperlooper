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

#include <wx/wx.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/utils.h>
#include <wx/dir.h>
#include <wx/spinctrl.h>
#include <wx/splash.h>

#include <iostream>
#include <cstdio>
#include <cmath>

#include "version.h"

#include "main_panel.hpp"
#include "gui_app.hpp"
#include "looper_panel.hpp"
#include "loop_control.hpp"
#include "slider_bar.hpp"
#include "choice_box.hpp"
#include "check_box.hpp"
#include "spin_box.hpp"
#include "pix_button.hpp"
#include "keyboard_target.hpp"
#include "help_window.hpp"
#include "prefs_dialog.hpp"

#include "pixmaps/sl_logo.xpm"
#include "pixmaps/tap_tempo_active.xpm"
#include "pixmaps/tap_tempo_disabled.xpm"
#include "pixmaps/tap_tempo_focus.xpm"
#include "pixmaps/tap_tempo_normal.xpm"
#include "pixmaps/tap_tempo_selected.xpm"

#include <midi_bind.hpp>

#include <pbd/xml++.h>

using namespace SooperLooper;
using namespace SooperLooperGui;
using namespace std;

enum {
	ID_UpdateTimer = 9000,
	ID_AboutMenu,
	ID_HelpTipsMenu,
	ID_PreferencesMenu,
	ID_ConnectionMenu,
	ID_KeybindingsMenu,
	ID_MidiBindingsMenu,
	ID_LoadSession,
	ID_SaveSession,
	ID_Quit,
	ID_QuitStop,
	ID_AddLoop,
	ID_AddMonoLoop,
	ID_AddStereoLoop,
	ID_RemoveLoop,
	ID_TempoSlider,
	ID_SyncChoice,
	ID_EighthSlider,
	ID_QuantizeChoice,
	ID_RoundCheck,
	ID_RelSyncCheck,
	ID_TapTempoButton,
	ID_TapTempoTimer,
	ID_AddCustomLoop,
	ID_XfadeSlider,
	ID_DryControl,
	ID_WetControl,
	ID_InGainControl,
	ID_MuteQuantCheck,
	ID_OdubQuantCheck,
	ID_SmartEighthCheck,
	ID_ReplQuantCheck,
	ID_GlobalCyclePos
};


BEGIN_EVENT_TABLE(MainPanel, wxPanel)

	EVT_IDLE(MainPanel::OnIdle)
	EVT_SIZE(MainPanel::OnSize)
	EVT_PAINT(MainPanel::OnPaint)
	EVT_TIMER(ID_UpdateTimer, MainPanel::OnUpdateTimer)
	EVT_TIMER(ID_TapTempoTimer, MainPanel::on_taptempo_timer)

    EVT_ACTIVATE (MainPanel::OnActivate)
	EVT_ACTIVATE_APP (MainPanel::OnActivate)
	
	
END_EVENT_TABLE()

	MainPanel::MainPanel(wxWindow * parent, wxWindowID id, const wxPoint& pos, const wxSize& size)
	: wxPanel(parent, id, pos, size)

{
	_keyboard = new KeyboardTarget (this, "gui_frame");
	_curr_loop = -1;
	_tapdelay_val = 1.0f;
	_prefs_dialog = 0;
	_got_new_data = 0;
	_help_window = 0;
	_engine_alive = true;
	_never_timeout = false;
	_update_timer_time = 11000; // ms
    _force_local = false;
    _got_add_custom = false;
    _add_num_loops = 1;
    _add_num_channels = 1;
    _add_discrete = true;
    _add_secs_channel = 40.0f;

    _default_position.x = 100;    
    _default_position.y = 100;

    _sliders_allow_mousewheel = false;
    SliderBar::set_use_mousewheel_default(_sliders_allow_mousewheel);    
    SpinBox::set_use_mousewheel_default(_sliders_allow_mousewheel);
    ChoiceBox::set_use_mousewheel_default(_sliders_allow_mousewheel);

	_rcdir = wxGetHomeDir() + wxFileName::GetPathSeparator() + wxT(".sooperlooper");

	_loop_control = new LoopControl(_rcdir);

	
	intialize_keybindings ();
		
	load_rc();
	
	init();

	_update_timer = new wxTimer(this, ID_UpdateTimer);
	_update_timer->Start(_update_timer_time, true);

	_taptempo_button_timer = new wxTimer(this, ID_TapTempoTimer);

	_connect_failed_connection = _loop_control->ConnectFailed.connect (mem_fun (*this,  &MainPanel::on_connect_failed));
	_lost_connect_connection = _loop_control->LostConnection.connect (mem_fun (*this,  &MainPanel::on_connection_lost));
	_isalive_connection = _loop_control->IsAlive.connect (mem_fun (*this,  &MainPanel::on_engine_alive));
	_error_recvd_connection =  _loop_control->ErrorReceived.connect (mem_fun (*this,  &MainPanel::on_error_received));
	
}

MainPanel::~MainPanel()
{
    //wxLogWarning(wxT("Mainpanel destroy"));
	save_rc();
	
	for (unsigned int i=0; i < _looper_panels.size(); ++i) {
		// unregister
		_loop_control->register_auto_updates((int) i, true);
		_loop_control->register_input_controls((int) i, true);

                // sleep for a little bit so the UDP isn't lost during this barrage
#if wxCHECK_VERSION(2,5,3)
                ::wxMilliSleep(50);
#else
                ::wxUsleep(50);
#endif

	}

    _loop_connect_connection.disconnect();
	_loop_disconnect_connection.disconnect();
	_loop_update_connection.disconnect();

    _connect_failed_connection.disconnect();
    _lost_connect_connection.disconnect();
    _isalive_connection.disconnect();
    _error_recvd_connection.disconnect();

    
	delete _loop_control;

	delete _keyboard;

	delete _update_timer;
	delete _taptempo_button_timer;
}

void
MainPanel::init()
{
	_scroller_sizer = new wxBoxSizer(wxVERTICAL);
	_topsizer = new wxBoxSizer(wxVERTICAL);

	//wxBoxSizer * rowsizer = new wxBoxSizer(wxHORIZONTAL);
	wxInitAllImageHandlers();
	
	//SetBackgroundColour(*wxBLACK);
	//SetThemeEnabled(false);
	
	wxFont sliderFont = *wxSMALL_FONT;

	wxBoxSizer * rowsizer = new wxBoxSizer(wxHORIZONTAL);
	_top_panel = new wxPanel(this);
	_top_panel->SetThemeEnabled(false);
	_top_panel->SetBackgroundColour(*wxBLACK);

	wxBoxSizer * topcolsizer = new wxBoxSizer(wxVERTICAL);
	
	rowsizer->Add (1, 1, 1);

	_sync_choice = new ChoiceBox (_top_panel, ID_SyncChoice, true, wxDefaultPosition, wxSize (130, 20));
	_sync_choice->set_label (wxT("sync to"));
	_sync_choice->SetFont (sliderFont);
	_sync_choice->value_changed.connect (mem_fun (*this,  &MainPanel::on_syncto_change));
	_sync_choice->bind_request.connect (sigc::bind(mem_fun (*this,  &MainPanel::on_bind_request), wxT("sync")));
	
	rowsizer->Add (_sync_choice, 0, wxALL|wxEXPAND, 2);
	
	_tempo_bar = new SpinBox(_top_panel, ID_TempoSlider, 0.0f, 10000.0f, 110.0f, true, wxDefaultPosition, wxSize(120, 20));
	_tempo_bar->set_units(wxT("bpm"));
	_tempo_bar->set_label(wxT("tempo"));
	_tempo_bar->set_snap_mode (SpinBox::IntegerSnap);
	_tempo_bar->set_allow_outside_bounds(true);
	_tempo_bar->SetFont (sliderFont);
	_tempo_bar->value_changed.connect (mem_fun (*this,  &MainPanel::on_tempo_change));
	_tempo_bar->bind_request.connect (sigc::bind(mem_fun (*this,  &MainPanel::on_bind_request), wxT("tempo")));
	rowsizer->Add (_tempo_bar, 0, wxALL|wxEXPAND, 2);

 	_taptempo_button = new PixButton(_top_panel, ID_TapTempoButton, true);
	_taptempo_button->set_normal_bitmap (wxBitmap(tap_tempo_normal));
	_taptempo_button->set_selected_bitmap (wxBitmap(tap_tempo_selected));
	_taptempo_button->set_focus_bitmap (wxBitmap(tap_tempo_focus));
	_taptempo_button->set_disabled_bitmap (wxBitmap(tap_tempo_disabled));
	_taptempo_button->set_active_bitmap (wxBitmap(tap_tempo_active));
	_taptempo_button->pressed.connect (mem_fun (*this, &MainPanel::on_taptempo_press));
	_taptempo_button->released.connect (mem_fun (*this, &MainPanel::on_taptempo_release));
	_taptempo_button->bind_request.connect (sigc::bind(mem_fun (*this,  &MainPanel::on_bind_request), wxT("taptempo")));
 	rowsizer->Add (_taptempo_button, 0, wxALL|wxEXPAND, 2);
	

	_eighth_cycle_bar = new SpinBox(_top_panel, ID_EighthSlider, 1.0f, 1024.0f, 16.0f, true, wxDefaultPosition, wxSize(110, 20));
	_eighth_cycle_bar->set_units(wxT(""));
	_eighth_cycle_bar->set_label(wxT("8th/cycle"));
	_eighth_cycle_bar->set_snap_mode (SpinBox::IntegerSnap);
	_eighth_cycle_bar->set_allow_outside_bounds(true);
	_eighth_cycle_bar->SetFont (sliderFont);
	_eighth_cycle_bar->value_changed.connect (mem_fun (*this,  &MainPanel::on_eighth_change));
	_eighth_cycle_bar->bind_request.connect (sigc::bind(mem_fun (*this,  &MainPanel::on_bind_request), wxT("eighth")));
	rowsizer->Add (_eighth_cycle_bar, 0, wxALL|wxEXPAND, 2);
	

	_quantize_choice = new ChoiceBox (_top_panel, ID_QuantizeChoice, true, wxDefaultPosition, wxSize (110, 20));
	_quantize_choice->SetFont (sliderFont);
	_quantize_choice->set_label (wxT("quantize"));
	_quantize_choice->value_changed.connect (mem_fun (*this,  &MainPanel::on_quantize_change));
	_quantize_choice->bind_request.connect (sigc::bind(mem_fun (*this,  &MainPanel::on_bind_request), wxT("quantize")));
	_quantize_choice->append_choice (wxT("off"), 0);
	_quantize_choice->append_choice (wxT("cycle"), 1);
	_quantize_choice->append_choice (wxT("8th"), 2);
	_quantize_choice->append_choice (wxT("loop"), 3);
	rowsizer->Add (_quantize_choice, 0, wxALL|wxEXPAND, 2);

	_mute_quant_check = new CheckBox(_top_panel, ID_MuteQuantCheck, wxT("mute quant"), true, wxDefaultPosition, wxSize(90, 18));
	_mute_quant_check->SetFont(sliderFont);
	_mute_quant_check->SetToolTip(wxT("quantize mute operations"));
	_mute_quant_check->value_changed.connect (mem_fun (*this, &MainPanel::on_mute_quant_check));
	_mute_quant_check->bind_request.connect (sigc::bind(mem_fun (*this, &MainPanel::on_bind_request), wxT("mute_quantized")));
	rowsizer->Add (_mute_quant_check, 0, wxALL|wxEXPAND, 2);

	_odub_quant_check = new CheckBox(_top_panel, ID_OdubQuantCheck, wxT("odub quant"), true, wxDefaultPosition, wxSize(90, 18));
	_odub_quant_check->SetFont(sliderFont);
	_odub_quant_check->SetToolTip(wxT("quantize overdub operations"));
	_odub_quant_check->value_changed.connect (mem_fun (*this, &MainPanel::on_odub_quant_check));
	_odub_quant_check->bind_request.connect (sigc::bind(mem_fun (*this, &MainPanel::on_bind_request), wxT("overdub_quantized")));
	rowsizer->Add (_odub_quant_check, 0, wxALL|wxEXPAND, 2);

	_repl_quant_check = new CheckBox(_top_panel, ID_ReplQuantCheck, wxT("repl quant"), true, wxDefaultPosition, wxSize(90, 18));
	_repl_quant_check->SetFont(sliderFont);
	_repl_quant_check->SetToolTip(wxT("quantize replace and substitute operations"));
	_repl_quant_check->value_changed.connect (mem_fun (*this, &MainPanel::on_repl_quant_check));
	_repl_quant_check->bind_request.connect (sigc::bind(mem_fun (*this, &MainPanel::on_bind_request), wxT("replace_quantized")));
	rowsizer->Add (_repl_quant_check, 0, wxALL|wxEXPAND, 2);

	rowsizer->Add (1, 1, 1);

	wxStaticBitmap * logobit = new wxStaticBitmap(_top_panel, -1, wxBitmap(sl_logo_xpm));
	rowsizer->Add (logobit, 0, wxALIGN_BOTTOM);

	topcolsizer->Add (rowsizer, 0, wxEXPAND|wxTOP, 3);

	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	rowsizer->Add (1, 1, 1);

	_xfade_bar = new SpinBox(_top_panel, ID_XfadeSlider, 0.0f, 128000.0f, 64.0f, true, wxDefaultPosition, wxSize(100, 20));
	_xfade_bar->set_units(wxT(""));
	_xfade_bar->set_label(wxT("xfade"));
	_xfade_bar->SetToolTip(wxT("operation crossfade length in samples"));
	_xfade_bar->set_snap_mode (SpinBox::IntegerSnap);
	_xfade_bar->set_decimal_digits (0);
	_xfade_bar->SetFont (sliderFont);
	_xfade_bar->value_changed.connect (mem_fun (*this,  &MainPanel::on_xfade_change));
	_xfade_bar->bind_request.connect (sigc::bind(mem_fun (*this,  &MainPanel::on_bind_request), wxT("fade_samples")));
	rowsizer->Add (_xfade_bar, 0, wxALL|wxEXPAND, 2);

	_common_ingain_bar = new SliderBar(_top_panel, ID_InGainControl, 0.0f, 1.0f, 1.0f, true, wxDefaultPosition, wxSize(132,20));
	_common_ingain_bar->set_units(wxT("dB"));
	_common_ingain_bar->set_label(wxT("input gain"));
	_common_ingain_bar->set_scale_mode(SliderBar::ZeroGainMode);
	_common_ingain_bar->set_show_indicator_bar(true);
	_common_ingain_bar->SetFont(sliderFont);
	_common_ingain_bar->value_changed.connect (mem_fun (*this, &MainPanel::on_ingain_change));
	_common_ingain_bar->bind_request.connect (sigc::bind(mem_fun (*this, &MainPanel::on_bind_request), wxT("input_gain")));
	rowsizer->Add (_common_ingain_bar, 0, wxALL|wxEXPAND, 2);
	
	_common_dry_bar = new SliderBar(_top_panel, ID_DryControl, 0.0f, 1.0f, 1.0f, true, wxDefaultPosition, wxSize(132,20));
	_common_dry_bar->set_units(wxT("dB"));
	_common_dry_bar->set_label(wxT("main in mon"));
	_common_dry_bar->set_scale_mode(SliderBar::ZeroGainMode);
	_common_dry_bar->set_show_indicator_bar(true);
	_common_dry_bar->SetFont(sliderFont);
	_common_dry_bar->value_changed.connect (mem_fun (*this, &MainPanel::on_dry_change));
	_common_dry_bar->bind_request.connect (sigc::bind(mem_fun (*this, &MainPanel::on_bind_request), wxT("dry")));
	rowsizer->Add (_common_dry_bar, 0, wxALL|wxEXPAND, 2);

	_common_wet_bar = new SliderBar(_top_panel, ID_WetControl, 0.0f, 1.0f, 1.0f, true, wxDefaultPosition, wxSize(132,20));
	_common_wet_bar->set_units(wxT("dB"));
	_common_wet_bar->set_label(wxT("main out"));
	_common_wet_bar->set_scale_mode(SliderBar::ZeroGainMode);
	_common_wet_bar->set_show_indicator_bar(true);
	_common_wet_bar->SetFont(sliderFont);
	_common_wet_bar->value_changed.connect (mem_fun (*this, &MainPanel::on_wet_change));
	_common_wet_bar->bind_request.connect (sigc::bind(mem_fun (*this, &MainPanel::on_bind_request), wxT("wet")));
	rowsizer->Add (_common_wet_bar, 0, wxALL|wxEXPAND, 2);
	

	
	_round_check = new CheckBox (_top_panel, ID_RoundCheck, wxT("round"), true, wxDefaultPosition, wxSize(60, 20));
	_round_check->SetFont (sliderFont);
	_round_check->value_changed.connect (mem_fun (*this, &MainPanel::on_round_check));
	_round_check->bind_request.connect (sigc::bind(mem_fun (*this,  &MainPanel::on_bind_request), wxT("round")));
	rowsizer->Add (_round_check, 0, wxALL|wxEXPAND, 2);

	_relsync_check = new CheckBox (_top_panel, ID_RelSyncCheck, wxT("rel sync"), true, wxDefaultPosition, wxSize(75, 20));
	_relsync_check->SetFont (sliderFont);
	_relsync_check->value_changed.connect (mem_fun (*this, &MainPanel::on_relsync_check));
	_relsync_check->bind_request.connect (sigc::bind(mem_fun (*this,  &MainPanel::on_bind_request), wxT("relative_sync")));
	rowsizer->Add (_relsync_check, 0, wxALL|wxEXPAND, 2);
	

	_smart_eighths_check = new CheckBox(_top_panel, ID_SmartEighthCheck, wxT("auto 8th"), true, wxDefaultPosition, wxSize(80, 18));
	_smart_eighths_check->SetFont(sliderFont);
	_smart_eighths_check->SetToolTip(wxT("auto adjust 8ths per cycle with tempo"));
	_smart_eighths_check->value_changed.connect (mem_fun (*this, &MainPanel::on_smart_eighths_check));
	_smart_eighths_check->bind_request.connect (sigc::bind(mem_fun (*this, &MainPanel::on_bind_request), wxT("smart_eighths")));
	rowsizer->Add (_smart_eighths_check, 0, wxALL|wxEXPAND, 2);


	rowsizer->Add (1, 1, 1);

	topcolsizer->Add (rowsizer, 0, wxEXPAND|wxBOTTOM, 3);

	
	_top_panel->SetSizer( topcolsizer );      // actually set the sizer

	_topsizer->Add (_top_panel, 0, wxEXPAND);

	
	_scroller = new wxScrolledWindow(this, -1, wxDefaultPosition, wxSize(846, 118), wxVSCROLL); // initial size should fit one loop
	_scroller->SetBackgroundColour(*wxBLACK);
	

	// todo request how many loopers to construct based on connection
	_loop_connect_connection = _loop_control->LooperConnected.connect (mem_fun (*this, &MainPanel::init_loopers));
	_loop_disconnect_connection = _loop_control->Disconnected.connect (sigc::bind(mem_fun (*this, &MainPanel::init_loopers), 0));
	_loop_update_connection = _loop_control->NewDataReady.connect (mem_fun (*this, &MainPanel::osc_data_ready));



	_topsizer->Add (_scroller, 1, wxEXPAND);
	
	_scroller->SetSizer( _scroller_sizer );      // actually set the sizer

	_scroller->SetScrollRate (0, 30);
	_scroller->EnableScrolling (true, true);
	
	this->SetSizer( _topsizer );      // actually set the sizer

	_scroller->SetFocus();
}

void
MainPanel::init_syncto_choice()
{
	// 		BrotherSync = -4,
	// 		InternalTempoSync = -3,
	// 		MidiClockSync = -2,
	// 		JackSync = -1,
	// 		NoSync = 0
		
	_sync_choice->clear_choices ();
	_sync_choice->append_choice (wxT("None"), 0);
	_sync_choice->append_choice (wxT("Internal"), -3);
	_sync_choice->append_choice (wxT("MidiClock"), -2);
	_sync_choice->append_choice (wxT("Jack/Host"), -1);
//	_sync_choice->append_choice (wxT("BrotherSync"), -4);

	// the remaining choices are loops
	for (unsigned int i=0; i < _looper_panels.size(); ++i) {
		_sync_choice->append_choice (wxString::Format(wxT("Loop %d"), i+1), i+1);
	}

	update_syncto_choice ();
}

    


void
MainPanel::init_loopers (int count)
{
	LooperPanel * looperpan;	

	if (count > (int) _looper_panels.size()) {
		while (count > (int) _looper_panels.size()) {
			looperpan = new LooperPanel(this, _loop_control, _scroller, -1);
			looperpan->set_index(_looper_panels.size());
			_scroller_sizer->Add (looperpan, 0, wxEXPAND|wxALL, 0);
			_looper_panels.push_back (looperpan);
		}
	}
	else if (count < (int)_looper_panels.size()) {
		while (count < (int)_looper_panels.size()) {
			looperpan = _looper_panels.back();
			_looper_panels.pop_back();
			_scroller_sizer->Remove((wxBoxSizer*)looperpan);
			looperpan->Destroy();
		}
	}

	_scroller->Layout();
	_scroller->FitInside();

	// maybe resize topwindow, keeping width the same, but resize height to be just big enough to hold the updated number of looper panels
	if (_looper_panels.size() > 0 && _looper_panels.size() <= 4) {
		wxTopLevelWindow *topwindow = wxStaticCast(wxGetTopLevelParent(this), wxTopLevelWindow);
		if (topwindow && topwindow->IsIconized() == false) // don't trigger refit if minimized
			topwindow->SetClientSize(GetSize().GetWidth(), _top_panel->GetSize().GetHeight() + _scroller_sizer->GetMinSize().GetHeight());
 	}

	// request all values for initial state
	_loop_control->register_all_in_new_thread(_looper_panels.size());

	init_syncto_choice ();

	set_curr_loop (_curr_loop);
	_engine_alive = true;
}

void
MainPanel::set_sliders_allow_mousewheel (bool flag)
{
    _sliders_allow_mousewheel = flag;
    SliderBar::set_use_mousewheel_default(_sliders_allow_mousewheel);
    SpinBox::set_use_mousewheel_default(_sliders_allow_mousewheel);
    ChoiceBox::set_use_mousewheel_default(_sliders_allow_mousewheel);
}

void
MainPanel::osc_data_ready()
{
	// cerr << "osc ready" << endl;
	// this is called from another thread
	_got_new_data++;

	::wxWakeUpIdle();
}

void 
MainPanel::on_connect_failed (const std::string & msg)
{
	wxMessageDialog dial(this, wxString::FromAscii(msg.c_str()), wxT("Connection Error"), wxOK);
	dial.SetTitle(wxT("Connection Error"));
	dial.ShowModal();
}

void 
MainPanel::on_connection_lost (const std::string & msg)
{
	wxMessageDialog dial(this, wxString::FromAscii(msg.c_str()), wxT("Lost Connection"), wxOK);
	dial.SetTitle(wxT("Lost Connection"));

	dial.ShowModal();
}

void
MainPanel::on_error_received (const std::string & msg)
{
	wxMessageDialog dial(this, wxString::FromAscii(msg.c_str()), wxT("Error Received"), wxOK);
	dial.SetTitle(wxT("Error Received"));

	dial.ShowModal();
}


void
MainPanel::on_engine_alive ()
{
	//cerr << "got alive ping" << endl;
	_engine_alive = true;
}


void
MainPanel::OnUpdateTimer(wxTimerEvent &ev)
{
	// check to see if our connected server is still alive
	
	if (_loop_control->connected() || _never_timeout) {
 		_loop_control->update_values();

		if (!_engine_alive && !_never_timeout) {		
			_loop_control->disconnect();
			on_connection_lost ("Lost connection to SooperLooper engine.\nSee the Preferences->Connections tab to start a new one");
		}

		_engine_alive = false;
		_loop_control->send_alive_ping();
	}

	_update_timer->Start(_update_timer_time, true);
}

void MainPanel::set_never_timeout(bool flag) 
{ 
	_never_timeout = flag; 
	if (_never_timeout) {
		// reduce sleep time to check more often
		_update_timer_time = 3000;
	}
}


void
MainPanel::do_close(bool quitengine)
{
	// send quit command to looper by default
	save_default_midibindings();
	_loop_update_connection.disconnect();
        _loop_disconnect_connection.disconnect();
        _loop_connect_connection.disconnect();

	_update_timer->Stop();
	_taptempo_button_timer->Stop();

	// sleep for a short period before stopping engine
#if wxCHECK_VERSION(2,5,3)
	::wxMilliSleep(500);
#else
	::wxUsleep(500);
#endif

	if (quitengine) {
		_loop_control->send_quit();
	}
}


void
MainPanel::OnActivate(wxActivateEvent &ev)
{
	cerr << "mainpan activeate" << endl;
	if (ev.GetActive()) {
		_keyboard->set_enabled (true);
	}
	else {
		_keyboard->set_enabled (false);
	}

	ev.Skip();
}


void
MainPanel::on_taptempo_timer(wxTimerEvent &ev)
{
	_taptempo_button->set_active(false);
}

void
MainPanel::update_controls()
{
	// get recent controls from loop control
	float val;

	if (_loop_control->is_global_updated(wxT("tempo"))) {
		_loop_control->get_global_value(wxT("tempo"), val);
		_tempo_bar->set_value (val);
	}

	if (_loop_control->is_global_updated(wxT("tap_tempo"))) {	
		_loop_control->get_global_value(wxT("tap_tempo"), val);

		float tempo;
		_loop_control->get_global_value(wxT("tempo"), tempo);
		// turn on tap active, then timeout to flip it back
		_taptempo_button->set_active(true);

		if (tempo > 200) {
			// half the tempo in ms
			int ms = (int) (1.0f/tempo * 30000.0f);
			_taptempo_button_timer->Start(ms, true);
		}
		else {
			_taptempo_button_timer->Start(150, true);
		}
	}
	
	
	if (_loop_control->is_global_updated(wxT("eighth_per_cycle"))) {
		_loop_control->get_global_value(wxT("eighth_per_cycle"), val);
		_eighth_cycle_bar->set_value (val);
	}
	
	if (_loop_control->is_global_updated(wxT("sync_source"))) {
		update_syncto_choice ();
	}

	// quantize from first loop
 	if (_loop_control->is_updated(0, wxT("quantize"))) {
		_loop_control->get_value(0, wxT("quantize"), val);
		val = roundf(val);
 		_quantize_choice->set_index_value ((int)val);
	}

 	if (_loop_control->is_updated(0, wxT("round"))) {
		_loop_control->get_value(0, wxT("round"), val);
 		_round_check->set_value (val > 0.0);
	}

 	if (_loop_control->is_updated(0, wxT("relative_sync"))) {
		_loop_control->get_value(0, wxT("relative_sync"), val);
 		_relsync_check->set_value (val > 0.0);
	}

 	if (_loop_control->is_updated(0, wxT("mute_quantized"))) {
		_loop_control->get_value(0, wxT("mute_quantized"), val);
 		_mute_quant_check->set_value (val > 0.0);
	}

 	if (_loop_control->is_updated(0, wxT("overdub_quantized"))) {
		_loop_control->get_value(0, wxT("overdub_quantized"), val);
 		_odub_quant_check->set_value (val > 0.0);
	}

 	if (_loop_control->is_updated(0, wxT("replace_quantized"))) {
		_loop_control->get_value(0, wxT("replace_quantized"), val);
 		_repl_quant_check->set_value (val > 0.0);
	}

 	if (_loop_control->is_global_updated(wxT("smart_eighths"))) {
		_loop_control->get_global_value(wxT("smart_eighths"), val);
 		_smart_eighths_check->set_value (val > 0.0);
	}
	
	
	if (_loop_control->is_updated(0, wxT("fade_samples"))) {
		_loop_control->get_value(0, wxT("fade_samples"), val);
		_xfade_bar->set_value (val);
	}

	if (_loop_control->is_global_updated(wxT("dry"))) {
		_loop_control->get_global_value(wxT("dry"), val);
		_common_dry_bar->set_value (val);
	}

	if (_loop_control->is_global_updated(wxT("input_gain"))) {
		_loop_control->get_global_value(wxT("input_gain"), val);
		_common_ingain_bar->set_value (val);
	}
	
	// don't check, others might be using it
	_loop_control->get_global_value(wxT("in_peak_meter"), val);
	_common_dry_bar->set_indicator_value (val);

	
	if (_loop_control->is_global_updated(wxT("wet"))) {
		_loop_control->get_global_value(wxT("wet"), val);
		_common_wet_bar->set_value (val);
	}
	
	if (_loop_control->is_global_updated (wxT("out_peak_meter"))) {
		_loop_control->get_global_value (wxT("out_peak_meter"), val);
		_common_wet_bar->set_indicator_value (val);
	}

	if (_loop_control->is_global_updated(wxT("selected_loop_num"))) {
		_loop_control->get_global_value(wxT("selected_loop_num"), val);
		set_curr_loop ((int) val);
	}

	if (_loop_control->is_global_updated(wxT("global_cycle_pos"))) {
		float gclen, gcpos;
		_loop_control->get_global_value(wxT("global_cycle_len"), gclen);
		_loop_control->get_global_value(wxT("global_cycle_pos"), gcpos);
		val = 0.0f;
		// only display global cycle len if it is >= 1s to reduce visual noise
		if(gclen >= 1.0f)
			val = gcpos / gclen;
		_quantize_choice->set_bar_value(val);
	}

}

void
MainPanel::update_syncto_choice()
{
	float val = 0.0f;
	_loop_control->get_global_value(wxT("sync_source"), val);
	
	long data = (long) roundf(val);
	int index = -1;
// 		BrotherSync = -4,
// 		InternalTempoSync = -3,
// 		MidiClockSync = -2,
// 		JackSync = -1,
// 		NoSync = 0
		
	wxString sval;
	ChoiceBox::ChoiceList chlist;
	_sync_choice->get_choices(chlist);
	int i=0;
	
	for (ChoiceBox::ChoiceList::iterator iter = chlist.begin(); iter != chlist.end(); ++iter, ++i)
	{
		if ((*iter).second == data) {
			index = i;
			break;
		}
	}
	
	_sync_choice->set_index_value (index);
}


void
MainPanel::OnHide(wxCommandEvent &event)
{

}


void
MainPanel::OnSize(wxSizeEvent & event)
{
	event.Skip();
}

void MainPanel::OnPaint(wxPaintEvent & event)
{
	event.Skip();
}

void
MainPanel::OnIdle(wxIdleEvent& event)
{
    
	if (_got_new_data) {
		//cerr << "idle update" << endl;

 		_loop_control->update_values();
		
    		for (unsigned int i=0; i < _looper_panels.size(); ++i) {
    			_looper_panels[i]->update_controls();
    		}
		
    		update_controls();

 		_got_new_data = 0;
	}
	
	//event.Skip();
}

void
MainPanel::save_default_midibindings ()
{
	wxString dirname = _rcdir;

	if ( ! wxDirExists(dirname) ) {
		if (!wxMkdir ( dirname, 0755 )) {
			printf ("Error creating %s\n", static_cast<const char *> (dirname.mb_str())); 
			return;
		}
	}

	_loop_control->save_midi_bindings ( (dirname + wxFileName::GetPathSeparator() + wxT("default_midi.slb")));
}

void
MainPanel::do_add_loop (const string & type)
{

	LoopControl::SpawnConfig & sconf = _loop_control->get_spawn_config();

	if (type == "mono") {
		_loop_control->post_add_loop (1, sconf.mem_secs, sconf.discrete_io);
	}
	else if (type == "stereo") {
		_loop_control->post_add_loop (2, sconf.mem_secs, sconf.discrete_io);
	}
	else {
		_loop_control->post_add_loop();
	}
}

#define IdAddCustomLoop 34567

void
MainPanel::on_modal_dialog_close (wxCommandEvent & ev)
{
#ifdef __WXMAC__    
    AddCustomLoopDialog * dial = (AddCustomLoopDialog *)((wxWindowModalDialogEvent *)&ev)->GetDialog();
    int retcode = ((wxWindowModalDialogEvent *)&ev)->GetReturnCode();
    if (retcode == wxID_OK) {
        // wxLogError("Num loops: %d   numchan: %d  secs: %g  discrete: %d", dial->num_loops, dial->num_channels, dial->secs_channel, dial->discrete);
        
		for (int i=0; i < dial->num_loops; ++i) {
			//cerr << "adding loop with " << dial->num_channels << "  secs: " << dial->secs_channel << endl;
			_loop_control->post_add_loop (dial->num_channels, dial->secs_channel, dial->discrete);
		}
    }
#endif
}

void
MainPanel::do_add_custom_loop ()
{
#ifdef __WXMAC__
	AddCustomLoopDialog * dial = new AddCustomLoopDialog(this, IdAddCustomLoop);
    this->Connect(IdAddCustomLoop, wxEVT_WINDOW_MODAL_DIALOG_CLOSED, wxCommandEventHandler(MainPanel::on_modal_dialog_close));
    dial->CentreOnParent();
    dial->ShowWindowModal();
    //dial->Show();
#else
    AddCustomLoopDialog * dial = new AddCustomLoopDialog(this);
	dial->CentreOnParent();
	// it takes care of itself
	if (dial->ShowModal() == wxID_OK) {
		for (int i=0; i < dial->num_loops; ++i) {
			//cerr << "adding loop with " << dial->num_channels << "  secs: " << dial->secs_channel << endl;
			_loop_control->post_add_loop (dial->num_channels, dial->secs_channel, dial->discrete);
		}
	}
	delete dial;

#endif
}

void
MainPanel::do_remove_loop ()
{
	_loop_control->post_remove_loop();
}

void
MainPanel::on_bind_request (wxString val)
{
	MidiBindInfo info;
	bool donothing = false;

	info.channel = 0;
	info.type = "cc";
	info.command = "set";
	info.instance = -2;
	info.lbound = 0.0;
	info.ubound = 1.0;
	info.style = MidiBindInfo::NormalStyle;

	
	if (val == wxT("tempo")) {
		info.lbound = 20.0f;
		info.ubound = 274.0f;
		info.control = "tempo";
	}
	else if (val == wxT("taptempo")) {
		info.control = "tap_tempo";
	}
	else if (val == wxT("eighth")) {
		info.control = "eighth_per_cycle";
		info.lbound = 1.0f;
		info.ubound = 128.0f;
	}
	else if (val == wxT("fade_samples")) {
		info.control = "fade_samples";
		info.lbound = 0.0f;
		info.ubound = 16384.0f;
	}
	else if (val == wxT("dry")) {
		info.control = "dry";
		info.style = MidiBindInfo::GainStyle;
	}
	else if (val == wxT("wet")) {
		info.control = "wet";
		info.style = MidiBindInfo::GainStyle;
	}
	else if (val == wxT("input_gain")) {
		info.control = "input_gain";
		info.style = MidiBindInfo::GainStyle;
	}
	else if (val == wxT("round")) {
		info.instance = -1;
		info.control = "round";
	}
	else if (val == wxT("sync")) {
		info.control = "sync_source";
		info.lbound = -3.0f;
		info.ubound = 16.0f;
	}
	else if (val == wxT("quantize")) {
		info.instance = -1;
		info.control = "quantize";
		info.lbound = 0.0f;
		info.ubound = 3.0f;
	}
	else if (val == wxT("mute_quantized")) {
		info.control = "mute_quantized";
		info.instance = -1;
	}
	else if (val == wxT("overdub_quantized")) {
		info.control = "overdub_quantized";
		info.instance = -1;
	}
	else if (val == wxT("replace_quantized")) {
		info.control = "replace_quantized";
		info.instance = -1;
	}
	else if (val == wxT("smart_eighths")) {
		info.control = "smart_eighths";
		info.instance = -1;
	}
	else {
		donothing = true;
	}

	if (!donothing) {
		_loop_control->learn_midi_binding(info, true);
	}
	
}


void
MainPanel::on_tempo_change (float value)
{
	_loop_control->post_global_ctrl_change (wxT("tempo"), value);
}

void
MainPanel::on_eighth_change (float value)
{
	_loop_control->post_global_ctrl_change (wxT("eighth_per_cycle"), value);
}

void
MainPanel::on_xfade_change (float value)
{
	_loop_control->post_ctrl_change (-1, wxT("fade_samples"), value);
}

void
MainPanel::on_dry_change (float value)
{
	//_loop_control->post_global_ctrl_change (wxT("dry"), value);
	_loop_control->post_ctrl_change (-2, wxT("dry"), value);
}

void
MainPanel::on_wet_change (float value)
{
	_loop_control->post_global_ctrl_change (wxT("wet"), value);
}

void
MainPanel::on_ingain_change (float value)
{
	_loop_control->post_global_ctrl_change (wxT("input_gain"), value);
}

void
MainPanel::on_syncto_change (int index, wxString val)
{
// 		BrotherSync = -4,
// 		InternalTempoSync = -3,
// 		MidiClockSync = -2,
// 		JackSync = -1,
// 		NoSync = 0
//            >0 is loop number
	
	float value = 0.0f;

	value = (float) _sync_choice->get_data_value();
	
	_loop_control->post_global_ctrl_change (wxT("sync_source"), value);
}


void
MainPanel::on_quantize_change (int index, wxString val)
{
	// 0 is none, 1 is cycle, 2 is eighth, 3 is loop
	// send for all loops
	_loop_control->post_ctrl_change (-1, wxT("quantize"), (float) index);
}

void
MainPanel::on_round_check (bool val)
{
	// send for all loops
	_loop_control->post_ctrl_change (-1, wxT("round"), val ? 1.0f: 0.0f);
}

void
MainPanel::on_mute_quant_check (bool val)
{
	// send for all loops
	_loop_control->post_ctrl_change (-1, wxT("mute_quantized"), val ? 1.0f: 0.0f);
}

void
MainPanel::on_odub_quant_check (bool val)
{
	// send for all loops
	_loop_control->post_ctrl_change (-1, wxT("overdub_quantized"), val ? 1.0f: 0.0f);
}

void
MainPanel::on_repl_quant_check (bool val)
{
	// send for all loops
	cerr << "on replace quan check" << endl;
	_loop_control->post_ctrl_change (-1, wxT("replace_quantized"), val ? 1.0f: 0.0f);
}

void
MainPanel::on_smart_eighths_check (bool val)
{
	// send for all loops
	_loop_control->post_global_ctrl_change (wxT("smart_eighths"), val ? 1.0f: 0.0f);
}

void
MainPanel::on_relsync_check (bool val)
{
	// send for all loops
	_loop_control->post_ctrl_change (-1, wxT("relative_sync"), val ? 1.0f: 0.0f);
}

void
MainPanel::on_taptempo_press (int button)
{
	// needs to be a normal ctrl change for RTness
	_loop_control->post_ctrl_change (-2, wxT("tap_tempo"), 1.0f);
}

void
MainPanel::on_taptempo_release (int button)
{
	if (button == PixButton::MiddleButton) {
		_loop_control->post_ctrl_change (-2, wxT("tap_tempo"), 1.0f);
	}
}


void
MainPanel::process_key_event (wxKeyEvent &ev)
{
	// this is a pretty extreme hack
	// to let textfields, etc named with the right name
	// get their key events
	static wxString textname = wxT("KeyAware");

	//cerr << "got " << ev.GetKeyCode() << endl;
	
	wxWindow * focwin = wxWindow::FindFocus();
	if (focwin && (focwin->GetName() == textname
		       || (focwin->GetParent() && ((focwin->GetParent()->GetName() == textname)
						   || (focwin->GetParent()->GetParent()
						       && focwin->GetParent()->GetParent()->GetName() == textname)))))
	{
		ev.Skip();
	}
	else {
		_keyboard->process_key_event (ev);
	}
}


void MainPanel::intialize_keybindings ()
{
	
	_keyboard->add_action ("record", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record")));
	_keyboard->add_action ("overdub", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("overdub")));
	_keyboard->add_action ("multiply", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("multiply")));
	_keyboard->add_action ("insert", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("insert")));
	_keyboard->add_action ("replace", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("replace")));
	_keyboard->add_action ("reverse", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("reverse")));
	_keyboard->add_action ("scratch", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("scratch")));
	_keyboard->add_action ("substitute", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("substitute")));
	_keyboard->add_action ("mute", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("mute")));
	_keyboard->add_action ("mute_on", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("mute_on")));
	_keyboard->add_action ("mute_off", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("mute_off")));
	_keyboard->add_action ("mute_trigger", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("mute_trigger")));
	_keyboard->add_action ("undo", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("undo")));
	_keyboard->add_action ("redo", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("redo")));	
	_keyboard->add_action ("undo_all", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("undo_all")));
	_keyboard->add_action ("redo_all", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("redo_all")));	
	_keyboard->add_action ("oneshot", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("oneshot")));
	_keyboard->add_action ("trigger", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("trigger")));
	_keyboard->add_action ("pause", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("pause")));
	_keyboard->add_action ("pause_on", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("pause_on")));
	_keyboard->add_action ("pause_off", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("pause_off")));
	_keyboard->add_action ("solo", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("solo")));
	_keyboard->add_action ("solo_prev", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("solo_prev")));
	_keyboard->add_action ("solo_next", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("solo_next")));
	_keyboard->add_action ("record_solo", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_solo")));
	_keyboard->add_action ("record_solo_prev", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_solo_prev")));
	_keyboard->add_action ("record_solo_next", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_solo_next")));
	_keyboard->add_action ("set_sync_pos", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("set_sync_pos")));
	_keyboard->add_action ("reset_sync_pos", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("reset_sync_pos")));
	_keyboard->add_action ("record_or_overdub", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_or_overdub")));
	_keyboard->add_action ("record_exclusive", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_exclusive")));
	_keyboard->add_action ("record_exclusive_next", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_exclusive_next")));
	_keyboard->add_action ("record_exclusive_prev", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_exclusive_prev")));	
	_keyboard->add_action ("record_or_overdub_excl", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_or_overdub_excl")));
	_keyboard->add_action ("record_or_overdub_excl_next", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_or_overdub_excl_next")));
	_keyboard->add_action ("record_or_overdub_excl_prev", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_or_overdub_excl_prev")));
	_keyboard->add_action ("record_or_overdub_solo", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_or_overdub_solo")));
	_keyboard->add_action ("record_or_overdub_solo_next", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_or_overdub_solo_next")));
	_keyboard->add_action ("record_or_overdub_solo_prev", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_or_overdub_solo_prev")));
	_keyboard->add_action ("record_overdub_end_solo", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_overdub_end_solo")));
	_keyboard->add_action ("record_overdub_end_solo_trig", sigc::bind(mem_fun (*this, &MainPanel::command_action), wxT("record_overdub_end_solo_trig")));

    
	_keyboard->add_action ("delay", sigc::bind(mem_fun (*this, &MainPanel::misc_action), wxT("delay")));
	_keyboard->add_action ("taptempo", sigc::bind(mem_fun (*this, &MainPanel::misc_action), wxT("taptempo")));
	_keyboard->add_action ("load", sigc::bind(mem_fun (*this, &MainPanel::misc_action), wxT("load")));
	_keyboard->add_action ("save", sigc::bind(mem_fun (*this, &MainPanel::misc_action), wxT("save")));
	_keyboard->add_action ("cancel_midi_learn", sigc::bind(mem_fun (*this, &MainPanel::misc_action), wxT("cancel_learn")));

	_keyboard->add_action ("select_prev_loop", sigc::bind(mem_fun (*this, &MainPanel::select_loop_action), -2));
	_keyboard->add_action ("select_next_loop", sigc::bind(mem_fun (*this, &MainPanel::select_loop_action), -1));
	_keyboard->add_action ("select_loop_1", sigc::bind(mem_fun (*this, &MainPanel::select_loop_action), 1));
	_keyboard->add_action ("select_loop_2", sigc::bind(mem_fun (*this, &MainPanel::select_loop_action), 2));
	_keyboard->add_action ("select_loop_3", sigc::bind(mem_fun (*this, &MainPanel::select_loop_action), 3));
	_keyboard->add_action ("select_loop_4", sigc::bind(mem_fun (*this, &MainPanel::select_loop_action), 4));
	_keyboard->add_action ("select_loop_5", sigc::bind(mem_fun (*this, &MainPanel::select_loop_action), 5));
	_keyboard->add_action ("select_loop_6", sigc::bind(mem_fun (*this, &MainPanel::select_loop_action), 6));
	_keyboard->add_action ("select_loop_7", sigc::bind(mem_fun (*this, &MainPanel::select_loop_action), 7));
	_keyboard->add_action ("select_loop_8", sigc::bind(mem_fun (*this, &MainPanel::select_loop_action), 8));
	_keyboard->add_action ("select_loop_9", sigc::bind(mem_fun (*this, &MainPanel::select_loop_action), 9));
	_keyboard->add_action ("select_loop_all", sigc::bind(mem_fun (*this, &MainPanel::select_loop_action), 0));

	
	// these are the defaults... they get overridden by rc file

	_keyboard->add_binding ("r", "record");
	_keyboard->add_binding ("o", "overdub");
	_keyboard->add_binding ("x", "multiply");
	_keyboard->add_binding ("i", "insert");
	_keyboard->add_binding ("p", "replace");
	_keyboard->add_binding ("v", "reverse");
	_keyboard->add_binding ("m", "mute");
	_keyboard->add_binding ("u", "undo");
	_keyboard->add_binding ("d", "redo");
	_keyboard->add_binding ("s", "scratch");
	_keyboard->add_binding ("b", "substitute");
	_keyboard->add_binding ("l", "delay");
	_keyboard->add_binding ("h", "oneshot");
	_keyboard->add_binding (" ", "trigger");
	_keyboard->add_binding ("t", "taptempo");
	_keyboard->add_binding ("z", "pause");
	_keyboard->add_binding ("q", "solo");

	_keyboard->add_binding ("Control-s", "save");
	_keyboard->add_binding ("Control-o", "load");
	_keyboard->add_binding ("escape", "cancel_midi_learn");
	
	_keyboard->add_binding ("1", "select_loop_1");
	_keyboard->add_binding ("2", "select_loop_2");
	_keyboard->add_binding ("3", "select_loop_3");
	_keyboard->add_binding ("4", "select_loop_4");
	_keyboard->add_binding ("5", "select_loop_5");
	_keyboard->add_binding ("6", "select_loop_6");
	_keyboard->add_binding ("7", "select_loop_7");
	_keyboard->add_binding ("8", "select_loop_8");
	_keyboard->add_binding ("9", "select_loop_9");
	_keyboard->add_binding ("0", "select_loop_all");
	
}


void MainPanel::command_action (bool release, wxString cmd)
{
	if (release) {
		_loop_control->post_up_event (_curr_loop, cmd);
	}
	else {
		_loop_control->post_down_event (_curr_loop, cmd);
		//cerr << "action " << cmd.ToAscii() << endl;
	}
}

void MainPanel::select_loop_action (bool release, int index)
{
	if (release) return;

	if (index == -2) {
		// prev loop
		_loop_control->post_ctrl_change(-2, wxT("select_prev_loop"), (float) 1.0f);
	}
	else if (index == -1)
	{
		// next loop
		_loop_control->post_ctrl_change(-2, wxT("select_next_loop"), (float) 1.0f);
	}
	else {
		index--;
		
		if (index < (int) _looper_panels.size()) {
			// send osc control
			_loop_control->post_ctrl_change(-2, wxT("selected_loop_num"), (float) index);
			set_curr_loop (index);
		}
	}
}


void MainPanel::misc_action (bool release, wxString cmd)
{
	int index = _curr_loop;

	// only on press
	if (release) return;

	if (index < 0) index = -1;
	
	if (cmd == wxT("taptempo")) {

		on_taptempo_press(1);
	}
	else if (cmd == wxT("delay")) {
		_tapdelay_val *= -1.0f;
		_loop_control->post_ctrl_change (index, wxString(wxT("tap_trigger")), _tapdelay_val);
	}
	else if (cmd == wxT("cancel_learn")) {

		_loop_control->cancel_midi_learn();
	}
	else if (cmd == wxT("save"))
	{
		if (index < 0) {
			index = 0;
		}


		wxString filename = do_file_selector (wxT("Choose file to save loop"), wxT("wav"), wxT("WAVE files (*.wav)|*.wav;*.WAV;*.Wav"),  wxFD_SAVE|wxFD_CHANGE_DIR|wxFD_OVERWRITE_PROMPT);
		
		if ( !filename.empty() )
		{
			// add .wav if there isn't one already
			if (filename.size() <= 4 || (filename.size() > 4 && filename.substr(filename.size() - 4, 4) != wxT(".wav"))) {
				filename += wxT(".wav");
			}
			// todo: specify format
			_loop_control->post_save_loop (index, filename);
		}
		
	}
	else if (cmd == wxT("load"))
	{
		if (index < 0) {
			index = 0;
		}

		wxString filename = do_file_selector (wxT("Choose file to open"), wxT(""), wxT("*.slsess"), wxFD_OPEN|wxFD_CHANGE_DIR);
		
		if ( !filename.empty() )
		{
			_loop_control->post_load_loop (index, filename);
		}

	}
}


bool MainPanel::load_rc()
{
	// open file
	string configfname(static_cast<const char *> ((_rcdir + wxFileName::GetPathSeparator() + wxT("gui_config.xml")).fn_str() ));
	XMLTree configdoc (configfname);

	if (!configdoc.initialized()) {
		fprintf (stderr, "Error loading config at %s!\n", configfname.c_str()); 
		return false;
	}

	XMLNode * rootNode = configdoc.root();
	if (!rootNode || rootNode->name() != "SLConfig") {
		fprintf (stderr, "Preset root node not found in %s!\n", configfname.c_str()); 
		return false;
	}

        XMLProperty *prop;

        if ((prop = rootNode->property ("window_x_pos")) != 0) {
                _default_position.x = atol (prop->value().c_str());
        }
        if ((prop = rootNode->property ("window_y_pos")) != 0) {
                _default_position.y = atol (prop->value().c_str());
        }

        if ((prop = rootNode->property ("sliders_allow_mousewheel")) != 0) {
            set_sliders_allow_mousewheel( (bool) atoi (prop->value().c_str()));
        }


        
	XMLNode * bindingsNode = rootNode->find_named_node ("KeyBindings");
	if (!bindingsNode ) {
		fprintf(stderr, "Preset Channels node not found in %s!\n", configfname.c_str()); 
		//return false;
	}
	else {
		_keyboard->set_binding_state (*bindingsNode);
	}

	bindingsNode = rootNode->find_named_node ("SpawnConfig");
	if (!bindingsNode ) {
		fprintf(stderr, "SpawnConfig node not found in %s!\n", configfname.c_str()); 
		//return false;
	}
	else {
		_loop_control->get_spawn_config().set_state (*bindingsNode);
		_loop_control->get_default_spawn_config().set_state (*bindingsNode);
	}


	return true;
}

bool MainPanel::save_rc()
{
	wxString dirname = _rcdir;
	
	if ( ! wxDirExists(dirname) ) {
		if (!wxMkdir ( dirname, 0755 )) {
			printf ("Error creating %s\n", static_cast<const char *> (dirname.mb_str())); 
			return false;
		}
	}

	// make xmltree
	XMLTree configdoc;
	XMLNode * rootNode = new XMLNode("SLConfig");
	rootNode->add_property("version", sooperlooper_version);

        char buf[40];

        snprintf(buf, sizeof(buf), "%d",_default_position.x);
        rootNode->add_property ("window_x_pos", buf);
        snprintf(buf, sizeof(buf), "%d", _default_position.y);
        rootNode->add_property ("window_y_pos", buf);
        snprintf(buf, sizeof(buf), "%d", (int)_sliders_allow_mousewheel);
        rootNode->add_property ("sliders_allow_mousewheel", buf);

        
	configdoc.set_root (rootNode);
	
	XMLNode * bindingsNode = rootNode->add_child ("KeyBindings");
	bindingsNode->add_child_nocopy (_keyboard->get_binding_state());

	bindingsNode = rootNode->add_child ("SpawnConfig");
	bindingsNode->add_child_nocopy (_loop_control->get_default_spawn_config().get_state());

	
	// write doc to file
	
	if (configdoc.write (static_cast<const char *> ((dirname + wxFileName::GetPathSeparator() + wxT("gui_config.xml")).fn_str())))
	{	    
		fprintf (stderr, "Stored settings into %s\n", static_cast<const char *> (dirname.fn_str()));
		return true;
	}
	else {
		fprintf (stderr, "Failed to store settings into %s\n", static_cast<const char *> (dirname.fn_str()));
		return false;
	}

}

void MainPanel::set_curr_loop (int index)
{
	if (index < 0) index = -1;
	
	_curr_loop = index;

	// cerr << "got loop index " << _curr_loop << endl;
	
	int i=0;
	for (vector<LooperPanel *>::iterator iter = _looper_panels.begin(); iter != _looper_panels.end(); ++iter, ++i) {

		if (_curr_loop == i) {

			(*iter)->set_selected (true);
		}
		else {
			(*iter)->set_selected (false);
		}
	}
}

void MainPanel::do_load_session ()
{
	wxString filename = do_file_selector (wxT("Choose session to load"), wxT("*.slsess"), wxT("*.slsess"), wxFD_OPEN|wxFD_CHANGE_DIR);
	
	if ( !filename.empty() )
	{
		_loop_control->post_load_session (filename);
	}

}

void MainPanel::do_save_session (bool write_audio)
{
	wxString filename = do_file_selector (wxT("Choose file to save session"), wxT("slsess"), wxT("*.slsess"), wxFD_SAVE|wxFD_CHANGE_DIR|wxFD_OVERWRITE_PROMPT);
	
	if ( !filename.empty() )
	{
		// add .slsession if there isn't one already
		if (filename.size() <= 7 || (filename.size() > 7 && filename.substr(filename.size() - 7, 7) != wxT(".slsess"))) {
			filename += wxT(".slsess");
		}
		_loop_control->post_save_session (filename, write_audio);
	}

}

wxString MainPanel::do_file_selector(const wxString & message, const wxString & ext, const wxString & wc, int style)
{
	wxString filestring;

	_keyboard->set_enabled(false);
	
	if (_force_local || _loop_control->is_engine_local()) {
		wxFileName filename;
		if (_last_used_path.IsEmpty()) {
			_last_used_path = filename.GetHomeDir();
		}
		filestring = ::wxFileSelector(message, _last_used_path, wxT(""), ext, wc, style);
		if (!filestring.IsEmpty()) {
				filename.Assign(filestring);
				_last_used_path = filename.GetPath();
		}
	}
	else {
		// popup basic filename text entry
		filestring = ::wxGetTextFromUser(wxString::Format(wxT("%s on remote host '%s'"), message.c_str(),
								_loop_control->get_engine_host().c_str())
					       , message);
	}
	//cerr << "last used:" << _last_used_path.ToAscii() << endl;

	_keyboard->set_enabled(true);

	return filestring;
}


// @@@@@@@@@@@@@@@@@@@2

int AddCustomLoopDialog::num_loops = 1;
int AddCustomLoopDialog::num_channels = 2;
float AddCustomLoopDialog::secs_channel = 40.0f;
bool AddCustomLoopDialog::discrete = true;

AddCustomLoopDialog::AddCustomLoopDialog (MainPanel * parent, wxWindowID id, const wxString& title,
					  const wxPoint& pos, const wxSize& size)
	: wxDialog ((wxWindow *)parent, id, title, pos, size, wxCAPTION)
{
	_parent = parent;
       
	wxBoxSizer *mainsizer = new wxBoxSizer(wxVERTICAL);

	wxFlexGridSizer * setsizer = new wxFlexGridSizer(0, 2, 4, 4);


	wxStaticText * statText = new wxStaticText(this, -1, wxT("# Loops to add:"));
	setsizer->Add (statText, 0, wxALIGN_RIGHT|wxALIGN_CENTRE_VERTICAL);
	_num_loops_spin = new wxSpinCtrl(this, -1, wxT("1"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 16, num_loops);
	_num_loops_spin->SetValue (num_loops);
	setsizer->Add (_num_loops_spin, 0);

	statText = new wxStaticText(this, -1, wxT("# Channels per loop:"));
	setsizer->Add (statText, 0, wxALIGN_RIGHT|wxALIGN_CENTRE_VERTICAL);
	_num_channels_spin = new wxSpinCtrl(this, -1, wxT("2"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 16, num_channels);
	_num_channels_spin->SetValue (num_channels);
	setsizer->Add (_num_channels_spin, 0);

	
	_discrete_check = new wxCheckBox(this, -1, wxT("Individual Loop Ins/Outs"));
	_discrete_check->SetValue(discrete);
	setsizer->Add (_discrete_check, 0);
	setsizer->Add (1,1,1);

	
	statText = new wxStaticText(this, -1, wxT("Loop time (secs minimum):"));
	setsizer->Add (statText, 0, wxALIGN_RIGHT|wxALIGN_CENTRE_VERTICAL);
	_secs_per_channel_spin = new wxSpinCtrl(this, -1, wxT("20"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 4, 1000000, (int)secs_channel);
	_secs_per_channel_spin->SetValue ((int)secs_channel);
	setsizer->Add (_secs_per_channel_spin, 0);

	mainsizer->Add (setsizer, 1, wxEXPAND|wxALL, 6);


	wxBoxSizer * buttsizer = new wxBoxSizer(wxHORIZONTAL);

	buttsizer->Add (1,1,1);
	
	wxButton * butt = new wxButton(this, wxID_CANCEL, wxT("Cancel"));
	buttsizer->Add (butt, 0, wxALL, 5);

	butt = new wxButton(this, wxID_OK, wxT("OK"));
	buttsizer->Add (butt, 0, wxALL, 5);
	
	
	mainsizer->Add (buttsizer, 0, wxEXPAND|wxALL, 8);
	

	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( mainsizer );      // actually set the sizer
	mainsizer->Fit( this );            // set size to minimum size as calculated by the sizer
	mainsizer->SetSizeHints( this );   // set size hints to honour mininum size
	
}


// called by wxOK
bool AddCustomLoopDialog::TransferDataFromWindow ()
{
	// set them

	num_loops = _num_loops_spin->GetValue();
	num_channels = _num_channels_spin->GetValue();
	secs_channel = (float) _secs_per_channel_spin->GetValue();
	discrete = _discrete_check->GetValue();
	return true;
}

