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

#include <iostream>

#include "gui_frame.hpp"
#include "gui_app.hpp"
#include "looper_panel.hpp"
#include "loop_control.hpp"
#include "slider_bar.hpp"
#include "choice_box.hpp"

using namespace SooperLooperGui;
using namespace std;

enum {
	ID_UpdateTimer = 9000,
	ID_AboutMenu,
	ID_HelpTipsMenu,
	ID_ConnectionMenu,
	ID_KeybindingsMenu,
	ID_MidiBindingsMenu,
	ID_Quit,
	ID_QuitStop,
	ID_AddLoop,
	ID_RemoveLoop,
	ID_TempoSlider,
	ID_SyncChoice,
	ID_EighthSlider,
	ID_QuantizeChoice,
	ID_RoundCheck
};


BEGIN_EVENT_TABLE(GuiFrame, wxFrame)

	EVT_IDLE(GuiFrame::OnIdle)
	EVT_SIZE(GuiFrame::OnSize)
	EVT_PAINT(GuiFrame::OnPaint)
	EVT_TIMER(ID_UpdateTimer, GuiFrame::OnUpdateTimer)

	EVT_MENU(ID_Quit, GuiFrame::OnQuit)
	EVT_MENU(ID_QuitStop, GuiFrame::OnQuit)

	EVT_MENU(ID_AddLoop, GuiFrame::on_add_loop)
	EVT_MENU(ID_RemoveLoop, GuiFrame::on_remove_loop)

	EVT_CHECKBOX (ID_RoundCheck, GuiFrame::on_round_check)
	
	
END_EVENT_TABLE()

GuiFrame::GuiFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame((wxFrame *)NULL, -1, title, pos, size, wxDEFAULT_FRAME_STYLE, "sooperlooper")
{

	init();

	_update_timer = new wxTimer(this, ID_UpdateTimer);
	_update_timer->Start(100);
}

GuiFrame::~GuiFrame()
{
	for (unsigned int i=0; i < _looper_panels.size(); ++i) {
		// unregister
		_loop_control->register_input_controls((int) i, true);
	}

	delete _loop_control;
}

void
GuiFrame::init()
{
	_main_sizer = new wxBoxSizer(wxVERTICAL);
	_topsizer = new wxBoxSizer(wxVERTICAL);

	//wxBoxSizer * rowsizer = new wxBoxSizer(wxHORIZONTAL);
	wxInitAllImageHandlers();
	
	SetBackgroundColour(*wxBLACK);
	SetThemeEnabled(false);
	
	GuiApp & guiapp = ::wxGetApp();

	wxFont sliderFont = *wxSMALL_FONT;
	
	wxBoxSizer * rowsizer = new wxBoxSizer(wxHORIZONTAL);

	rowsizer->Add (1, 1, 1);

	_sync_choice = new ChoiceBox (this, ID_SyncChoice, wxDefaultPosition, wxSize (140, 22));
	_sync_choice->set_label (wxT("sync to"));
	_sync_choice->SetFont (sliderFont);
	_sync_choice->value_changed.connect (slot (*this,  &GuiFrame::on_syncto_change));
	
	rowsizer->Add (_sync_choice, 0, wxALL, 2);
	
	_tempo_bar = new SliderBar(this, ID_TempoSlider, 0.0f, 300.0f, 120.0f, wxDefaultPosition, wxSize(150, 22));
	_tempo_bar->set_units(wxT("bpm"));
	_tempo_bar->set_label(wxT("tempo"));
	_tempo_bar->set_snap_mode (SliderBar::IntegerSnap);
	_tempo_bar->SetFont (sliderFont);
	_tempo_bar->value_changed.connect (slot (*this,  &GuiFrame::on_tempo_change));
	rowsizer->Add (_tempo_bar, 0, wxALL, 2);


	_eighth_cycle_bar = new SliderBar(this, ID_EighthSlider, 1.0f, 128.0f, 8.0f, wxDefaultPosition, wxSize(110, 22));
	_eighth_cycle_bar->set_units(wxT(""));
	_eighth_cycle_bar->set_label(wxT("8th/cycle"));
	_eighth_cycle_bar->set_snap_mode (SliderBar::IntegerSnap);
	_eighth_cycle_bar->SetFont (sliderFont);
	_eighth_cycle_bar->value_changed.connect (slot (*this,  &GuiFrame::on_eighth_change));
	rowsizer->Add (_eighth_cycle_bar, 0, wxALL, 2);
	

	_quantize_choice = new ChoiceBox (this, ID_QuantizeChoice, wxDefaultPosition, wxSize (110, 22));
	_quantize_choice->set_label (wxT("quantize"));
	_quantize_choice->SetFont (sliderFont);
	_quantize_choice->value_changed.connect (slot (*this,  &GuiFrame::on_quantize_change));
	_quantize_choice->append_choice (wxT("off"));
	_quantize_choice->append_choice (wxT("cycle"));
	_quantize_choice->append_choice (wxT("8th"));
	_quantize_choice->append_choice (wxT("loop"));
	rowsizer->Add (_quantize_choice, 0, wxALL, 2);

	_round_check = new wxCheckBox(this, ID_RoundCheck, wxT("round"));
	_round_check->SetFont(sliderFont);
	_round_check->SetBackgroundColour(wxColour(90,90,90));
	_round_check->SetForegroundColour(*wxWHITE);
	rowsizer->Add (_round_check, 0, wxALL, 2);
	
	rowsizer->Add (1, 1, 1);

	
	
	_topsizer->Add (rowsizer, 0, wxALL|wxEXPAND, 4);

	
	_scroller = new wxScrolledWindow(this, -1, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
	_scroller->SetBackgroundColour(*wxBLACK);
	
	_loop_control = new LoopControl(guiapp.get_host(), guiapp.get_port(), guiapp.get_force_spawn(),
					guiapp.get_exec_name(), guiapp.get_engine_args());

	// todo request how many loopers to construct based on connection
	_loop_control->LooperConnected.connect (slot (*this, &GuiFrame::init_loopers));


	wxMenuBar *menuBar = new wxMenuBar();

	wxMenu *menuFile = new wxMenu(wxT(""));

	menuFile->Append(ID_AddLoop, wxT("Add Loop"), wxT("Add one default loop"));
	menuFile->Append(ID_RemoveLoop, wxT("Remove Loop"), wxT("Remove last loop"));

	menuFile->AppendSeparator();
	
	menuFile->Append(ID_Quit, wxT("Quit but Leave Engine Running\tCtrl-Shift-Q"), wxT("Exit from GUI and leave engine running"));
	menuFile->Append(ID_QuitStop, wxT("Quit and Stop Engine\tCtrl-Q"), wxT("Exit from GUI and stop engine"));
	
	menuBar->Append(menuFile, wxT("&Control"));
	
	menuFile = new wxMenu(wxT(""));

	menuFile->Append(ID_ConnectionMenu, wxT("Looper &Connection...\tCtrl-C"), wxT("Configure Looper Engine Connection"));
	menuFile->Append(ID_KeybindingsMenu, wxT("&Key Bindings...\tCtrl-K"), wxT("Configure Keybindings"));
	menuFile->Append(ID_MidiBindingsMenu, wxT("&Midi Bindings...\tCtrl-M"), wxT("Configure Midi bindings"));
	
	menuBar->Append(menuFile, wxT("&Configure"));

	wxMenu *menuHelp = new wxMenu(wxT(""));
	menuHelp->Append(ID_HelpTipsMenu, wxT("&Usage Tips...\tCtrl-H"), wxT("Show Usage Tips window"));
	menuHelp->Append(ID_AboutMenu, wxT("&About...\tCtrl-A"), wxT("Show about dialog"));
	menuBar->Append(menuHelp, wxT("&Help"));
	
	// ... and attach this menu bar to the frame
	SetMenuBar(menuBar);


	_topsizer->Add (_scroller, 1, wxEXPAND);
	
	_scroller->SetSizer( _main_sizer );      // actually set the sizer
	_scroller->SetAutoLayout( true );     // tell dialog to use sizer

	_scroller->SetScrollRate (0, 30);
	_scroller->EnableScrolling (true, true);

	//_main_sizer->Fit( _scroller );            // set size to minimum size as calculated by the sizer
	_main_sizer->SetSizeHints( _scroller );   // set size hints to honour mininum size

	
	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( _topsizer );      // actually set the sizer
	_topsizer->Fit( this );            // set size to minimum size as calculated by the sizer
	_topsizer->SetSizeHints( this );   // set size hints to honour mininum size
}

void
GuiFrame::init_syncto_choice()
{
	_sync_choice->clear_choices ();
	_sync_choice->append_choice (wxT("None"));
	_sync_choice->append_choice (wxT("Internal"));
	_sync_choice->append_choice (wxT("MidiClock"));
	_sync_choice->append_choice (wxT("Jack"));
	_sync_choice->append_choice (wxT("BrotherSync"));

	// the remaining choices are loops
	for (unsigned int i=0; i < _looper_panels.size(); ++i) {
		_sync_choice->append_choice (wxString::Format(wxT("Loop %d"), i+1));
	}
	
}

    


void
GuiFrame::init_loopers (int count)
{
	LooperPanel * looperpan;	

	if (count > (int) _looper_panels.size()) {
		while (count > (int) _looper_panels.size()) {
			looperpan = new LooperPanel(_loop_control, _scroller, -1);
			looperpan->set_index(_looper_panels.size());
			_main_sizer->Add (looperpan, 0, wxEXPAND|wxALL, 0);
			_looper_panels.push_back (looperpan);
		}
	}
	else if (count < (int)_looper_panels.size()) {
		while (count < (int)_looper_panels.size()) {
			looperpan = _looper_panels.back();
			_looper_panels.pop_back();
			_main_sizer->Remove(looperpan);
			looperpan->Destroy();
		}
	}

	_scroller->SetClientSize(_scroller->GetClientSize());
	_scroller->Layout();
	_scroller->SetScrollRate(0,30);

 	if (!_looper_panels.empty()) {
 		wxSize bestsz = _looper_panels[0]->GetBestSize();
		//cerr << "best w: " << bestsz.GetWidth() << endl;
 		_scroller->SetVirtualSizeHints (bestsz.GetWidth(), -1);
		_topsizer->Layout();
// 		_topsizer->Fit(this);
// 		_topsizer->SetSizeHints(this);
 	}
	

	//_main_sizer->Layout();
	//_main_sizer->Fit(_scroller);
	//_main_sizer->SetSizeHints( _scroller );   // set size hints to honour mininum size
	
	// request all values for initial state
	_loop_control->request_global_values ();
	
	for (unsigned int i=0; i < _looper_panels.size(); ++i) {
		_looper_panels[i]->set_index(i);
		_loop_control->register_input_controls((int) i);
		_loop_control->request_all_values ((int)i);
	}

	init_syncto_choice ();
}


void
GuiFrame::OnUpdateTimer(wxTimerEvent &ev)
{
	_loop_control->update_values();

	for (unsigned int i=0; i < _looper_panels.size(); ++i) {
		_loop_control->request_values ((int)i);
	}

	_loop_control->update_values();

	for (unsigned int i=0; i < _looper_panels.size(); ++i) {
		_looper_panels[i]->update_controls();
	}

	update_controls ();
}

void
GuiFrame::update_controls()
{
	// get recent controls from loop control
	float val;
	
	if (_loop_control->is_global_updated("tempo")) {
		_loop_control->get_global_value("tempo", val);
		_tempo_bar->set_value (val);
	}

	if (_loop_control->is_global_updated("eighth_per_cycle")) {
		_loop_control->get_global_value("eighth_per_cycle", val);
		_eighth_cycle_bar->set_value (val);
	}
	
	if (_loop_control->is_global_updated("sync_source")) {
		_loop_control->get_global_value("sync_source", val);

		int index = -1;
// 		BrotherSync = -4,
// 		InternalTempoSync = -3,
// 		MidiClockSync = -2,
// 		JackSync = -1,
// 		NoSync = 0

	        if (val == 0.0f) {
			index = 0;
		}
		if (val == -3.0f) {
			index = 1;
		} else if (val == -2.0f) {
			index = 2;
		} else if (val == -1.0f) {
			index = 3;
		} else if (val == -4.0f) {
			index = 4;
		}
		else if (val > 0.0f) {
			// the loop instances
			index = (int) (val + 4);
		}

		_sync_choice->set_index_value (index);
	}

	// quantize from first loop
 	if (_loop_control->is_updated(0, "quantize")) {
		_loop_control->get_value(0, "quantize", val);
 		_quantize_choice->set_index_value ((int)val);
	}

 	if (_loop_control->is_updated(0, "round")) {
		_loop_control->get_value(0, "round", val);
 		_round_check->SetValue (val > 0.0);
	}
	
	
}


void
GuiFrame::OnQuit(wxCommandEvent& event)
{
	int id = event.GetId();
	
	if (id == ID_Quit) {
		Close();
	}
	else if (id == ID_QuitStop) {
		// send quit command to looper
		_loop_control->send_quit();
		Close();
	}
}

void
GuiFrame::OnHide(wxCommandEvent &event)
{

}

void
GuiFrame::OnSize(wxSizeEvent & event)
{
	event.Skip();

}

void GuiFrame::OnPaint(wxPaintEvent & event)
{
	event.Skip();

}

void
GuiFrame::OnIdle(wxIdleEvent& event)
{
	
	event.Skip();
}

void
GuiFrame::on_add_loop (wxCommandEvent &ev)
{
	_loop_control->post_add_loop();
}

void
GuiFrame::on_remove_loop (wxCommandEvent &ev)
{
	_loop_control->post_remove_loop();
}

void
GuiFrame::on_tempo_change (float value)
{
	_loop_control->post_global_ctrl_change ("tempo", value);
}

void
GuiFrame::on_eighth_change (float value)
{
	_loop_control->post_global_ctrl_change ("eighth_per_cycle", value);
}


void
GuiFrame::on_syncto_change (int index, wxString val)
{
// 		BrotherSync = -4,
// 		InternalTempoSync = -3,
// 		MidiClockSync = -2,
// 		JackSync = -1,
// 		NoSync = 0
//            >0 is loop number
	
	float value = 0.0f;
	
	if (index == 0) {
		value = 0;
	}
	else if (index == 1) {
		value = -3;
	} else if (index == 2) {
		value = -2;
	} else if (index == 3) {
		value = -1;
	} else if (index == 4) {
		value = -4;
	}
	else if (index > 4) {
		// the loop instances
		value = (float) (index - 4);
	}

	_loop_control->post_global_ctrl_change ("sync_source", value);
}


void
GuiFrame::on_quantize_change (int index, wxString val)
{
	// 0 is none, 1 is cycle, 2 is eighth, 3 is loop
	// send for all loops
	_loop_control->post_ctrl_change (-1, wxT("quantize"), (float) index);
}

void
GuiFrame::on_round_check (wxCommandEvent &ev)
{
	// send for all loops
	_loop_control->post_ctrl_change (-1, wxT("round"), _round_check->GetValue() ? 1.0f: 0.0f);
}
