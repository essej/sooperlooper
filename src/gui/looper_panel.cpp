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

#include "gui_app.hpp"
#include "gui_frame.hpp"
#include "keyboard_target.hpp"
#include "looper_panel.hpp"
#include "pix_button.hpp"
#include "loop_control.hpp"
#include "time_panel.hpp"
#include "slider_bar.hpp"
#include "choice_box.hpp"
#include "check_box.hpp"

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

	ID_QuantizeCheck,
	ID_QuantizeChoice,
	ID_RoundCheck,
	ID_SyncCheck,
	ID_UseFeedbackPlayCheck,
	ID_PlaySyncCheck

};

BEGIN_EVENT_TABLE(LooperPanel, wxPanel)

	
END_EVENT_TABLE()

LooperPanel::LooperPanel(LoopControl * control, wxWindow * parent, wxWindowID id, const wxPoint& pos, const wxSize& size)
	: wxPanel(parent, id, pos, size), _loop_control(control), _index(0), _last_state(LooperStateUnknown), _tap_val(1.0f)
{
	_learning = false;
	_scratch_pressed = false;
	_last_state = LooperStateUnknown;

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

	wxBoxSizer * colsizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer * rowsizer;

	// add selbar
	_bgcolor.Set(0,0,0);
	_selbgcolor.Set(244, 255, 158);
	_learnbgcolor.Set(134, 80, 158);

	_selbar = new wxPanel(this, -1, wxDefaultPosition, wxSize(4,-1));
	_selbar->SetThemeEnabled(false);
	_selbar->SetBackgroundColour (_bgcolor);
	
	mainSizer->Add (_selbar, 0, wxEXPAND|wxBOTTOM|wxLEFT, 0);

	// create all buttons first, then add them to sizers
	// must do this because the bitmaps need to be loaded
	// before adding to sizer
	create_buttons();
	
	
 	colsizer->Add (_undo_button, 0, wxTOP, 5);

 	colsizer->Add (_redo_button, 0, wxTOP, 5);
	
	mainSizer->Add (colsizer, 0, wxEXPAND|wxBOTTOM|wxLEFT, 5);

	
	colsizer = new wxBoxSizer(wxVERTICAL);

	
 	colsizer->Add (_record_button, 0, wxTOP|wxLEFT, 5);

 	colsizer->Add (_overdub_button, 0, wxTOP|wxLEFT, 5);

 	colsizer->Add (_multiply_button, 0, wxTOP|wxLEFT, 5);
	
	mainSizer->Add (colsizer, 0, wxEXPAND|wxBOTTOM, 5);


	colsizer = new wxBoxSizer(wxVERTICAL);
	rowsizer = new wxBoxSizer(wxHORIZONTAL);

	SliderBar *slider;
	wxFont sliderFont = *wxSMALL_FONT;
	
	_thresh_control = slider = new SliderBar(this, ID_ThreshControl, 0.0f, 1.0f, 0.0f);
	slider->set_units(wxT("dB"));
	slider->set_label(wxT("rec thresh"));
	slider->set_scale_mode(SliderBar::ZeroGainMode);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (bind (slot (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (bind (slot (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	colsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 5);

	_feedback_control = slider = new SliderBar(this, ID_FeedbackControl, 0.0f, 100.0f, 100.0f);
	slider->set_units(wxT("%"));
	slider->set_label(wxT("feedback"));
	slider->SetFont(sliderFont);
	slider->value_changed.connect (bind (slot (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (bind (slot (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	colsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 5);

	//colsizer->Add (20, 5, 0, wxEXPAND);
	
 	rowsizer->Add (_replace_button, 0, wxTOP|wxLEFT, 5);

 	rowsizer->Add (_insert_button, 0, wxTOP|wxLEFT, 5);

	colsizer->Add (rowsizer, 0);

	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	
 	rowsizer->Add (_substitute_button, 0, wxTOP|wxLEFT, 5);

	//_reverse_button->Show(false);
 	rowsizer->Add (_delay_button, 0, wxTOP|wxLEFT, 5);

	colsizer->Add (rowsizer, 0);
	
	mainSizer->Add (colsizer, 0, wxEXPAND|wxBOTTOM, 5);
	

	// time area
	colsizer = new wxBoxSizer(wxVERTICAL);

	_time_panel = new TimePanel(_loop_control, this, -1);
	_time_panel->set_index (_index);
	
	colsizer->Add (_time_panel, 0, wxLEFT|wxTOP, 5);

	//colsizer->Add (20, -1, 1);

	_dry_control = slider = new SliderBar(this, ID_DryControl, 0.0f, 1.0f, 1.0f);
	slider->set_units(wxT("dB"));
	slider->set_label(wxT("dry"));
	slider->set_scale_mode(SliderBar::ZeroGainMode);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (bind (slot (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (bind (slot (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	colsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 4);

	_wet_control = slider = new SliderBar(this, ID_WetControl, 0.0f, 1.0f, 1.0f);
	slider->set_units(wxT("dB"));
	slider->set_label(wxT("wet"));
	slider->set_scale_mode(SliderBar::ZeroGainMode);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (bind (slot (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (bind (slot (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	colsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 4);
	
	
	mainSizer->Add (colsizer, 0, wxEXPAND|wxBOTTOM, 5);


	//

	colsizer = new wxBoxSizer(wxVERTICAL);
	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer * lilcolsizer = new wxBoxSizer(wxVERTICAL);
	
// 	_quantize_choice = new ChoiceBox (this, ID_QuantizeChoice, wxDefaultPosition, wxSize (110, 22));
// 	_quantize_choice->set_label (wxT("quantize"));
// 	_quantize_choice->SetFont (sliderFont);
// 	_quantize_choice->value_changed.connect (slot (*this,  &LooperPanel::on_quantize_change));
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
	_sync_check->value_changed.connect (bind (slot (*this, &LooperPanel::check_events), wxT("sync")));
	_sync_check->bind_request.connect (bind (slot (*this, &LooperPanel::control_bind_events), (int) _sync_check->GetId()));
	lilrowsizer->Add (_sync_check, 1, wxLEFT, 3);
	lilcolsizer->Add (lilrowsizer, 0, wxTOP|wxEXPAND, 0);

	lilrowsizer = new wxBoxSizer(wxHORIZONTAL);
	_play_sync_check = new CheckBox(this, ID_PlaySyncCheck, wxT("play sync"), true, wxDefaultPosition, wxSize(55, 18));
	_play_sync_check->SetFont(sliderFont);
	_play_sync_check->SetToolTip(wxT("sync playback auto-triggering to quantized sync source"));
	_play_sync_check->value_changed.connect (bind (slot (*this, &LooperPanel::check_events), wxT("playback_sync")));
	_play_sync_check->bind_request.connect (bind (slot (*this, &LooperPanel::control_bind_events), (int) _play_sync_check->GetId()));
	lilrowsizer->Add (_play_sync_check, 1, wxLEFT, 3);
	lilcolsizer->Add (lilrowsizer, 0, wxTOP|wxEXPAND, 0);
	
	lilrowsizer = new wxBoxSizer(wxHORIZONTAL);
	_play_feed_check = new CheckBox(this, ID_UseFeedbackPlayCheck, wxT("p. feedb"), true, wxDefaultPosition, wxSize(55, 18));
	_play_feed_check->SetFont(sliderFont);
	_play_feed_check->SetToolTip(wxT("enable feedback during playback"));
	_play_feed_check->value_changed.connect (bind (slot (*this, &LooperPanel::check_events), wxT("use_feedback_play")));
	_play_feed_check->bind_request.connect (bind (slot (*this, &LooperPanel::control_bind_events), (int) _play_feed_check->GetId()));
	lilrowsizer->Add (_play_feed_check, 1, wxLEFT, 3);
	lilcolsizer->Add (lilrowsizer, 0, wxTOP|wxEXPAND, 0);
	
	rowsizer->Add(lilcolsizer, 1, wxTOP|wxLEFT, 3);


	lilcolsizer = new wxBoxSizer(wxVERTICAL);
	
	lilcolsizer->Add (_load_button, 0, wxTOP, 2);

	lilcolsizer->Add (_save_button, 0, wxTOP, 2);
	
	rowsizer->Add(lilcolsizer, 0, wxTOP|wxLEFT, 3);

	lilcolsizer = new wxBoxSizer(wxVERTICAL);
	
	lilcolsizer->Add (_trig_button, 0, wxTOP, 2);

	lilcolsizer->Add (_once_button, 0, wxTOP, 2);
	
	rowsizer->Add(lilcolsizer, 0, wxTOP|wxLEFT, 3);
	
	
 	rowsizer->Add (_mute_button, 0, wxTOP|wxLEFT, 5);
	
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
	slider->set_style (SliderBar::CenterStyle);
	slider->set_decimal_digits (3);
	slider->set_show_value(false);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (bind (slot (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (bind (slot (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	rowsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 3);
	
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
	slider->value_changed.connect (bind (slot (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	slider->bind_request.connect (bind (slot (*this, &LooperPanel::control_bind_events), (int) slider->GetId()));
	rowsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 3);
	
	colsizer->Add (rowsizer, 0, wxEXPAND|wxLEFT, 1);

	
	mainSizer->Add (colsizer, 1, wxEXPAND|wxBOTTOM|wxRIGHT, 5);


	// add an index static text fixed position

// 	_index_text = new wxStaticText(this, -1, wxString::Format(wxT("%d"), _index+1), wxPoint(4,4), wxDefaultSize);
// 	_index_text->SetForegroundColour(wxColour(14, 50, 89));
// 	wxFont textfont(12, wxSWISS, wxNORMAL, wxBOLD);
// 	_index_text->SetFont(textfont);
// 	_index_text->Raise();
	
	bind_events();
	
	
	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( mainSizer );      // actually set the sizer
	mainSizer->Fit( this );            // set size to minimum size as calculated by the sizer
	mainSizer->SetSizeHints( this );   // set size hints to honour mininum size

}

void
LooperPanel::set_selected (bool flag)
{
	if (flag) {
		_selbar->SetBackgroundColour (_selbgcolor);
	}
	else {
		_selbar->SetBackgroundColour (_bgcolor);
	}

	_selbar->Refresh();
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
	_undo_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("undo")));
	_undo_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("undo")));
	_undo_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("undo")));

	_redo_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("redo")));
	_redo_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("redo")));
	_redo_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("redo")));

	_record_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("record")));
	_record_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("record")));
	_record_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("record")));

	_overdub_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("overdub")));
	_overdub_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("overdub")));
	_overdub_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("overdub")));

	_multiply_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("multiply")));
	_multiply_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("multiply")));
	_multiply_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("multiply")));

	_replace_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("replace")));
	_replace_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("replace")));
	_replace_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("replace")));

	_insert_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("insert")));
	_insert_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("insert")));
	_insert_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("insert")));

	_once_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("oneshot")));
	_once_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("oneshot")));
	_once_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("oneshot")));

	_trig_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("trigger")));
	_trig_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("trigger")));
	_trig_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("trigger")));

	_delay_button->pressed.connect (slot (*this, &LooperPanel::delay_button_press_event));
	_delay_button->released.connect (slot (*this, &LooperPanel::delay_button_release_event));
	_delay_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("delay_trigger")));

	_reverse_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("reverse")));
	_reverse_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("reverse")));
	_reverse_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("reverse")));

	_substitute_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("substitute")));
	_substitute_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("substitute")));
	_substitute_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("substitute")));
	
	_mute_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("mute")));
	_mute_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("mute")));
	_mute_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("mute")));

	_halfx_button->pressed.connect (bind (slot (*this, &LooperPanel::rate_button_event), 0.5f));
	_1x_button->pressed.connect (bind (slot (*this, &LooperPanel::rate_button_event), 1.0f));
	_2x_button->pressed.connect (bind (slot (*this, &LooperPanel::rate_button_event), 2.0f));

	_scratch_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("scratch")));
	_scratch_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("scratch")));
	_scratch_button->bind_request.connect (bind (slot (*this, &LooperPanel::button_bind_events), wxString("scratch")));

	_save_button->clicked.connect (bind (slot (*this, &LooperPanel::clicked_events), wxString("save")));
	_load_button->clicked.connect (bind (slot (*this, &LooperPanel::clicked_events), wxString("load")));


	_scratch_control->pressed.connect (bind (slot (*this, &LooperPanel::scratch_events), wxString("scratch_press")));
	_scratch_control->released.connect (bind (slot (*this, &LooperPanel::scratch_events), wxString("scratch_release")));

	
	_loop_control->MidiBindingChanged.connect (slot (*this, &LooperPanel::got_binding_changed));
	_loop_control->MidiLearnCancelled.connect (slot (*this, &LooperPanel::got_learn_canceled));
	
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
	_reverse_button->SetToolTip(wxT("reverses direction"));
 	_substitute_button = new PixButton(this, ID_SubstituteButton);
 	_load_button = new PixButton(this, ID_LoadButton, false);
 	_save_button = new PixButton(this, ID_SaveButton, false);
 	_trig_button = new PixButton(this, ID_TrigButton);
 	_once_button = new PixButton(this, ID_OnceButton);
 	_mute_button = new PixButton(this, ID_MuteButton);
 	_scratch_button = new PixButton(this, ID_ScratchButton);
 	_halfx_button = new PixButton(this, ID_HalfXButton, false);
 	_1x_button = new PixButton(this, ID_OneXButton, false);
	_2x_button = new PixButton(this, ID_DoubleXButton, false);

	
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


wxString
LooperPanel::get_pixmap_path (const wxString & namebase)
{
	wxString filename;
	wxString pixmapdir("pixmaps/");
	
#ifdef PIXMAPDIR
	pixmapdir = PIXMAPDIR;
#endif
	
	if (wxFile::Exists(wxString::Format("%s%s", pixmapdir.c_str(), namebase.c_str()))) {
		filename = wxString::Format("%s%s", pixmapdir.c_str(), namebase.c_str());
	}
	else if (wxFile::Exists(wxString::Format("pixmaps%c%s", wxFileName::GetPathSeparator(), namebase.c_str()))) {
		filename = wxString::Format("pixmaps%c%s", wxFileName::GetPathSeparator(), namebase.c_str());
	}
	else if (wxFile::Exists (namebase)) {
		filename = namebase;
	}
	
	return filename;
}

bool
LooperPanel::load_bitmaps (PixButton * butt, wxString namebase)
{
	wxString bpath;

	if(!(bpath = get_pixmap_path(namebase + wxT("_normal.png"))).empty()) {
		butt->set_normal_bitmap (wxBitmap(bpath, wxBITMAP_TYPE_PNG));
	}
	
	if(!(bpath = get_pixmap_path(namebase + wxT("_selected.png"))).empty()) {
		butt->set_selected_bitmap (wxBitmap(bpath, wxBITMAP_TYPE_PNG));
	}

	if(!(bpath = get_pixmap_path(namebase + wxT("_focus.png"))).empty()) {
		butt->set_focus_bitmap (wxBitmap(bpath, wxBITMAP_TYPE_PNG));
	}

	if(!(bpath = get_pixmap_path(namebase + wxT("_disabled.png"))).empty()) {
		butt->set_disabled_bitmap (wxBitmap(bpath, wxBITMAP_TYPE_PNG));
	}

	if(!(bpath = get_pixmap_path(namebase + wxT("_active.png"))).empty()) {
		butt->set_active_bitmap (wxBitmap(bpath, wxBITMAP_TYPE_PNG));
	}

	return true;
}


void
LooperPanel::update_controls()
{
	// get recent controls from loop control
	float val;
	
	if (_loop_control->is_updated(_index, "feedback")) {
		_loop_control->get_value(_index, "feedback", val);
		_feedback_control->set_value ((val * 100.0f));
	}
	if (_loop_control->is_updated(_index, "rec_thresh")) {
		_loop_control->get_value(_index, "rec_thresh", val);
		_thresh_control->set_value (val);
	}
	if (_loop_control->is_updated(_index, "dry")) {
		_loop_control->get_value(_index, "dry", val);
		_dry_control->set_value (val);
	}
	if (_loop_control->is_updated(_index, "wet")) {
		_loop_control->get_value(_index, "wet", val);
		_wet_control->set_value (val);
	}
	if (_loop_control->is_updated(_index, "rate")) {
		_loop_control->get_value(_index, "rate", val);
		_rate_control->set_value (val);

		update_rate_buttons(val);
	}
	if (_loop_control->is_updated(_index, "rate_output")) {
		_loop_control->get_value(_index, "rate_output", val);
		if (val < 0.0) {
			_reverse_button->set_active(true);
		}
		else {
			_reverse_button->set_active(false);
		}
	}
	if (_loop_control->is_updated(_index, "scratch_pos")) {
		_loop_control->get_value(_index, "scratch_pos", val);
		_scratch_control->set_value (val);
	}
// 	if (_loop_control->is_updated(_index, "quantize")) {
// 		_loop_control->get_value(_index, "quantize", val);
// 		//_quantize_choice->set_index_value ((int)val);
// 		_quantize_check->SetValue (val > 0.0);
// 	}
// 	if (_loop_control->is_updated(_index, "round")) {
// 		_loop_control->get_value(_index, "round", val);
// 		_round_check->SetValue (val > 0.0);
// 	}
	if (_loop_control->is_updated(_index, "sync")) {
		_loop_control->get_value(_index, "sync", val);
		_sync_check->set_value (val > 0.0);
	}
	if (_loop_control->is_updated(_index, "playback_sync")) {
		_loop_control->get_value(_index, "playback_sync", val);
		_play_sync_check->set_value (val > 0.0);
	}
	if (_loop_control->is_updated(_index, "use_feedback_play")) {
		_loop_control->get_value(_index, "use_feedback_play", val);
		_play_feed_check->set_value (val > 0.0);
	}
// 	if (_loop_control->is_updated(_index, "use_rate")) {
// 		_loop_control->get_value(_index, "use_rate", val);
// 		_rate_button->set_active(val != 0.0f);
// 	}

	bool state_updated = _loop_control->is_updated(_index, "state");
	bool pos_updated = _loop_control->is_updated(_index, "loop_pos");
	
	if (_time_panel->update_time()) {
		_time_panel->Refresh(false);
	}

	if (state_updated) {
		update_state();
	}

	if (pos_updated && _last_state != LooperStateScratching) {
		float looplen;
		_loop_control->get_value(_index, "loop_len", looplen);
		_loop_control->get_value(_index, "loop_pos", val);
		_scratch_control->set_value (val / looplen);
	}
}


void
LooperPanel::update_state()
{
	wxString statestr;
	LooperState state;
	
	_loop_control->get_state(_index, state, statestr);

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
		_mute_button->set_active(false);
		break;
	default:
		break;
	}

	
	switch(state) {
	case LooperStateRecording:
	case LooperStateWaitStop:
		_record_button->set_active(true);
		break;
	case LooperStateOverdubbing:
		_overdub_button->set_active(true);
		break;
	case LooperStateMultiplying:
		_multiply_button->set_active(true);
		break;
	case LooperStateReplacing:
		_replace_button->set_active(true);
		break;
	case LooperStateSubstitute:
		_substitute_button->set_active(true);
		break;
	case LooperStateDelay:
		_delay_button->set_active(true);
		break;
	case LooperStateInserting:
		_insert_button->set_active(true);
		break;
	case LooperStateScratching:
		_scratch_button->set_active(true);
		//_rate_button->Enable(true);
		break;
	case LooperStateMuted:
		_mute_button->set_active(true);
		break;
	default:
		break;
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

	info.channel = 0;
	info.type = "n";
	info.control = cmd.c_str();
	if (cmd == wxT("delay_trigger")) {
		info.command = "set";
	} else {
		info.command = "note"; // should this be something else?
	}
	info.instance = _index;
	info.lbound = 0.0;
	info.ubound = 1.0;

	start_learning (info);
}


void
LooperPanel::delay_button_press_event (int button)
{
	_tap_val *= -1.0f;
	post_control_event (wxString("delay_trigger"), _tap_val);
}

void
LooperPanel::delay_button_release_event (int button)
{
	if (button == PixButton::MiddleButton) {
		_tap_val *= -1.0f;
		post_control_event (wxString("delay_trigger"), _tap_val);
	}
}

void
LooperPanel::rate_button_event (int button, float rate)
{
// 	float val = 0.0;
// 	_loop_control->get_value(_index, "use_rate", val);

// 	val = val == 0.0f ? 1.0f : 0.0f;
// 	post_control_event (wxString("use_rate"), val);

	post_control_event (wxString("rate"), rate);
}

void
LooperPanel::clicked_events (int button, wxString cmd)
{
	if (cmd == wxT("save"))
	{
		// popup local file dialog if we are local
		if (_loop_control->is_engine_local()) {

			::wxGetApp().getFrame()->get_keyboard().set_enabled(false);

			wxString filename = ::wxFileSelector(wxT("Choose file to save loop"), wxT(""), wxT(""), wxT(".wav"), wxT("*.*"), wxSAVE|wxCHANGE_DIR);
			if ( !filename.empty() )
			{
				// todo: specify format
				_loop_control->post_save_loop (_index, filename);
			}

			::wxGetApp().getFrame()->get_keyboard().set_enabled(true);
			
		}
		else {
			// popup basic filename text entry
			::wxGetApp().getFrame()->get_keyboard().set_enabled(false);

			wxString filename = ::wxGetTextFromUser(wxString::Format("Choose file to save on remote host '%s'",
										 _loop_control->get_engine_host().c_str())
								, wxT("Save Loop"));

			if (!filename.empty()) {
				// todo: specify format
				_loop_control->post_save_loop (_index, filename);
			}

			::wxGetApp().getFrame()->get_keyboard().set_enabled(true);
		}

		
		
	}
	else if (cmd == wxT("load"))
	{
		if (_loop_control->is_engine_local()) {
			
			::wxGetApp().getFrame()->get_keyboard().set_enabled(false);
			wxString filename = wxFileSelector(wxT("Choose file to open"), wxT(""), wxT(""), wxT(""), wxT("*.*"), wxOPEN|wxCHANGE_DIR);
			::wxGetApp().getFrame()->get_keyboard().set_enabled(true);

			if ( !filename.empty() )
			{
				_loop_control->post_load_loop (_index, filename);
			}
		}
		else {
			// popup basic filename text entry
			::wxGetApp().getFrame()->get_keyboard().set_enabled(false);

			wxString filename = ::wxGetTextFromUser(wxString::Format("Choose file to load on remote host '%s'",
										 _loop_control->get_engine_host().c_str())
								, wxT("Open Loop"));

			::wxGetApp().getFrame()->get_keyboard().set_enabled(true);

			if (!filename.empty()) {
				// todo: specify format
				_loop_control->post_load_loop (_index, filename);
			}

			
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
	wxString ctrl;
	MidiBindInfo info;
	bool donothing = false;

	info.channel = 0;
	info.type = "cc";
	info.command = "set";
	info.instance = _index;
	info.lbound = 0.0;
	info.ubound = 1.0;

	
	switch(id)
	{
	case ID_ThreshControl:
		ctrl = wxT("rec_thresh");
		info.control = ctrl.c_str();
		info.style = MidiBindInfo::GainStyle;
		break;
	case ID_FeedbackControl:
		ctrl = wxT("feedback");
		info.control = ctrl.c_str();
		info.style = MidiBindInfo::NormalStyle;
		break;
	case ID_DryControl:
		ctrl = wxT("dry");
		info.control = ctrl.c_str();
		info.style = MidiBindInfo::GainStyle;
		break;
	case ID_WetControl:
		ctrl = wxT("wet");
		info.control = ctrl.c_str();
		info.command = "set";
		info.style = MidiBindInfo::GainStyle;
		break;
	case ID_ScratchControl:
		ctrl = wxT("scratch_pos");
		info.control = ctrl.c_str();
		info.style = MidiBindInfo::NormalStyle;
		break;
	case ID_RateControl:
		ctrl = wxT("rate");
		info.control = ctrl.c_str();
		info.style = MidiBindInfo::NormalStyle;
		info.lbound = 0.25;
		info.ubound = 4.0;
		break;
	case ID_SyncCheck:
		ctrl = wxT("sync");
		info.control = ctrl.c_str();
		info.style = MidiBindInfo::NormalStyle;
		break;
	case ID_PlaySyncCheck:
		ctrl = wxT("playback_sync");
		info.control = ctrl.c_str();
		info.style = MidiBindInfo::NormalStyle;
		break;
	case ID_UseFeedbackPlayCheck:
		ctrl = wxT("use_feedback_play");
		info.control = ctrl.c_str();
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

void LooperPanel::start_learning(MidiBindInfo & info)
{
	if (!_learning) {
		_learning = true;

		_selbar->SetBackgroundColour (_learnbgcolor);
		_selbar->Refresh();
		
	}

	_loop_control->learn_midi_binding(info, true);
}


void
LooperPanel::got_binding_changed(SooperLooper::MidiBindInfo & info)
{
	if (_learning) {

		_selbar->SetBackgroundColour (_bgcolor);
		_selbar->Refresh();
		_learning = false;
	}
}

void
LooperPanel::got_learn_canceled()
{
	if (_learning) {
		_selbar->SetBackgroundColour (_bgcolor);
		_selbar->Refresh();
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

