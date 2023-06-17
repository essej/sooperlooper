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

#include <iostream>
#include <cstring>
#include <cmath>

#include "main_panel.hpp"
#include "keyboard_target.hpp"
#include "looper_panel.hpp"
#include "pix_button.hpp"
#include "loop_control.hpp"
#include "time_panel.hpp"
#include "slider_bar.hpp"
#include "choice_box.hpp"
#include "check_box.hpp"
#include "spin_box.hpp"

#include "pixmap_includes.hpp"

#include <midi_bind.hpp>

using namespace SooperLooper;
using namespace SooperLooperGui;
using namespace std;


enum {
	ID_UndoButton = 8000,
	ID_RedoButton,
	ID_RecordButton,
	ID_OverdubButton,
	ID_MultiplyButton,
	ID_InsertButton,
	ID_ReplaceButton,
	ID_TapButton,
	ID_ReverseButton,
	ID_SubstituteButton,
	ID_MuteButton,
	ID_PauseButton,
	ID_SoloButton,
	ID_RateButton,
	ID_ScratchButton,
	ID_LoadButton,
	ID_SaveButton,
	ID_OnceButton,
	ID_TrigButton,
	ID_OneXButton,
	ID_HalfXButton,
	ID_DoubleXButton,
	
	ID_ThreshControl,
	ID_FeedbackControl,
	ID_DryControl,
	ID_WetControl,
	ID_ScratchControl,
	ID_RateControl,
	ID_StretchControl,
	ID_PitchControl,
	ID_InputGainControl,

	ID_InputLatency,
	ID_OutputLatency,
	ID_TriggerLatency,
	
	ID_QuantizeCheck,
	ID_QuantizeChoice,
	ID_RoundCheck,
	ID_SyncCheck,
	ID_UseFeedbackPlayCheck,
	ID_TempoStretchCheck,
	ID_PlaySyncCheck,
	ID_UseMainInCheck,
	ID_Panner,
	ID_PrefaderCheck,
	ID_NameText,

	ID_FlashTimer

};

enum {
	FlashRate = 200
};


BEGIN_EVENT_TABLE(LooperPanel, wxPanel)

	EVT_TIMER(ID_FlashTimer, LooperPanel::on_flash_timer)
	EVT_TEXT_ENTER (ID_NameText, LooperPanel::on_text_event)

END_EVENT_TABLE()

	LooperPanel::LooperPanel(MainPanel * mainpan, LoopControl * control, wxWindow * parent, wxWindowID id, const wxPoint& pos, const wxSize& size)
	: wxPanel(parent, id, pos, size), _loop_control(control), _index(0), _last_state(LooperStateUnknown), _tap_val(1.0f)
{
	_mainpanel = mainpan;
	_learning = false;
	_scratch_pressed = false;
	_last_state = LooperStateUnknown;
	_chan_count = 0;
	_panners = 0;
	_has_discrete_io = false;
	_waiting = 0;
	_flashing_button = 0;

	_flash_timer = new wxTimer(this, ID_FlashTimer);
	
	init();
}

LooperPanel::~LooperPanel()
{

}


void
LooperPanel::init()
{
	SetBackgroundColour (*wxBLACK);
	SetThemeEnabled(false);

	wxBoxSizer * mainSizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer * mainVSizer = new wxBoxSizer(wxVERTICAL);

	wxBoxSizer * colsizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer * rowsizer;

	// add selbar
	_bgcolor.Set(0,0,0);
	_selbgcolor.Set(244, 255, 158);
	_learnbgcolor.Set(134, 80, 158);

	_leftSelbar = new wxPanel(this, -1, wxDefaultPosition, wxSize(4,-1));
	_leftSelbar->SetThemeEnabled(false);
	_leftSelbar->SetBackgroundColour (_bgcolor);


	_rightSelbar = new wxPanel(this, -1, wxDefaultPosition, wxSize(4,-1));
	_rightSelbar->SetThemeEnabled(false);
	_rightSelbar->SetBackgroundColour (_bgcolor);

	_topSelbar = new wxPanel(this, -1, wxDefaultPosition, wxSize(-1,4));
	_topSelbar->SetThemeEnabled(false);
	_topSelbar->SetBackgroundColour (_bgcolor);

	_bottomSelbar = new wxPanel(this, -1, wxDefaultPosition, wxSize(-1,4));
	_bottomSelbar->SetThemeEnabled(false);
	_bottomSelbar->SetBackgroundColour (_bgcolor);

	
	mainVSizer->Add (_topSelbar, 0, wxEXPAND|wxBOTTOM|wxLEFT, 0);
	mainSizer->Add (_leftSelbar, 0, wxEXPAND|wxBOTTOM|wxLEFT, 0);


	// create all buttons first, then add them to sizers
	// must do this because the bitmaps need to be loaded
	// before adding to sizer
	create_buttons();
	
	int edgegap = 0;

 	colsizer->Add (_undo_button, 0, 0, 0);

 	colsizer->Add (_redo_button, 0, wxTOP, 5);
	
	mainSizer->Add (colsizer, 0, wxEXPAND|wxLEFT, 5);

	
	colsizer = new wxBoxSizer(wxVERTICAL);

	
 	colsizer->Add (_record_button, 0, wxLEFT, 5);

 	colsizer->Add (_overdub_button, 0, wxTOP|wxLEFT, 5);

 	colsizer->Add (_multiply_button, 0, wxTOP|wxLEFT, 5);
	
	mainSizer->Add (colsizer, 0, wxEXPAND|wxBOTTOM, 0);


	colsizer = new wxBoxSizer(wxVERTICAL);
	_maininsizer = new wxBoxSizer(wxHORIZONTAL);

	SliderBar *slider;
	wxFont sliderFont = *wxSMALL_FONT;
	//cerr << "looper frame small: " << sliderFont.GetPointSize() << endl;
	
	wxBoxSizer * inthresh_sizer = new wxBoxSizer(wxHORIZONTAL);

	_in_gain_control = slider = new SliderBar(this, ID_InputGainControl, 0.0f, 1.0f, 0.0f);
	slider->set_units(wxT(""));
	slider->set_label(wxT("in gain"));
	slider->set_show_indicator_bar (false);
	slider->set_scale_mode(SliderBar::ZeroGainMode);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	inthresh_sizer->Add (slider, 1, wxALL|wxEXPAND, 0);
	
	_thresh_control = slider = new SliderBar(this, ID_ThreshControl, 0.0f, 1.0f, 0.0f);
	slider->set_units(wxT(""));
	slider->set_label(wxT("thresh"));
	slider->set_show_indicator_bar (true);
	slider->set_scale_mode(SliderBar::ZeroGainMode);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	inthresh_sizer->Add (slider, 1, wxLEFT|wxEXPAND, 3);
	
	colsizer->Add (inthresh_sizer, 1, wxEXPAND|wxLEFT, 5);

	_feedback_control = slider = new SliderBar(this, ID_FeedbackControl, 0.0f, 100.0f, 100.0f);
	slider->set_units(wxT("%"));
	slider->set_label(wxT("feedback"));
	slider->SetFont(sliderFont);
	slider->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));

	_maininsizer->Add (slider, 1, wxEXPAND|wxTOP, 5);

	// mainin check added later
	_use_main_in_check = 0;
	
	colsizer->Add (_maininsizer, 1, wxEXPAND|wxLEFT, 5);


	
	//colsizer->Add (20, 5, 0, wxEXPAND);
	rowsizer = new wxBoxSizer(wxHORIZONTAL);
 	rowsizer->Add (_replace_button, 0, wxTOP|wxLEFT, 5);

 	rowsizer->Add (_insert_button, 0, wxTOP|wxLEFT, 5);

	colsizer->Add (rowsizer, 0);

	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	
 	rowsizer->Add (_substitute_button, 0, wxTOP|wxLEFT, 5);

	//_reverse_button->Show(false);
 	rowsizer->Add (_delay_button, 0, wxTOP|wxLEFT, 5);

	colsizer->Add (rowsizer, 0);
	
	mainSizer->Add (colsizer, 0, wxEXPAND, 5);
	

	// time area
	colsizer = new wxBoxSizer(wxVERTICAL);

	_time_panel = new TimePanel(_loop_control, this, -1);
	_time_panel->set_index (_index);
	
	colsizer->Add (_time_panel, 0, wxLEFT, 5);

	//colsizer->Add (20, -1, 1);
	_toppansizer = new wxBoxSizer(wxHORIZONTAL);

	// dry is added later
// 	_dry_control = slider = new SliderBar(this, ID_DryControl, 0.0f, 1.0f, 1.0f);
// 	slider->set_units(wxT("dB"));
// 	slider->set_label(wxT("dry"));
// 	slider->set_scale_mode(SliderBar::ZeroGainMode);
// 	slider->SetFont(sliderFont);
// 	slider->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) slider->GetId()));
// 	slider->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
// 	_toppansizer->Add (slider, 1, wxEXPAND, 0);

	// panners are added later
	
	colsizer->Add (_toppansizer, 1, wxEXPAND|wxTOP|wxLEFT, 4);

	_botpansizer = new wxBoxSizer(wxHORIZONTAL);

	_wet_control = slider = new SliderBar(this, ID_WetControl, 0.0f, 1.0f, 1.0f);
	slider->set_units(wxT("dB"));
	slider->set_label(wxT("out"));
	slider->set_show_indicator_bar (true);
	slider->set_scale_mode(SliderBar::ZeroGainMode);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	_botpansizer->Add (slider, 1, wxEXPAND, 0);

	/*
	_outlatency_spin =  new SpinBox(this, ID_OutputLatency, 0.0f, 32768.0f, 0.0f, true, wxDefaultPosition, wxSize(65, 20));
	_outlatency_spin->set_units(wxT(""));
	_outlatency_spin->set_label(wxT("o.lat"));
	_outlatency_spin->set_snap_mode (SpinBox::IntegerSnap);
	_outlatency_spin->set_allow_outside_bounds(true);
	_outlatency_spin->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) _outlatency_spin->GetId()));
	_outlatency_spin->SetFont(sliderFont);
	_botpansizer->Add (_outlatency_spin, 0, wxALL, 0);

	_inlatency_spin =  new SpinBox(this, ID_InputLatency, 0.0f, 32768.0f, 0.0f, true, wxDefaultPosition, wxSize(65, 20));
	_inlatency_spin->set_units(wxT(""));
	_inlatency_spin->set_label(wxT("i.lat"));
	_inlatency_spin->set_snap_mode (SpinBox::IntegerSnap);
	_inlatency_spin->set_allow_outside_bounds(true);
	_inlatency_spin->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) _inlatency_spin->GetId()));
	_inlatency_spin->SetFont(sliderFont);
	_botpansizer->Add (_inlatency_spin, 0, wxALL, 0);
	*/
	
	colsizer->Add (_botpansizer, 1, wxEXPAND|wxTOP|wxLEFT, 4);
	
	
	mainSizer->Add (colsizer, 0, wxEXPAND, 5);


	//

	colsizer = new wxBoxSizer(wxVERTICAL);
	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer * lilcolsizer = new wxBoxSizer(wxVERTICAL);
	
// 	_quantize_choice = new ChoiceBox (this, ID_QuantizeChoice, wxDefaultPosition, wxSize (110, 22));
// 	_quantize_choice->set_label (wxT("quantize"));
// 	_quantize_choice->SetFont (sliderFont);
// 	_quantize_choice->value_changed.connect (mem_fun (*this,  &LooperPanel::on_quantize_change));
// 	_quantize_choice->append_choice (wxT("off"));
// 	_quantize_choice->append_choice (wxT("cycle"));
// 	_quantize_choice->append_choice (wxT("8th"));
// 	_quantize_choice->append_choice (wxT("loop"));
//	lilcolsizer->Add (_quantize_choice, 0);

// 	_quantize_check = new wxCheckBox(this, ID_QuantizeCheck, "quantize");
// 	_quantize_check->SetFont(sliderFont);
// 	_quantize_check->SetBackgroundColour(wxColour(90,90,90));
// 	_quantize_check->SetForegroundColour(*wxWHITE);
// 	lilrowsizer->Add (_quantize_check, 0, wxEXPAND);
	
	
	wxBoxSizer * lilrowsizer = new wxBoxSizer(wxHORIZONTAL);
	
// 	_round_check = new wxCheckBox(this, ID_RoundCheck, "round");
// 	_round_check->SetFont(sliderFont);
// 	_round_check->SetBackgroundColour(wxColour(90,90,90));
// 	_round_check->SetForegroundColour(*wxWHITE);
// 	lilrowsizer->Add (_round_check, 0, wxEXPAND);

	_sync_check = new CheckBox(this, ID_SyncCheck, wxT("sync"), true, wxDefaultPosition, wxSize(55, 18));
	_sync_check->SetFont(sliderFont);
	_sync_check->SetToolTip(wxT("sync operations to quantize source"));
	_sync_check->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::check_events), wxT("sync")));
	_sync_check->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) _sync_check->GetId()));
	lilrowsizer->Add (_sync_check, 1, wxLEFT, 3);

	_name_text = new wxTextCtrl(this, ID_NameText, wxT(""), wxDefaultPosition, wxSize(40, 18), wxTE_PROCESS_ENTER|wxTE_LEFT, wxDefaultValidator, wxT("KeyAware"));
	_name_text->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	_name_text->SetToolTip(wxT("loop name"));
	_name_text->SetFont(sliderFont);
	lilrowsizer->Add (_name_text, 1, wxLEFT|wxALIGN_CENTRE_VERTICAL, 3);

	lilcolsizer->Add (lilrowsizer, 0, wxTOP|wxEXPAND, 0);

	lilrowsizer = new wxBoxSizer(wxHORIZONTAL);
	_play_sync_check = new CheckBox(this, ID_PlaySyncCheck, wxT("play sync"), true, wxDefaultPosition, wxSize(55, 18));
	_play_sync_check->SetFont(sliderFont);
	_play_sync_check->SetToolTip(wxT("sync playback auto-triggering to quantized sync source"));
	_play_sync_check->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::check_events), wxT("playback_sync")));
	_play_sync_check->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) _play_sync_check->GetId()));
	lilrowsizer->Add (_play_sync_check, 1, wxLEFT, 3);
    
	_prefader_check = new CheckBox(this, ID_PrefaderCheck, wxT("prefader"), true, wxDefaultPosition, wxSize(55, 18));
	_prefader_check->SetFont(sliderFont);
	_prefader_check->SetToolTip(wxT("discrete outputs are pre-fader"));
	_prefader_check->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::check_events), wxT("discrete_prefader")));
	_prefader_check->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) _prefader_check->GetId()));
	lilrowsizer->Add (_prefader_check, 1, wxLEFT, 3);
    
	lilcolsizer->Add (lilrowsizer, 0, wxTOP|wxEXPAND, 0);
	
	lilrowsizer = new wxBoxSizer(wxHORIZONTAL);
	_play_feed_check = new CheckBox(this, ID_UseFeedbackPlayCheck, wxT("p. feedb"), true, wxDefaultPosition, wxSize(55, 18));
	_play_feed_check->SetFont(sliderFont);
	_play_feed_check->SetToolTip(wxT("enable feedback during playback"));
	_play_feed_check->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::check_events), wxT("use_feedback_play")));
	_play_feed_check->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) _play_feed_check->GetId()));
	lilrowsizer->Add (_play_feed_check, 1, wxLEFT, 3);

	_tempo_stretch_check = new CheckBox(this, ID_TempoStretchCheck, wxT("t. stretch"), true, wxDefaultPosition, wxSize(55, 18));
	_tempo_stretch_check->SetFont(sliderFont);
	_tempo_stretch_check->SetToolTip(wxT("enable automatic timestretch when tempo changes"));
	_tempo_stretch_check->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::check_events), wxT("tempo_stretch")));
	_tempo_stretch_check->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) _tempo_stretch_check->GetId()));
	lilrowsizer->Add (_tempo_stretch_check, 1, wxLEFT, 3);

	lilcolsizer->Add (lilrowsizer, 0, wxTOP|wxEXPAND, 0);
	
	rowsizer->Add(lilcolsizer, 1, wxLEFT, 3);


	lilcolsizer = new wxBoxSizer(wxVERTICAL);
	
	lilcolsizer->Add (_load_button, 0, wxTOP, 0);

	lilcolsizer->Add (_save_button, 0, wxTOP, 2);
	
	rowsizer->Add(lilcolsizer, 0, wxLEFT, 3);

	lilcolsizer = new wxBoxSizer(wxVERTICAL);
	
	lilcolsizer->Add (_trig_button, 0, wxTOP, 0);

	lilcolsizer->Add (_once_button, 0, wxTOP, 2);
	
	rowsizer->Add(lilcolsizer, 0, wxLEFT, 3);
	
	lilcolsizer = new wxBoxSizer(wxVERTICAL);
	
 	lilcolsizer->Add (_mute_button, 0, wxTOP, 0);

 	lilcolsizer->Add (_solo_button, 0, wxTOP, 2);

	rowsizer->Add(lilcolsizer, 0, wxLEFT, 3);	

	colsizer->Add (rowsizer, 0, wxEXPAND);

	colsizer->Add (20,-1, 1);
	

	// scratch stuff
	rowsizer = new wxBoxSizer(wxHORIZONTAL);

 	rowsizer->Add (_reverse_button, 0, wxTOP|wxLEFT, 3);

	rowsizer->Add (_scratch_button, 0, wxTOP|wxLEFT, 3);

	// scratch control
	_scratch_control = slider = new SliderBar(this, ID_ScratchControl, 0.0f, 1.0f, 0.0f);
	slider->set_units(wxT(""));
	slider->set_label(wxT("pos"));
	slider->set_style (SliderBar::HiddenStyle);
	slider->set_decimal_digits (3);
	slider->set_show_value(false);
	slider->set_show_indicator_bar (true);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	rowsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 3);

	// pitch control
	_pitch_control = slider = new SliderBar(this, ID_PitchControl, -12.0f, 12.0f, 0.0f);
	slider->set_units(wxT(""));
	slider->set_label(wxT("pitch"));
	slider->set_style (SliderBar::CenterStyle);
	slider->set_decimal_digits (1);
	slider->set_snap_mode(SliderBar::IntegerSnap);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	rowsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 3);

	// pause
 	rowsizer->Add (_pause_button, 0, wxTOP|wxLEFT, 3);

	
	colsizer->Add (rowsizer, 0, wxEXPAND);

	// rate stuff
	rowsizer = new wxBoxSizer(wxHORIZONTAL);

 	rowsizer->Add (_halfx_button, 0, wxTOP|wxLEFT, 3);
 	rowsizer->Add (_1x_button, 0, wxTOP|wxLEFT, 3);
 	rowsizer->Add (_2x_button, 0, wxTOP|wxLEFT, 3);

	// rate control
	_rate_control = slider = new SliderBar(this, ID_RateControl, 0.25f, 4.0f, 1.0f);
	slider->set_units(wxT(""));
	slider->set_label(wxT("rate"));
	slider->set_style (SliderBar::CenterStyle);
	slider->set_decimal_digits (3);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	rowsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 3);

	// stretch control
	_stretch_control = slider = new SliderBar(this, ID_StretchControl, 0.5f, 4.0f, 1.0f);
	slider->set_units(wxT(""));
	slider->set_label(wxT("stretch"));
	slider->set_style (SliderBar::CenterStyle);
	slider->set_decimal_digits (2);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	rowsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 3);


	/*
	_triglatency_spin =  new SpinBox(this, ID_TriggerLatency, 0.0f, 32768.0f, 0.0f, true, wxDefaultPosition, wxSize(65, 20));
	_triglatency_spin->set_units(wxT(""));
	_triglatency_spin->set_label(wxT("t.lat"));
	_triglatency_spin->set_snap_mode (SpinBox::IntegerSnap);
	_triglatency_spin->set_allow_outside_bounds(true);
	_triglatency_spin->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) _triglatency_spin->GetId()));
	_triglatency_spin->SetFont(sliderFont);
	rowsizer->Add (_triglatency_spin, 0, wxALL, 0);
	*/
	
	colsizer->Add (rowsizer, 0, wxEXPAND|wxLEFT, 1);

	
	mainSizer->Add (colsizer, 1, wxEXPAND|wxRIGHT, 0);

        mainSizer->Add (_rightSelbar, 0, wxEXPAND|wxLEFT, 0);


        mainVSizer->Add (mainSizer, 1, wxEXPAND, 0);
        mainVSizer->Add (_bottomSelbar, 0, wxEXPAND|wxLEFT, 0);


	// add an index static text fixed position

// 	_index_text = new wxStaticText(this, -1, wxString::Format(wxT("%d"), _index+1), wxPoint(4,4), wxDefaultSize);
// 	_index_text->SetForegroundColour(wxColour(14, 50, 89));
// 	wxFont textfont(12, wxSWISS, wxNORMAL, wxBOLD);
// 	_index_text->SetFont(textfont);
// 	_index_text->Raise();
	
	bind_events();
	
	
	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( mainVSizer );      // actually set the sizer
	mainVSizer->Fit( this );            // set size to minimum size as calculated by the sizer
	mainVSizer->SetSizeHints( this );   // set size hints to honour mininum size

}

void
LooperPanel::post_init()
{
	// now we have channel count
	SliderBar * slider;
	wxFont sliderFont = *wxSMALL_FONT;
	
	_panners = new SliderBar*[_chan_count];
	int barwidth = _chan_count <= 4 ? 50 : 30;

	// without discrete i/o mains are the only option
	float val;
	if (_loop_control->get_value(_index, wxT("has_discrete_io"), val) && val != 0.0f)
	{
		_has_discrete_io = true;

		// dry is only meaningful with discrete io
		_dry_control = slider = new SliderBar(this, ID_DryControl, 0.0f, 1.0f, 1.0f);
		slider->set_units(wxT("dB"));
		slider->set_label(wxT("in mon"));
		slider->set_scale_mode(SliderBar::ZeroGainMode);
		slider->SetFont(sliderFont);
		slider->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::slider_events), (int) slider->GetId()));
		slider->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
		_toppansizer->Add (slider, 1, wxEXPAND, 0);

		_use_main_in_check = new CheckBox(this, ID_UseMainInCheck, wxT("main in"), true, wxDefaultPosition, wxSize(65, 18));
		_use_main_in_check->SetFont(sliderFont);
		_use_main_in_check->SetToolTip(wxT("mix input from Main inputs"));
		_use_main_in_check->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::check_events), wxT("use_common_ins")));
		_use_main_in_check->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::control_bind_events), (int) _use_main_in_check->GetId()));
		//_maininsizer->Add (_use_main_in_check, 0, wxALL|wxEXPAND|wxALIGN_CENTRE_VERTICAL ,0);
		_maininsizer->Add (_use_main_in_check, 0, wxALL|wxALIGN_CENTRE_VERTICAL ,0);
		_maininsizer->Layout();

		_feedback_control->set_label(wxT("feedb"));

	}
	else {
		_has_discrete_io = false;
		_dry_control = 0;
	}
	
	
	for (int i=0; i < _chan_count; ++i)
	{
		float defval = 0.5f;
		if (_chan_count == 2) {
			defval = (i == 0) ? 0.0f : 1.0f;
		}
		
		_panners[i] = slider =  new SliderBar(this, ID_Panner, 0.0f, 1.0f, defval, true, wxDefaultPosition, wxSize(barwidth,-1));
		slider->set_units(wxT(""));
		if (_chan_count > 1) {
			slider->set_label(wxString::Format(wxT("pan %d"), i+1));
		}
		else {
			slider->set_label(wxString::Format(wxT("pan")));
		}
		slider->set_style (SliderBar::CenterStyle);
		slider->set_decimal_digits (3);
		slider->set_show_value (false);
		slider->SetFont(sliderFont);
		slider->value_changed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pan_events), (int) i));
		slider->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::pan_bind_events), (int) i));

		if (!_has_discrete_io) {
			_toppansizer->Add (slider, 1, (i==0) ? wxEXPAND : wxEXPAND|wxLEFT, 2);
		}
		else if (_chan_count <= 2 || i < (int) ceil(_chan_count*0.5)) {
			_toppansizer->Add (slider, 0, wxEXPAND|wxLEFT, 2);
		}
		else {
			_botpansizer->Add (slider, 0, wxEXPAND|wxLEFT, 2);
		}

	}

	_toppansizer->Layout();
	_botpansizer->Layout();
	GetSizer()->Layout();
	Refresh(false);
}

void
LooperPanel::set_selected (bool flag)
{
	if (flag) {

		_leftSelbar->SetBackgroundColour (_selbgcolor);
		_rightSelbar->SetBackgroundColour (_selbgcolor);
		_bottomSelbar->SetBackgroundColour (_selbgcolor);
		_topSelbar->SetBackgroundColour (_selbgcolor);
		//this->SetBackgroundColour (_selbgcolor);
	}
	else {
		_leftSelbar->SetBackgroundColour (_bgcolor);
		_rightSelbar->SetBackgroundColour (_bgcolor);
		_topSelbar->SetBackgroundColour (_bgcolor);
		_bottomSelbar->SetBackgroundColour (_bgcolor);
		//this->SetBackgroundColour (_bgcolor);
	}

	_leftSelbar->Refresh();
	_rightSelbar->Refresh();
	_bottomSelbar->Refresh();
	_topSelbar->Refresh();
}

void
LooperPanel::set_index(int ind)
{
	_index = ind;
	_time_panel->set_index (_index);
}


void
LooperPanel::bind_events()
{
	_undo_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("undo"))));
	_undo_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("undo"))));
	_undo_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("undo"))));

	_redo_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("redo"))));
	_redo_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("redo"))));
	_redo_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("redo"))));

	_record_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("record"))));
	_record_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("record"))));
	_record_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("record"))));

	_overdub_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("overdub"))));
	_overdub_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("overdub"))));
	_overdub_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("overdub"))));

	_multiply_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("multiply"))));
	_multiply_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("multiply"))));
	_multiply_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("multiply"))));

	_replace_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("replace"))));
	_replace_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("replace"))));
	_replace_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("replace"))));

	_insert_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("insert"))));
	_insert_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("insert"))));
	_insert_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("insert"))));

	_once_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("oneshot"))));
	_once_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("oneshot"))));
	_once_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("oneshot"))));

	_trig_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("trigger"))));
	_trig_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("trigger"))));
	_trig_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("trigger"))));

	_delay_button->pressed.connect (mem_fun (*this, &LooperPanel::delay_button_press_event));
	_delay_button->released.connect (mem_fun (*this, &LooperPanel::delay_button_release_event));
	_delay_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("delay_trigger"))));

	_reverse_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("reverse"))));
	_reverse_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("reverse"))));
	_reverse_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("reverse"))));

	_substitute_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("substitute"))));
	_substitute_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("substitute"))));
	_substitute_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("substitute"))));
	
	_mute_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("mute"))));
	_mute_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("mute"))));
	_mute_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("mute"))));

	_pause_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("pause"))));
	_pause_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("pause"))));
	_pause_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("pause"))));

	_solo_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("solo"))));
	_solo_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("solo"))));
	_solo_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("solo"))));

	_halfx_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::rate_button_event), 0.5f));
	_halfx_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::rate_bind_events), 0.5f));
	_1x_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::rate_button_event), 1.0f));
	_1x_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::rate_bind_events), 1.0f));
	_2x_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::rate_button_event), 2.0f));
	_2x_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::rate_bind_events), 2.0f));

	_scratch_button->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::pressed_events), wxString(wxT("scratch"))));
	_scratch_button->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::released_events), wxString(wxT("scratch"))));
	_scratch_button->bind_request.connect (sigc::bind(mem_fun (*this, &LooperPanel::button_bind_events), wxString(wxT("scratch"))));

	_save_button->clicked.connect (sigc::bind(mem_fun (*this, &LooperPanel::clicked_events), wxString(wxT("save"))));
	_load_button->clicked.connect (sigc::bind(mem_fun (*this, &LooperPanel::clicked_events), wxString(wxT("load"))));


	_scratch_control->pressed.connect (sigc::bind(mem_fun (*this, &LooperPanel::scratch_events), wxString(wxT("scratch_press"))));
	_scratch_control->released.connect (sigc::bind(mem_fun (*this, &LooperPanel::scratch_events), wxString(wxT("scratch_release"))));

	
	_loop_control->MidiBindingChanged.connect (mem_fun (*this, &LooperPanel::got_binding_changed));
	_loop_control->MidiLearnCancelled.connect (mem_fun (*this, &LooperPanel::got_learn_canceled));
	
}

void LooperPanel::create_buttons()
{
 	_undo_button = new PixButton(this, ID_UndoButton);
 	_redo_button = new PixButton(this, ID_RedoButton);
 	_record_button = new PixButton(this, ID_RecordButton);
 	_overdub_button = new PixButton(this, ID_OverdubButton);
 	_multiply_button = new PixButton(this, ID_MultiplyButton);
 	_replace_button = new PixButton(this, ID_ReplaceButton);
 	_delay_button = new PixButton(this, ID_TapButton);
 	_insert_button = new PixButton(this, ID_InsertButton);
 	_reverse_button = new PixButton(this, ID_ReverseButton);
	//_reverse_button->SetToolTip(wxT("reverses direction"));
 	_substitute_button = new PixButton(this, ID_SubstituteButton);
 	_load_button = new PixButton(this, ID_LoadButton, false);
 	_save_button = new PixButton(this, ID_SaveButton, false);
 	_trig_button = new PixButton(this, ID_TrigButton);
 	_once_button = new PixButton(this, ID_OnceButton);
 	_mute_button = new PixButton(this, ID_MuteButton);
 	_pause_button = new PixButton(this, ID_PauseButton);
 	_solo_button = new PixButton(this, ID_SoloButton);
 	_scratch_button = new PixButton(this, ID_ScratchButton);
 	_halfx_button = new PixButton(this, ID_HalfXButton, true);
 	_1x_button = new PixButton(this, ID_OneXButton, true);
	_2x_button = new PixButton(this, ID_DoubleXButton, true);

	
	// load them all up manually
	_undo_button->set_normal_bitmap (wxBitmap(undo_normal));
	_undo_button->set_selected_bitmap (wxBitmap(undo_selected));
	_undo_button->set_focus_bitmap (wxBitmap(undo_focus));
	_undo_button->set_disabled_bitmap (wxBitmap(undo_disabled));
	_undo_button->set_active_bitmap (wxBitmap(undo_active));
	
	_redo_button->set_normal_bitmap (wxBitmap(redo_normal));
	_redo_button->set_selected_bitmap (wxBitmap(redo_selected));
	_redo_button->set_focus_bitmap (wxBitmap(redo_focus));
	_redo_button->set_disabled_bitmap (wxBitmap(redo_disabled));
	_redo_button->set_active_bitmap (wxBitmap(redo_active));

	_record_button->set_normal_bitmap (wxBitmap(record_normal));
	_record_button->set_selected_bitmap (wxBitmap(record_selected));
	_record_button->set_focus_bitmap (wxBitmap(record_focus));
	_record_button->set_disabled_bitmap (wxBitmap(record_disabled));
	_record_button->set_active_bitmap (wxBitmap(record_active));

	_overdub_button->set_normal_bitmap (wxBitmap(overdub_normal));
	_overdub_button->set_selected_bitmap (wxBitmap(overdub_selected));
	_overdub_button->set_focus_bitmap (wxBitmap(overdub_focus));
	_overdub_button->set_disabled_bitmap (wxBitmap(overdub_disabled));
	_overdub_button->set_active_bitmap (wxBitmap(overdub_active));

	_multiply_button->set_normal_bitmap (wxBitmap(multiply_normal));
	_multiply_button->set_selected_bitmap (wxBitmap(multiply_selected));
	_multiply_button->set_focus_bitmap (wxBitmap(multiply_focus));
	_multiply_button->set_disabled_bitmap (wxBitmap(multiply_disabled));
	_multiply_button->set_active_bitmap (wxBitmap(multiply_active));

	_replace_button->set_normal_bitmap (wxBitmap(replace_normal));
	_replace_button->set_selected_bitmap (wxBitmap(replace_selected));
	_replace_button->set_focus_bitmap (wxBitmap(replace_focus));
	_replace_button->set_disabled_bitmap (wxBitmap(replace_disabled));
	_replace_button->set_active_bitmap (wxBitmap(replace_active));

	_delay_button->set_normal_bitmap (wxBitmap(delay_normal));
	_delay_button->set_selected_bitmap (wxBitmap(delay_selected));
	_delay_button->set_focus_bitmap (wxBitmap(delay_focus));
	_delay_button->set_disabled_bitmap (wxBitmap(delay_disabled));
	_delay_button->set_active_bitmap (wxBitmap(delay_active));

	_insert_button->set_normal_bitmap (wxBitmap(insert_normal));
	_insert_button->set_selected_bitmap (wxBitmap(insert_selected));
	_insert_button->set_focus_bitmap (wxBitmap(insert_focus));
	_insert_button->set_disabled_bitmap (wxBitmap(insert_disabled));
	_insert_button->set_active_bitmap (wxBitmap(insert_active));

	_reverse_button->set_normal_bitmap (wxBitmap(reverse_normal));
	_reverse_button->set_selected_bitmap (wxBitmap(reverse_selected));
	_reverse_button->set_focus_bitmap (wxBitmap(reverse_focus));
	_reverse_button->set_disabled_bitmap (wxBitmap(reverse_disabled));
	_reverse_button->set_active_bitmap (wxBitmap(reverse_active));

	_substitute_button->set_normal_bitmap (wxBitmap(substitute_normal));
	_substitute_button->set_selected_bitmap (wxBitmap(substitute_selected));
	_substitute_button->set_focus_bitmap (wxBitmap(substitute_focus));
	_substitute_button->set_disabled_bitmap (wxBitmap(substitute_disabled));
	_substitute_button->set_active_bitmap (wxBitmap(substitute_active));
	
	_mute_button->set_normal_bitmap (wxBitmap(mute_normal));
	_mute_button->set_selected_bitmap (wxBitmap(mute_selected));
	_mute_button->set_focus_bitmap (wxBitmap(mute_focus));
	_mute_button->set_disabled_bitmap (wxBitmap(mute_disabled));
	_mute_button->set_active_bitmap (wxBitmap(mute_active));

	_pause_button->set_normal_bitmap (wxBitmap(pause_normal));
	_pause_button->set_selected_bitmap (wxBitmap(pause_selected));
	_pause_button->set_focus_bitmap (wxBitmap(pause_focus));
	_pause_button->set_disabled_bitmap (wxBitmap(pause_disabled));
	_pause_button->set_active_bitmap (wxBitmap(pause_active));

	_solo_button->set_normal_bitmap (wxBitmap(solo_normal));
	_solo_button->set_selected_bitmap (wxBitmap(solo_selected));
	_solo_button->set_focus_bitmap (wxBitmap(solo_focus));
	_solo_button->set_disabled_bitmap (wxBitmap(solo_disabled));
	_solo_button->set_active_bitmap (wxBitmap(solo_active));

	_scratch_button->set_normal_bitmap (wxBitmap(scratch_normal));
	_scratch_button->set_selected_bitmap (wxBitmap(scratch_selected));
	_scratch_button->set_focus_bitmap (wxBitmap(scratch_focus));
	_scratch_button->set_disabled_bitmap (wxBitmap(scratch_disabled));
	_scratch_button->set_active_bitmap (wxBitmap(scratch_active));

	_load_button->set_normal_bitmap (wxBitmap(load_normal));
	_load_button->set_selected_bitmap (wxBitmap(load_selected));
	_load_button->set_focus_bitmap (wxBitmap(load_focus));
	_load_button->set_disabled_bitmap (wxBitmap(load_disabled));
	_load_button->set_active_bitmap (wxBitmap(load_active));

	_save_button->set_normal_bitmap (wxBitmap(save_normal));
	_save_button->set_selected_bitmap (wxBitmap(save_selected));
	_save_button->set_focus_bitmap (wxBitmap(save_focus));
	_save_button->set_disabled_bitmap (wxBitmap(save_disabled));
	_save_button->set_active_bitmap (wxBitmap(save_active));

	_once_button->set_normal_bitmap (wxBitmap(once_normal));
	_once_button->set_selected_bitmap (wxBitmap(once_selected));
	_once_button->set_focus_bitmap (wxBitmap(once_focus));
	_once_button->set_disabled_bitmap (wxBitmap(once_disabled));
	_once_button->set_active_bitmap (wxBitmap(once_active));

	_trig_button->set_normal_bitmap (wxBitmap(trig_normal));
	_trig_button->set_selected_bitmap (wxBitmap(trig_selected));
	_trig_button->set_focus_bitmap (wxBitmap(trig_focus));
	_trig_button->set_disabled_bitmap (wxBitmap(trig_disabled));
	_trig_button->set_active_bitmap (wxBitmap(trig_active));

	_1x_button->set_normal_bitmap (wxBitmap(onex_rate_normal));
	_1x_button->set_selected_bitmap (wxBitmap(onex_rate_selected));
	_1x_button->set_focus_bitmap (wxBitmap(onex_rate_focus));
	_1x_button->set_disabled_bitmap (wxBitmap(onex_rate_disabled));
	_1x_button->set_active_bitmap (wxBitmap(onex_rate_active));

	_2x_button->set_normal_bitmap (wxBitmap(double_rate_normal));
	_2x_button->set_selected_bitmap (wxBitmap(double_rate_selected));
	_2x_button->set_focus_bitmap (wxBitmap(double_rate_focus));
	_2x_button->set_disabled_bitmap (wxBitmap(double_rate_disabled));
	_2x_button->set_active_bitmap (wxBitmap(double_rate_active));

	_halfx_button->set_normal_bitmap (wxBitmap(half_rate_normal));
	_halfx_button->set_selected_bitmap (wxBitmap(half_rate_selected));
	_halfx_button->set_focus_bitmap (wxBitmap(half_rate_focus));
	_halfx_button->set_disabled_bitmap (wxBitmap(half_rate_disabled));
	_halfx_button->set_active_bitmap (wxBitmap(half_rate_active));

	
}



void
LooperPanel::update_controls()
{
	// get recent controls from loop control
	float val;

	// first see if we have channel count yet
	if (_chan_count == 0 && _loop_control->is_updated(_index, wxT("channel_count"))
	    && _loop_control->is_updated(_index, wxT("has_discrete_io")))
	{
		_loop_control->get_value(_index, wxT("channel_count"), val);
		_chan_count = (int) val;
		// do post_init
		post_init();
	}
	
	if (_loop_control->is_updated(_index, wxT("feedback"))) {
		_loop_control->get_value(_index, wxT("feedback"), val);
		_feedback_control->set_value ((val * 100.0f));
	}
	if (_loop_control->is_updated(_index, wxT("input_gain"))) {
		_loop_control->get_value(_index, wxT("input_gain"), val);
		_in_gain_control->set_value (val);
	}
	if (_loop_control->is_updated(_index, wxT("rec_thresh"))) {
		_loop_control->get_value(_index, wxT("rec_thresh"), val);
		_thresh_control->set_value (val);
	}

	if (_has_discrete_io) {
		if (_loop_control->is_updated(_index, wxT("in_peak_meter"))) {
			_loop_control->get_value(_index, wxT("in_peak_meter"), val);
			_thresh_control->set_indicator_value (val);
		}

		if (_loop_control->is_updated(_index, wxT("dry"))) {
			_loop_control->get_value(_index, wxT("dry"), val);
			_dry_control->set_value (val);
		}
	}
	else {
		if (_loop_control->is_updated(_index, wxT("in_peak_meter"))) {
			_loop_control->get_value(_index, wxT("in_peak_meter"), val);
			_thresh_control->set_indicator_value (val);
		}
	}

	if (_loop_control->is_updated(_index, wxT("out_peak_meter"))) {
		_loop_control->get_value(_index, wxT("out_peak_meter"), val);
		_wet_control->set_indicator_value (val);
	}
	if (_loop_control->is_updated(_index, wxT("wet"))) {
		_loop_control->get_value(_index, wxT("wet"), val);
		_wet_control->set_value (val);
	}
	if (_loop_control->is_updated(_index, wxT("rate"))) {
		_loop_control->get_value(_index, wxT("rate"), val);
		_rate_control->set_value (val);

		update_rate_buttons(val);
	}
	if (_loop_control->is_updated(_index, wxT("stretch_ratio"))) {
		_loop_control->get_value(_index, wxT("stretch_ratio"), val);
		_stretch_control->set_value (val);
	}
	if (_loop_control->is_updated(_index, wxT("pitch_shift"))) {
		_loop_control->get_value(_index, wxT("pitch_shift"), val);
		_pitch_control->set_value (val);
	}
	if (_loop_control->is_updated(_index, wxT("rate_output"))) {
		_loop_control->get_value(_index, wxT("rate_output"), val);
		if (val < 0.0) {
			_reverse_button->set_active(true);
		}
		else {
			_reverse_button->set_active(false);
		}
	}
	if (_loop_control->is_updated(_index, wxT("scratch_pos"))) {
		_loop_control->get_value(_index, wxT("scratch_pos"), val);
		_scratch_control->set_value (val);
		_scratch_control->set_indicator_value (val);
	}

	/*
	if (_loop_control->is_updated(_index, wxT("output_latency"))) {
		_loop_control->get_value(_index, wxT("output_latency"), val);
		_outlatency_spin->set_value (val);
	}
	if (_loop_control->is_updated(_index, wxT("input_latency"))) {
		_loop_control->get_value(_index, wxT("input_latency"), val);
		_inlatency_spin->set_value (val);
	}
	if (_loop_control->is_updated(_index, wxT("trigger_latency"))) {
		_loop_control->get_value(_index, wxT("trigger_latency"), val);
		_triglatency_spin->set_value (val);
	}
	*/
	
// 	if (_loop_control->is_updated(_index, "quantize")) {
// 		_loop_control->get_value(_index, "quantize", val);
// 		//_quantize_choice->set_index_value ((int)val);
// 		_quantize_check->SetValue (val > 0.0);
// 	}
// 	if (_loop_control->is_updated(_index, "round")) {
// 		_loop_control->get_value(_index, "round", val);
// 		_round_check->SetValue (val > 0.0);
// 	}
	if (_loop_control->is_updated(_index, wxT("sync"))) {
		_loop_control->get_value(_index, wxT("sync"), val);
		_sync_check->set_value (val > 0.0f);
	}
	if (_loop_control->is_updated(_index, wxT("playback_sync"))) {
		_loop_control->get_value(_index, wxT("playback_sync"), val);
		_play_sync_check->set_value (val > 0.0f);
	}
	if (_loop_control->is_updated(_index, wxT("use_feedback_play"))) {
		_loop_control->get_value(_index, wxT("use_feedback_play"), val);
		_play_feed_check->set_value (val > 0.0f);
	}
	if (_loop_control->is_updated(_index, wxT("tempo_stretch"))) {
		_loop_control->get_value(_index, wxT("tempo_stretch"), val);
		_tempo_stretch_check->set_value (val > 0.0f);
	}
	if (_loop_control->is_updated(_index, wxT("discrete_prefader"))) {
		_loop_control->get_value(_index, wxT("discrete_prefader"), val);
		_prefader_check->set_value (val > 0.0f);
	}
    
	if (_loop_control->is_updated(_index, wxT("is_soloed"))) {
		_loop_control->get_value(_index, wxT("is_soloed"), val);
		_solo_button->set_active(val > 0.0f);
	}

	if (_loop_control->is_updated(_index, wxT("name"))) {
		wxString prop;
		_loop_control->get_property(_index, wxT("name"), prop);
		if (prop.IsEmpty()) {
			wxString tmpname = wxString::Format(wxT("LOOP %d"), _index+1);
			_name_text->SetValue(tmpname);
		} else {
			_name_text->SetValue(prop);
		}
	}

	for (int i=0; i < _chan_count; ++i) {
		wxString panstr = wxString::Format(wxT("pan_%d"), i+1);
		if (_loop_control->is_updated(_index, panstr)) {
			_loop_control->get_value(_index, panstr, val);
			_panners[i]->set_value (val);
		}
	}

	if (_use_main_in_check) {
		if (_loop_control->is_updated(_index, wxT("use_common_ins"))) {
			_loop_control->get_value(_index, wxT("use_common_ins"), val);
			_use_main_in_check->set_value (val > 0.0f);
		}
	}
// 	if (_loop_control->is_updated(_index, "use_rate")) {
// 		_loop_control->get_value(_index, "use_rate", val);
// 		_rate_button->set_active(val != 0.0f);
// 	}

	
	bool state_updated = _loop_control->is_updated(_index, wxT("state"));
	bool pos_updated = _loop_control->is_updated(_index, wxT("loop_pos"));
	bool waiting_updated = _loop_control->is_updated(_index, wxT("waiting"));

	if (_time_panel->update_time()) {
		_time_panel->Refresh(false);
	}

	if (state_updated || waiting_updated) {
		update_state();
	}

	if (pos_updated /*&& _last_state != LooperStateScratching */) {
		float looplen;
		_loop_control->get_value(_index, wxT("loop_len"), looplen);
		_loop_control->get_value(_index, wxT("loop_pos"), val);
		_scratch_control->set_indicator_value (val / looplen);
	}
}


void
LooperPanel::update_state()
{
	wxString statestr, nstatestr;
	LooperState state, nextstate;
	float val;
	float soloed = false;

	_loop_control->get_state(_index, state, statestr);
	_loop_control->get_next_state(_index, nextstate, nstatestr);
	_loop_control->get_value(_index, wxT("waiting"), val);
	_loop_control->get_value(_index, wxT("is_soloed"), soloed);
	_waiting = (val > 0.0f) ? true : false;

	if (!_waiting && _flashing_button) {
		// clear flashing
		_flashing_button->set_active(false);
		_flashing_button = 0;
	}
		
	
	//_rate_button->Enable(false);
	
	// set not active for all state buttons
	switch(_last_state) {
	case LooperStateRecording:
	case LooperStateWaitStop:
		_record_button->set_active(false);
		break;
	case LooperStateOverdubbing:
		_overdub_button->set_active(false);
		break;
	case LooperStateMultiplying:
		_multiply_button->set_active(false);
		break;
	case LooperStateReplacing:
		_replace_button->set_active(false);
		break;
	case LooperStateSubstitute:
		_substitute_button->set_active(false);
		break;
	case LooperStateDelay:
		_delay_button->set_active(false);
		break;
	case LooperStateInserting:
		_insert_button->set_active(false);
		break;
	case LooperStateScratching:
		_scratch_button->set_active(false);
		break;
	case LooperStateMuted:
	case LooperStateOffMuted:
		_mute_button->set_active(false);
		break;
	case LooperStateOneShot:
		_once_button->set_active(false);
		break;
	case LooperStatePaused:
		_pause_button->set_active(false);
		break;
	default:
		break;
	}

	
	switch(state) {
	case LooperStateRecording:
	case LooperStateWaitStop:
		_record_button->set_active(true);
		_flashing_button = _record_button;
		break;
	case LooperStateWaitStart:
		_flashing_button = _record_button;
		break;
	case LooperStateOverdubbing:
		_overdub_button->set_active(true);
		_flashing_button = _overdub_button;
		break;
	case LooperStateMultiplying:
		_multiply_button->set_active(true);
		_flashing_button = _multiply_button;
		break;
	case LooperStateReplacing:
		_replace_button->set_active(true);
		_flashing_button = _replace_button;
		break;
	case LooperStateSubstitute:
		_substitute_button->set_active(true);
		_flashing_button = _substitute_button;
		break;
	case LooperStateDelay:
		_delay_button->set_active(true);
		_flashing_button = _delay_button;
		break;
	case LooperStateInserting:
		_insert_button->set_active(true);
	        _flashing_button = _insert_button;
		break;
	case LooperStateScratching:
		_scratch_button->set_active(true);
		_scratch_control->set_style(SliderBar::CenterStyle);
		//_rate_button->Enable(true);
		break;
	case LooperStateMuted:
	case LooperStateOffMuted:
		_mute_button->set_active(true);
		_flashing_button = _mute_button;
		break;
	case LooperStateOneShot:
		_once_button->set_active(true);
		_flashing_button = _once_button;
		break;
	case LooperStatePaused:
		_pause_button->set_active(true);
		_flashing_button = _pause_button;
		break;
	default:
		break;
	}

	if (state != LooperStateScratching) {
		_scratch_control->set_style(SliderBar::HiddenStyle);
	}
	
	if (_waiting) {
		if (nextstate != LooperStateUnknown) {
			// reset flashing button to use
			switch(nextstate) {
			case LooperStateRecording:
			case LooperStateWaitStart:
			case LooperStateWaitStop:
				_flashing_button = _record_button;
				break;
			case LooperStateOverdubbing:
				_flashing_button = _overdub_button;
				break;
			case LooperStateMultiplying:
				_flashing_button = _multiply_button;
				break;
			case LooperStateReplacing:
				_flashing_button = _replace_button;
				break;
			case LooperStateSubstitute:
				_flashing_button = _substitute_button;
				break;
			case LooperStateDelay:
				_flashing_button = _delay_button;
				break;
			case LooperStateInserting:
				_flashing_button = _insert_button;
				break;
			case LooperStateOneShot:
				_flashing_button = _once_button;
				break;
			case LooperStateMuted:
			case LooperStateOffMuted:
				if (state == LooperStatePlaying)
					_flashing_button = _mute_button;
				else if ( state == LooperStateMuted)
					_flashing_button = _reverse_button;
				break;
			case LooperStatePlaying:
				//if (soloed) {
				//	_flashing_button = _solo_button;
				//}
				if (state == LooperStatePlaying)
					_flashing_button = _reverse_button;
				else if( state == LooperStateMuted) {
					_flashing_button = _mute_button;
				}
				break;
			default:
				break;
			}
			      
		}
		else if (state == LooperStatePlaying || state == LooperStateMuted) {
			// special case, we are pending reverse
			//if (soloed) {
			//	_flashing_button = _solo_button;
			//} else {
				_flashing_button = _reverse_button;
			//}
			
		}
		
		// make sure flash time is going
		if (!_flash_timer->IsRunning()) {
			_flash_timer->Start ((int)FlashRate);
		}
	}
	else {

		if (_flash_timer->IsRunning()) {
			_flash_timer->Stop();

			_loop_control->get_value(_index, wxT("rate_output"), val);
			if (val < 0.0f) {
				_reverse_button->set_active(true);
			}
			else {
				_reverse_button->set_active(false);
			}

			_solo_button->set_active(soloed);
		}

	}
	
	_last_state = state;
}

void
LooperPanel::update_rate_buttons(float val)
{
	if (val == 1.0f) {
		_1x_button->set_active(true);
		_2x_button->set_active(false);
		_halfx_button->set_active(false);
	}
	else if (val == 2.0f) {
		_1x_button->set_active(false);
		_2x_button->set_active(true);
		_halfx_button->set_active(false);
	}
	else if (val == 0.5f) {
		_1x_button->set_active(false);
		_2x_button->set_active(false);
		_halfx_button->set_active(true);
	}
	else {
		_1x_button->set_active(false);
		_2x_button->set_active(false);
		_halfx_button->set_active(false);
	}
}

void
LooperPanel::on_flash_timer (wxTimerEvent &ev)
{
	// toggle the active state of current flash button

	if (_flashing_button) {
		_flashing_button->set_active (!_flashing_button->get_active());
	}
}

void
LooperPanel::on_text_event (wxCommandEvent &ev)
{
	if (ev.GetEventType() == wxEVT_COMMAND_TEXT_ENTER) {
		cerr << "Got text event" << endl;

		// commit change
		_loop_control->post_property_change(_index, wxT("name"), _name_text->GetValue());

		_time_panel->SetFocus();
	}
}


void
LooperPanel::pressed_events (int button, wxString cmd)
{
	_loop_control->post_down_event (_index, cmd);
}

void
LooperPanel::released_events (int button, wxString cmd)
{
	
	if (button == PixButton::MiddleButton) {
		// force up
		_loop_control->post_up_event (_index, cmd, true);
	}
	else {
		_loop_control->post_up_event (_index, cmd);
	}
}


void
LooperPanel::scratch_events (wxString cmd)
{
	if (_last_state == LooperStateRecording) return;
	
	if (cmd == wxT("scratch_press") && (_last_state != LooperStateScratching)) {
		// toggle scratch on
		_scratch_pressed = true;
		_loop_control->post_down_event (_index, wxT("scratch"));
	}
	else if (_scratch_pressed) {
		// toggle scratch off
		_loop_control->post_down_event (_index, wxT("scratch"));
		_scratch_pressed = false;
	}
}

void
LooperPanel::button_bind_events (wxString cmd)
{
	MidiBindInfo info;
	char cmdbuf[100];
	
	info.channel = 0;
	info.type = "n";
	snprintf(cmdbuf, sizeof(cmdbuf), "%s", (const char *) cmd.ToAscii());
	info.control = cmdbuf;

	if (cmd == wxT("delay_trigger")) {
		info.command = "set";
	} else {
		info.command = "note"; // should this be something else?
	}
	info.instance = _index;
	info.lbound = 0.0f;
	info.ubound = 1.0f;

	start_learning (info);
}

void
LooperPanel::rate_bind_events (float val)
{
	MidiBindInfo info;
	
	info.channel = 0;
	info.type = "n";
	info.control = "rate";

	info.command = "set";
	info.instance = _index;
	info.lbound = val;
	info.ubound = val;

	start_learning (info);
}


void
LooperPanel::delay_button_press_event (int button)
{
	_tap_val *= -1.0f;
	post_control_event (wxString(wxT("delay_trigger")), _tap_val);
}

void
LooperPanel::delay_button_release_event (int button)
{
	if (button == PixButton::MiddleButton) {
		_tap_val *= -1.0f;
		post_control_event (wxString(wxT("delay_trigger")), _tap_val);
	}
}

void
LooperPanel::rate_button_event (int button, float rate)
{
// 	float val = 0.0;
// 	_loop_control->get_value(_index, "use_rate", val);

// 	val = val == 0.0f ? 1.0f : 0.0f;
// 	post_control_event (wxString("use_rate"), val);

	post_control_event (wxString(wxT("rate")), rate);
	update_rate_buttons (rate);
	_rate_control->set_value (rate);
}

void
LooperPanel::clicked_events (int button, wxString cmd)
{
	if (cmd == wxT("save"))
	{
		wxString filename = _mainpanel->do_file_selector (wxT("Choose file to save loop"),
											      wxT("wav"), wxT("WAVE files (*.wav)|*.wav;*.WAV;*.Wav"),  wxFD_SAVE|wxFD_CHANGE_DIR|wxFD_OVERWRITE_PROMPT);
		
		if ( !filename.empty() )
		{
			// add .wav if there isn't one already
			if (filename.size() <= 4 || (filename.size() > 4 && filename.substr(filename.size() - 4, 4) != wxT(".wav"))) {
				filename += wxT(".wav");
			}
			// todo: specify format
			_loop_control->post_save_loop (_index, filename);
		}
	}
	else if (cmd == wxT("load"))
	{
		wxString filename = _mainpanel->do_file_selector(wxT("Choose file to open"), wxT(""), wxT("Audio files (*.wav,*.aif)|*.wav;*.WAV;*.Wav;*.aif;*.aiff;*.AIF;*.AIFF|All files (*.*)|*.*"), wxFD_OPEN|wxFD_CHANGE_DIR);
		
		if ( !filename.empty() )
		{
			_loop_control->post_load_loop (_index, filename);
		}
	}
}


void
LooperPanel::slider_events(float val, int id)
{
	wxString ctrl;
	
	switch(id)
	{
	case ID_ThreshControl:
		ctrl = wxT("rec_thresh");
		val = val;
		break;
	case ID_FeedbackControl:
		ctrl = wxT("feedback");
		val = _feedback_control->get_value() / 100.0f;
		break;
	case ID_InputGainControl:
		ctrl = wxT("input_gain");
		val = _in_gain_control->get_value();
		break;
	case ID_DryControl:
		ctrl = wxT("dry");
		val = _dry_control->get_value();
		break;
	case ID_WetControl:
		ctrl = wxT("wet");
		val = _wet_control->get_value();
		break;
	case ID_ScratchControl:
		ctrl = wxT("scratch_pos");
		val = _scratch_control->get_value();
		break;
	case ID_RateControl:
		ctrl = wxT("rate");
		val = _rate_control->get_value();
		update_rate_buttons (val);
		break;
	case ID_StretchControl:
		ctrl = wxT("stretch_ratio");
		val = _stretch_control->get_value();
		break;
	case ID_PitchControl:
		ctrl = wxT("pitch_shift");
		val = _pitch_control->get_value();
		break;
	case ID_OutputLatency:
		ctrl = wxT("output_latency");
		cerr << "outlat " << val << endl;
		break;
	case ID_InputLatency:
		ctrl = wxT("input_latency");
		cerr << "inlat " << val << endl;
		break;
	case ID_TriggerLatency:
		ctrl = wxT("trigger_latency");
		cerr << "triglat " << val << endl;
		break;
	default:
		break;
	}

	if (!ctrl.empty()) {
		post_control_event (ctrl, val);
	}
}

void
LooperPanel::control_bind_events(int id)
{
	MidiBindInfo info;
	bool donothing = false;

	info.channel = 0;
	info.type = "cc";
	info.command = "set";
	info.instance = _index;
	info.lbound = 0.0f;
	info.ubound = 1.0f;

	
	switch(id)
	{
	case ID_ThreshControl:
		info.control = "rec_thresh";
		info.style = MidiBindInfo::GainStyle;
		break;
	case ID_FeedbackControl:
		info.control = "feedback";
		info.style = MidiBindInfo::NormalStyle;
		break;
	case ID_DryControl:
		info.control = "dry";
		info.style = MidiBindInfo::GainStyle;
		break;
	case ID_InputGainControl:
		info.control = "input_gain";
		info.style = MidiBindInfo::GainStyle;
		break;
	case ID_WetControl:
		info.control = "wet";
		info.command = "set";
		info.style = MidiBindInfo::GainStyle;
		break;
	case ID_ScratchControl:
		info.control = "scratch_pos";
		info.style = MidiBindInfo::NormalStyle;
		break;
	case ID_RateControl:
		info.control = "rate";
		info.style = MidiBindInfo::NormalStyle;
		info.lbound = 0.25;
		info.ubound = 4.0;
		break;
	case ID_StretchControl:
		info.control = "stretch_ratio";
		info.style = MidiBindInfo::NormalStyle;
		info.lbound = 0.5;
		info.ubound = 4.0;
		break;
	case ID_PitchControl:
		info.control = "pitch_shift";
		info.style = MidiBindInfo::IntegerStyle;
		info.lbound = -12;
		info.ubound = 12;
		break;
	case ID_SyncCheck:
		info.control = "sync";
		info.style = MidiBindInfo::NormalStyle;
		break;
	case ID_PlaySyncCheck:
		info.control = "playback_sync";
		info.style = MidiBindInfo::NormalStyle;
		break;
    case ID_PrefaderCheck:
    	info.control = "discrete_prefader";
    	info.style = MidiBindInfo::NormalStyle;
    	break;
	case ID_UseFeedbackPlayCheck:
		info.control = "use_feedback_play";
		info.style = MidiBindInfo::NormalStyle;
		break;
	case ID_TempoStretchCheck:
		info.control = "tempo_stretch";
		info.style = MidiBindInfo::NormalStyle;
		break;
	default:
		donothing = true;
		break;
	}

	if (!donothing) {
		start_learning(info);
	}
}

void LooperPanel::pan_events(float val, int chan)
{
	wxString ctrl = wxString::Format(wxT("pan_%d"), chan+1);

	post_control_event (ctrl, val);
}
       
void LooperPanel::pan_bind_events(int chan)
{
	MidiBindInfo info;
	char cmdbuf[20];
	
	info.channel = 0;
	info.type = "cc";
	info.command = "set";
	info.instance = _index;
	info.lbound = 0.0f;
	info.ubound = 1.0f;
	info.style = MidiBindInfo::NormalStyle;

	snprintf (cmdbuf, sizeof(cmdbuf), "pan_%d", chan+1);
	info.control = cmdbuf;

	start_learning(info);
}


void LooperPanel::start_learning(MidiBindInfo & info)
{
	if (!_learning) {
		_learning = true;

		_leftSelbar->SetBackgroundColour (_learnbgcolor);
		_rightSelbar->SetBackgroundColour (_learnbgcolor);
		_topSelbar->SetBackgroundColour (_learnbgcolor);
		_bottomSelbar->SetBackgroundColour (_learnbgcolor);
		_leftSelbar->Refresh();
		_rightSelbar->Refresh();
		_topSelbar->Refresh();
		_bottomSelbar->Refresh();
		
	}

	_loop_control->learn_midi_binding(info, true);
}


void
LooperPanel::got_binding_changed(SooperLooper::MidiBindInfo & info)
{
	if (_learning) {

		_leftSelbar->SetBackgroundColour (_bgcolor);
		_rightSelbar->SetBackgroundColour (_bgcolor);
		_topSelbar->SetBackgroundColour (_bgcolor);
		_bottomSelbar->SetBackgroundColour (_bgcolor);
		_leftSelbar->Refresh();
		_rightSelbar->Refresh();
		_topSelbar->Refresh();
		_bottomSelbar->Refresh();
		_learning = false;
	}
}

void
LooperPanel::got_learn_canceled()
{
	if (_learning) {
		_leftSelbar->SetBackgroundColour (_bgcolor);
		_rightSelbar->SetBackgroundColour (_bgcolor);
		_topSelbar->SetBackgroundColour (_bgcolor);
		_bottomSelbar->SetBackgroundColour (_bgcolor);
		_leftSelbar->Refresh();
		_rightSelbar->Refresh();
		_topSelbar->Refresh();
		_bottomSelbar->Refresh();
		_learning = false;
	}
}


void
LooperPanel::check_events(bool val, wxString which)
{
	post_control_event (which, val ? 1.0f: 0.0f);
}

void LooperPanel::on_quantize_change (int index, wxString strval)
{
	// 0 is none, 1 is cycle, 2 is eighth, 3 is loop

	post_control_event (wxT("quantize"), (float) index);

}


void
LooperPanel::post_control_event (wxString ctrl, float val)
{
	_loop_control->post_ctrl_change (_index, ctrl, val);
}

