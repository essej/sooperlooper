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

#include "looper_panel.hpp"
#include "pix_button.hpp"
#include "loop_control.hpp"
#include "time_panel.hpp"
#include "slider_bar.hpp"
#include "choice_box.hpp"

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
	ID_MuteButton,
	ID_RateButton,
	ID_ScratchButton,
	ID_LoadButton,
	ID_SaveButton,

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


};

BEGIN_EVENT_TABLE(LooperPanel, wxPanel)

	EVT_CHECKBOX (ID_QuantizeCheck, LooperPanel::check_events)
	EVT_CHECKBOX (ID_RoundCheck, LooperPanel::check_events)
	EVT_CHECKBOX (ID_SyncCheck, LooperPanel::check_events)
	
END_EVENT_TABLE()

LooperPanel::LooperPanel(LoopControl * control, wxWindow * parent, wxWindowID id, const wxPoint& pos, const wxSize& size)
	: wxPanel(parent, id, pos, size), _loop_control(control), _index(0), _last_state(LooperStateUnknown), _tap_val(1.0f)
{
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
	PixButton * bitbutt;

 	_undo_button = bitbutt = new PixButton(this, ID_UndoButton);
	load_bitmaps (bitbutt, wxT("undo"));
 	colsizer->Add (bitbutt, 0, wxTOP, 5);

 	_redo_button = bitbutt = new PixButton(this, ID_RedoButton);
	load_bitmaps (bitbutt, wxT("redo"));
 	colsizer->Add (bitbutt, 0, wxTOP, 5);
	
	mainSizer->Add (colsizer, 0, wxEXPAND|wxBOTTOM|wxLEFT, 5);

	
	colsizer = new wxBoxSizer(wxVERTICAL);

 	_record_button = bitbutt = new PixButton(this, ID_RecordButton);
	load_bitmaps (bitbutt, wxT("record"));
	
 	colsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);

 	_overdub_button = bitbutt = new PixButton(this, ID_OverdubButton);
	load_bitmaps (bitbutt, wxT("overdub"));
 	colsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);

 	_multiply_button = bitbutt = new PixButton(this, ID_MultiplyButton);
	load_bitmaps (bitbutt, wxT("multiply"));
 	colsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);
	
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
	colsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 5);

	_feedback_control = slider = new SliderBar(this, ID_FeedbackControl, 0.0f, 100.0f, 0.0f);
	slider->set_units(wxT("%"));
	slider->set_label(wxT("feedback"));
	slider->SetFont(sliderFont);
	slider->value_changed.connect (bind (slot (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	colsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 5);

	//colsizer->Add (20, 5, 0, wxEXPAND);
	
 	_replace_button = bitbutt = new PixButton(this, ID_ReplaceButton);
	load_bitmaps (bitbutt, wxT("replace"));
 	rowsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);

 	_tap_button = bitbutt = new PixButton(this, ID_TapButton);
	load_bitmaps (bitbutt, wxT("tap"));
 	rowsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);

	colsizer->Add (rowsizer, 0);

	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	
 	_insert_button = bitbutt = new PixButton(this, ID_InsertButton);
	load_bitmaps (bitbutt, wxT("insert"));
 	rowsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);

 	_reverse_button = bitbutt = new PixButton(this, ID_ReverseButton);
	load_bitmaps (bitbutt, wxT("reverse"));
 	rowsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);

	colsizer->Add (rowsizer, 0);
	
	mainSizer->Add (colsizer, 0, wxEXPAND|wxBOTTOM, 5);
	

	// time area
	colsizer = new wxBoxSizer(wxVERTICAL);

	_time_panel = new TimePanel(_loop_control, this, -1, wxDefaultPosition, wxSize(210, 60));
	_time_panel->set_index (_index);
	
	colsizer->Add (_time_panel, 0, wxLEFT|wxTOP, 5);

	colsizer->Add (20, -1, 1);

	_dry_control = slider = new SliderBar(this, ID_DryControl, 0.0f, 1.0f, 1.0f);
	slider->set_units(wxT("dB"));
	slider->set_label(wxT("dry"));
	slider->set_scale_mode(SliderBar::ZeroGainMode);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (bind (slot (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	colsizer->Add (slider, 0, wxEXPAND|wxTOP|wxLEFT, 4);

	_wet_control = slider = new SliderBar(this, ID_WetControl, 0.0f, 1.0f, 1.0f);
	slider->set_units(wxT("dB"));
	slider->set_label(wxT("wet"));
	slider->set_scale_mode(SliderBar::ZeroGainMode);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (bind (slot (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	colsizer->Add (slider, 0, wxEXPAND|wxTOP|wxLEFT, 4);
	
	
	mainSizer->Add (colsizer, 0, wxEXPAND|wxBOTTOM, 5);


	//

	colsizer = new wxBoxSizer(wxVERTICAL);
	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer * lilcolsizer = new wxBoxSizer(wxVERTICAL);
	
	_quantize_choice = new ChoiceBox (this, ID_QuantizeChoice, wxDefaultPosition, wxSize (110, 22));
	_quantize_choice->set_label (wxT("quantize"));
	_quantize_choice->SetFont (sliderFont);
	_quantize_choice->value_changed.connect (slot (*this,  &LooperPanel::on_quantize_change));
	_quantize_choice->append_choice (wxT("off"));
	_quantize_choice->append_choice (wxT("cycle"));
	_quantize_choice->append_choice (wxT("8th"));
	_quantize_choice->append_choice (wxT("loop"));

	lilcolsizer->Add (_quantize_choice, 0);
	
	wxBoxSizer * lilrowsizer = new wxBoxSizer(wxHORIZONTAL);
	
	_round_check = new wxCheckBox(this, ID_RoundCheck, "round");
	_round_check->SetFont(sliderFont);
	_round_check->SetBackgroundColour(wxColour(90,90,90));
	_round_check->SetForegroundColour(*wxWHITE);
	lilrowsizer->Add (_round_check, 0, wxEXPAND);

	_sync_check = new wxCheckBox(this, ID_SyncCheck, "sync");
	_sync_check->SetFont(sliderFont);
	_sync_check->SetBackgroundColour(wxColour(90,90,90));
	_sync_check->SetForegroundColour(*wxWHITE);
	lilrowsizer->Add (_sync_check, 0, wxEXPAND|wxLEFT, 3);

	lilcolsizer->Add (lilrowsizer, 0, wxTOP, 2);
	
	rowsizer->Add(lilcolsizer, 1, wxTOP|wxLEFT, 5);


	lilcolsizer = new wxBoxSizer(wxVERTICAL);
	
 	_load_button = bitbutt = new PixButton(this, ID_LoadButton);
	load_bitmaps (bitbutt, wxT("load"));
	lilcolsizer->Add (bitbutt, 0, wxTOP, 3);

 	_save_button = bitbutt = new PixButton(this, ID_SaveButton);
	load_bitmaps (bitbutt, wxT("save"));
	lilcolsizer->Add (bitbutt, 0, wxTOP, 3);
	
	rowsizer->Add(lilcolsizer, 0, wxTOP|wxLEFT, 2);
	
	
 	_mute_button = bitbutt = new PixButton(this, ID_MuteButton);
	load_bitmaps (bitbutt, wxT("mute"));
 	rowsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 5);
	
	colsizer->Add (rowsizer, 0, wxEXPAND);

	colsizer->Add (20,-1, 1);
	
	// rate stuff
	rowsizer = new wxBoxSizer(wxHORIZONTAL);
 	_rate_button = bitbutt = new PixButton(this, ID_RateButton);
	load_bitmaps (bitbutt, wxT("rate"));
 	rowsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 3);

	// rate control
	_rate_control = slider = new SliderBar(this, ID_RateControl, -4.0f, 4.0f, 1.0f);
	slider->set_units(wxT(""));
	slider->set_label(wxT(""));
	slider->set_style (SliderBar::CenterStyle);
	slider->set_decimal_digits (3);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (bind (slot (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	rowsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 3);
	
	colsizer->Add (rowsizer, 0, wxEXPAND);

	// scratch stuff
	rowsizer = new wxBoxSizer(wxHORIZONTAL);
 	_scratch_button = bitbutt = new PixButton(this, ID_ScratchButton);
	load_bitmaps (bitbutt, wxT("scratch"));
 	rowsizer->Add (bitbutt, 0, wxTOP|wxLEFT, 3);

	// scratch control
	_scratch_control = slider = new SliderBar(this, ID_ScratchControl, 0.0f, 1.0f, 0.0f);
	slider->set_units(wxT(""));
	slider->set_label(wxT("pos"));
	slider->set_style (SliderBar::CenterStyle);
	slider->set_decimal_digits (3);
	slider->SetFont(sliderFont);
	slider->value_changed.connect (bind (slot (*this, &LooperPanel::slider_events), (int) slider->GetId()));
	rowsizer->Add (slider, 1, wxEXPAND|wxTOP|wxLEFT, 3);
	
	colsizer->Add (rowsizer, 0, wxEXPAND);
	
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

	_redo_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("redo")));
	_redo_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("redo")));

	_record_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("record")));
	_record_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("record")));

	_overdub_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("overdub")));
	_overdub_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("overdub")));

	_multiply_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("multiply")));
	_multiply_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("multiply")));

	_replace_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("replace")));
	_replace_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("replace")));

	_insert_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("insert")));
	_insert_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("insert")));

	_tap_button->pressed.connect (slot (*this, &LooperPanel::tap_button_event));

	_reverse_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("reverse")));
	_reverse_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("reverse")));

	_mute_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("mute")));
	_mute_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("mute")));

	_rate_button->pressed.connect (slot (*this, &LooperPanel::rate_button_event));

	_scratch_button->pressed.connect (bind (slot (*this, &LooperPanel::pressed_events), wxString("scratch")));
	_scratch_button->released.connect (bind (slot (*this, &LooperPanel::released_events), wxString("scratch")));

	_save_button->clicked.connect (bind (slot (*this, &LooperPanel::clicked_events), wxString("save")));
	_load_button->clicked.connect (bind (slot (*this, &LooperPanel::clicked_events), wxString("load")));
	
}

wxString
LooperPanel::get_pixmap_path (const wxString & namebase)
{
	wxString filename;
	
	if (wxFile::Exists(wxString::Format("%s%s", PIXMAPDIR, namebase.c_str()))) {
		filename = wxString::Format("%s%s", PIXMAPDIR, namebase.c_str());
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
	}
	if (_loop_control->is_updated(_index, "scratch_pos")) {
		_loop_control->get_value(_index, "scratch_pos", val);
		_scratch_control->set_value (val);
	}
	if (_loop_control->is_updated(_index, "quantize")) {
		_loop_control->get_value(_index, "quantize", val);
		_quantize_choice->set_index_value ((int)val);
	}
	if (_loop_control->is_updated(_index, "round")) {
		_loop_control->get_value(_index, "round", val);
		_round_check->SetValue (val > 0.0);
	}
	if (_loop_control->is_updated(_index, "sync")) {
		_loop_control->get_value(_index, "sync", val);
		_sync_check->SetValue (val > 0.0);
	}
	if (_loop_control->is_updated(_index, "use_rate")) {
		_loop_control->get_value(_index, "use_rate", val);
		_rate_button->set_active(val != 0.0f);
	}

	bool state_updated = _loop_control->is_updated(_index, "state");
	
	if (_time_panel->update_time()) {
		_time_panel->Refresh(false);
	}

	if (state_updated) {
		update_state();
	}
}


void
LooperPanel::update_state()
{
	wxString statestr;
	LooperState state;
	
	_loop_control->get_state(_index, state, statestr);

	_rate_button->Enable(false);
	
	// set not active for all state buttons
	switch(_last_state) {
	case LooperStateRecording:
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
	case LooperStateDelay:
		_tap_button->set_active(false);
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
	case LooperStateDelay:
		_tap_button->set_active(true);
		break;
	case LooperStateInserting:
		_insert_button->set_active(true);
		break;
	case LooperStateScratching:
		_scratch_button->set_active(true);
		_rate_button->Enable(true);
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
LooperPanel::pressed_events (wxString cmd)
{
	_loop_control->post_down_event (_index, cmd);
}

void
LooperPanel::released_events (wxString cmd)
{
	_loop_control->post_up_event (_index, cmd);

}

void
LooperPanel::tap_button_event ()
{
	_tap_val *= -1.0f;
	post_control_event (wxString("tap_trigger"), _tap_val);
}

void
LooperPanel::rate_button_event ()
{
	float val = 0.0;
	_loop_control->get_value(_index, "use_rate", val);

	val = val == 0.0f ? 1.0f : 0.0f;
	post_control_event (wxString("use_rate"), val);
}

void
LooperPanel::clicked_events (wxString cmd)
{
	if (cmd == wxT("save"))
	{
		// popup local file dialog if we are local
		if (_loop_control->is_engine_local()) {

			wxString filename = ::wxFileSelector(wxT("Choose file to save loop"), wxT(""), wxT(""), wxT(".wav"), wxT("*.*"), wxSAVE|wxCHANGE_DIR);
			if ( !filename.empty() )
			{
				// todo: specify format
				_loop_control->post_save_loop (_index, filename);
			}
		}
		else {
			// popup basic filename text entry
			wxString filename = ::wxGetTextFromUser(wxString::Format("Choose file to save on remote host '%s'",
										 _loop_control->get_engine_host().c_str())
								, wxT("Save Loop"));

			if (!filename.empty()) {
				// todo: specify format
				_loop_control->post_save_loop (_index, filename);
			}
		}

		
		
	}
	else if (cmd == wxT("load"))
	{
		if (_loop_control->is_engine_local()) {

			wxString filename = wxFileSelector(wxT("Choose file to open"), wxT(""), wxT(""), wxT(""), wxT("*.*"), wxOPEN|wxCHANGE_DIR);
			if ( !filename.empty() )
			{
				_loop_control->post_load_loop (_index, filename);
			}
		}
		else {
			// popup basic filename text entry
			wxString filename = ::wxGetTextFromUser(wxString::Format("Choose file to load on remote host '%s'",
										 _loop_control->get_engine_host().c_str())
								, wxT("Save Loop"));

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
		break;
	default:
		break;
	}

	if (!ctrl.empty()) {
		post_control_event (ctrl, val);
	}
}

void
LooperPanel::check_events(wxCommandEvent &ev)
{
	int id = ev.GetId();

	switch (id) {
	case ID_RoundCheck:
		post_control_event (wxT("round"), _round_check->GetValue() ? 1.0f: 0.0f);
		break;
	case ID_QuantizeCheck:
		post_control_event (wxT("quantize"), _quantize_check->GetValue() ? 1.0f: 0.0f);
		break;
	case ID_SyncCheck:
		post_control_event (wxT("sync"), _sync_check->GetValue() ? 1.0f: 0.0f);
		break;
	default:
		break;
	}

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
