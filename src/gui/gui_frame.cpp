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
#include <wx/utils.h>
#include <wx/dir.h>

#include <iostream>
#include <cstdio>
#include <cmath>

#include "version.h"

#include "gui_frame.hpp"
#include "gui_app.hpp"
#include "looper_panel.hpp"
#include "loop_control.hpp"
#include "slider_bar.hpp"
#include "choice_box.hpp"
#include "check_box.hpp"
#include "pix_button.hpp"
#include "keyboard_target.hpp"
#include "keys_dialog.hpp"
#include "xml++.hpp"

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
	ID_RoundCheck,
	ID_TapTempoButton
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

	EVT_MENU(ID_KeybindingsMenu, GuiFrame::on_view_menu)
	
END_EVENT_TABLE()

GuiFrame::GuiFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame((wxFrame *)NULL, -1, title, pos, size, wxDEFAULT_FRAME_STYLE, "sooperlooper")

{
	_keyboard = new KeyboardTarget (this, "gui_frame");
	_curr_loop = -1;
	_tapdelay_val = 1.0f;
	_keys_dialog = 0;

	_rcdir = wxGetHomeDir() + wxFileName::GetPathSeparator() + wxT(".sooperlooper");

	intialize_keybindings ();
		
	load_rc();
	
	init();

	_update_timer = new wxTimer(this, ID_UpdateTimer);
	_update_timer->Start(100);
}

GuiFrame::~GuiFrame()
{
	save_rc();
	
	for (unsigned int i=0; i < _looper_panels.size(); ++i) {
		// unregister
		_loop_control->register_input_controls((int) i, true);
	}

	delete _loop_control;

	delete _keyboard;
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

 	_taptempo_button = new PixButton(this, ID_TapTempoButton);
	load_bitmaps (_taptempo_button, wxT("tap_tempo"));
	_taptempo_button->pressed.connect (slot (*this, &GuiFrame::on_taptempo_event));
 	rowsizer->Add (_taptempo_button, 0, wxALL, 2);
	

	_eighth_cycle_bar = new SliderBar(this, ID_EighthSlider, 1.0f, 128.0f, 8.0f, wxDefaultPosition, wxSize(110, 22));
	_eighth_cycle_bar->set_units(wxT(""));
	_eighth_cycle_bar->set_label(wxT("8th/cycle"));
	_eighth_cycle_bar->set_snap_mode (SliderBar::IntegerSnap);
	_eighth_cycle_bar->SetFont (sliderFont);
	_eighth_cycle_bar->value_changed.connect (slot (*this,  &GuiFrame::on_eighth_change));
	rowsizer->Add (_eighth_cycle_bar, 0, wxALL, 2);
	

	_quantize_choice = new ChoiceBox (this, ID_QuantizeChoice, wxDefaultPosition, wxSize (110, 22));
	_quantize_choice->SetFont (sliderFont);
	_quantize_choice->set_label (wxT("quantize"));
	_quantize_choice->value_changed.connect (slot (*this,  &GuiFrame::on_quantize_change));
	_quantize_choice->append_choice (wxT("off"), 0);
	_quantize_choice->append_choice (wxT("cycle"), 1);
	_quantize_choice->append_choice (wxT("8th"), 2);
	_quantize_choice->append_choice (wxT("loop"), 3);
	rowsizer->Add (_quantize_choice, 0, wxALL, 2);

	_round_check = new CheckBox (this, ID_RoundCheck, wxT("round"), wxDefaultPosition, wxSize(80, 22));
	_round_check->SetFont (sliderFont);
	_round_check->value_changed.connect (slot (*this, &GuiFrame::on_round_check));
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

	_scroller->SetFocus();
}

void
GuiFrame::init_syncto_choice()
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
//	_sync_choice->append_choice (wxT("Jack"), -1);
//	_sync_choice->append_choice (wxT("BrotherSync"), -4);

	// the remaining choices are loops
	for (unsigned int i=0; i < _looper_panels.size(); ++i) {
		_sync_choice->append_choice (wxString::Format(wxT("Loop %d"), i+1), i+1);
	}

	update_syncto_choice ();
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

	set_curr_loop (_curr_loop);
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
		update_syncto_choice ();
	}

	// quantize from first loop
 	if (_loop_control->is_updated(0, "quantize")) {
		_loop_control->get_value(0, "quantize", val);
 		_quantize_choice->set_index_value ((int)val);
	}

 	if (_loop_control->is_updated(0, "round")) {
		_loop_control->get_value(0, "round", val);
 		_round_check->set_value (val > 0.0);
	}
	
	
}

void
GuiFrame::update_syncto_choice()
{
	float val = 0.0f;
	_loop_control->get_global_value("sync_source", val);
	
	long data = (long) val;
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

	value = (float) _sync_choice->get_data_value();
	
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
GuiFrame::on_round_check (bool val)
{
	// send for all loops
	_loop_control->post_ctrl_change (-1, wxT("round"), val ? 1.0f: 0.0f);
}

void
GuiFrame::on_taptempo_event ()
{
	_loop_control->post_ctrl_change (-2, wxT("tap_tempo"), 1.0f);
}


wxString
GuiFrame::get_pixmap_path (const wxString & namebase)
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
GuiFrame::load_bitmaps (PixButton * butt, wxString namebase)
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
GuiFrame::process_key_event (wxKeyEvent &ev)
{
	// this is a pretty extreme hack
	// to let textfields, etc named with the right name
	// get their key events
	static wxString textname = "KeyAware";

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


void GuiFrame::intialize_keybindings ()
{
	
	KeyboardTarget::add_action ("record", bind (slot (*this, &GuiFrame::command_action), wxT("record")));
	KeyboardTarget::add_action ("overdub", bind (slot (*this, &GuiFrame::command_action), wxT("overdub")));
	KeyboardTarget::add_action ("multiply", bind (slot (*this, &GuiFrame::command_action), wxT("multiply")));
	KeyboardTarget::add_action ("insert", bind (slot (*this, &GuiFrame::command_action), wxT("insert")));
	KeyboardTarget::add_action ("replace", bind (slot (*this, &GuiFrame::command_action), wxT("replace")));
	KeyboardTarget::add_action ("reverse", bind (slot (*this, &GuiFrame::command_action), wxT("reverse")));
	KeyboardTarget::add_action ("scratch", bind (slot (*this, &GuiFrame::command_action), wxT("scratch")));
	KeyboardTarget::add_action ("mute", bind (slot (*this, &GuiFrame::command_action), wxT("mute")));
	KeyboardTarget::add_action ("undo", bind (slot (*this, &GuiFrame::command_action), wxT("undo")));
	KeyboardTarget::add_action ("redo", bind (slot (*this, &GuiFrame::command_action), wxT("redo")));	
	KeyboardTarget::add_action ("oneshot", bind (slot (*this, &GuiFrame::command_action), wxT("oneshot")));
	KeyboardTarget::add_action ("trigger", bind (slot (*this, &GuiFrame::command_action), wxT("trigger")));

	KeyboardTarget::add_action ("delay", bind (slot (*this, &GuiFrame::misc_action), wxT("delay")));
	KeyboardTarget::add_action ("taptempo", bind (slot (*this, &GuiFrame::misc_action), wxT("taptempo")));
	KeyboardTarget::add_action ("load", bind (slot (*this, &GuiFrame::misc_action), wxT("load")));
	KeyboardTarget::add_action ("save", bind (slot (*this, &GuiFrame::misc_action), wxT("save")));

	KeyboardTarget::add_action ("select_loop_1", bind (slot (*this, &GuiFrame::select_loop_action), 1));
	KeyboardTarget::add_action ("select_loop_2", bind (slot (*this, &GuiFrame::select_loop_action), 2));
	KeyboardTarget::add_action ("select_loop_3", bind (slot (*this, &GuiFrame::select_loop_action), 3));
	KeyboardTarget::add_action ("select_loop_4", bind (slot (*this, &GuiFrame::select_loop_action), 4));
	KeyboardTarget::add_action ("select_loop_5", bind (slot (*this, &GuiFrame::select_loop_action), 5));
	KeyboardTarget::add_action ("select_loop_6", bind (slot (*this, &GuiFrame::select_loop_action), 6));
	KeyboardTarget::add_action ("select_loop_7", bind (slot (*this, &GuiFrame::select_loop_action), 7));
	KeyboardTarget::add_action ("select_loop_8", bind (slot (*this, &GuiFrame::select_loop_action), 8));
	KeyboardTarget::add_action ("select_loop_9", bind (slot (*this, &GuiFrame::select_loop_action), 9));
	KeyboardTarget::add_action ("select_loop_all", bind (slot (*this, &GuiFrame::select_loop_action), -1));
	
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
	_keyboard->add_binding ("l", "delay");
	_keyboard->add_binding ("h", "oneshot");
	_keyboard->add_binding (" ", "trigger");
	_keyboard->add_binding ("t", "taptempo");
	_keyboard->add_binding ("Control-s", "save");
	_keyboard->add_binding ("Control-o", "load");

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

void GuiFrame::on_view_menu (wxCommandEvent &ev)
{
	if (ev.GetId() == ID_KeybindingsMenu) {
		if (!_keys_dialog) {
			_keys_dialog = new KeysDialog(this, -1, wxT("SooperLooper Key bindings"));
			_keys_dialog->SetSize (230,410);
		}
		else if (!_keys_dialog->IsShown()) {
			_keys_dialog->refresh_state();
		}

		_keys_dialog->Show(true);
		
	}
}


void GuiFrame::command_action (bool release, wxString cmd)
{
	if (release) {
		_loop_control->post_up_event (_curr_loop, cmd);
	}
	else {
		_loop_control->post_down_event (_curr_loop, cmd);
	}
}

void GuiFrame::select_loop_action (bool release, int index)
{
	if (release) return;

	index--;
	
	if (index < (int) _looper_panels.size()) {

		set_curr_loop (index);
	}
}


void GuiFrame::misc_action (bool release, wxString cmd)
{
	int index = _curr_loop;

	// only on press
	if (release) return;

	if (index < 0) index = -1;
	
	if (cmd == wxT("taptempo")) {

		on_taptempo_event();
	}
	else if (cmd == wxT("delay")) {
		_tapdelay_val *= -1.0f;
		_loop_control->post_ctrl_change (index, wxString("tap_trigger"), _tapdelay_val);
	}
	else if (cmd == wxT("save"))
	{
		if (index < 0) {
			index = 0;
		}
		// popup local file dialog if we are local
		if (_loop_control->is_engine_local()) {

			wxString filename = ::wxFileSelector(wxT("Choose file to save loop"), wxT(""), wxT(""), wxT(".wav"), wxT("*.*"), wxSAVE|wxCHANGE_DIR);
			if ( !filename.empty() )
			{
				// todo: specify format
				_loop_control->post_save_loop (index, filename);
			}
		}
		else {
			// popup basic filename text entry
			wxString filename = ::wxGetTextFromUser(wxString::Format("Choose file to save on remote host '%s'",
										 _loop_control->get_engine_host().c_str())
								, wxT("Save Loop"));

			if (!filename.empty()) {
				// todo: specify format
				_loop_control->post_save_loop (index, filename);
			}
		}

		
		
	}
	else if (cmd == wxT("load"))
	{
		if (index < 0) {
			index = 0;
		}

		if (_loop_control->is_engine_local()) {

			wxString filename = wxFileSelector(wxT("Choose file to open"), wxT(""), wxT(""), wxT(""), wxT("*.*"), wxOPEN|wxCHANGE_DIR);
			if ( !filename.empty() )
			{
				_loop_control->post_load_loop (index, filename);
			}
		}
		else {
			// popup basic filename text entry
			wxString filename = ::wxGetTextFromUser(wxString::Format("Choose file to load on remote host '%s'",
										 _loop_control->get_engine_host().c_str())
								, wxT("Save Loop"));

			if (!filename.empty()) {
				// todo: specify format
				_loop_control->post_load_loop (index, filename);
			}
		}
	}
}


bool GuiFrame::load_rc()
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

	XMLNode * bindingsNode = rootNode->find_named_node ("KeyBindings");
	if (!bindingsNode ) {
		fprintf(stderr, "Preset Channels node not found in %s!\n", configfname.c_str()); 
		return false;
	}

	_keyboard->set_binding_state (*bindingsNode);

	return true;
}

bool GuiFrame::save_rc()
{
	wxString dirname = _rcdir;
	
	if ( ! wxDirExists(dirname) ) {
		if (!wxMkdir ( dirname.fn_str(), 0755 )) {
			printf ("Error creating %s\n", static_cast<const char *> (dirname.mb_str())); 
			return false;
		}
	}

	// make xmltree
	XMLTree configdoc;
	XMLNode * rootNode = new XMLNode("SLConfig");
	rootNode->add_property("version", sooperlooper_version);
	configdoc.set_root (rootNode);
	
	XMLNode * bindingsNode = rootNode->add_child ("KeyBindings");

	bindingsNode->add_child_nocopy (_keyboard->get_binding_state());


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

void GuiFrame::set_curr_loop (int index)
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
