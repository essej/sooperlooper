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
#include <wx/listctrl.h>
#include <wx/spinctrl.h>

#include <iostream>
#include <string>
#include <vector>

#include "main_panel.hpp"
#include "config_panel.hpp"
#include "loop_control.hpp"
#include "keyboard_target.hpp"

using namespace SooperLooperGui;
using namespace std;

#define DEFAULT_OSC_PORT 9951


enum {
	ID_ConnectButton = 8000,
	ID_CloseButton,
	ID_DisconnectButton,
	ID_MidiBrowseButton,
	ID_DefaultButton,
	ID_CommitButton,
	ID_RevertButton,
	ID_SessionBrowseButton
};

BEGIN_EVENT_TABLE(SooperLooperGui::ConfigPanel, wxPanel)

	EVT_BUTTON (ID_CloseButton, SooperLooperGui::ConfigPanel::on_button)
	EVT_BUTTON (ID_ConnectButton, SooperLooperGui::ConfigPanel::on_button)
	EVT_BUTTON (ID_DisconnectButton, SooperLooperGui::ConfigPanel::on_button)
	EVT_BUTTON (ID_DefaultButton, SooperLooperGui::ConfigPanel::on_button)
	EVT_BUTTON (ID_CommitButton, SooperLooperGui::ConfigPanel::on_button)
	EVT_BUTTON (ID_RevertButton, SooperLooperGui::ConfigPanel::on_button)
	EVT_BUTTON (ID_MidiBrowseButton, SooperLooperGui::ConfigPanel::on_button)
	EVT_BUTTON (ID_SessionBrowseButton, SooperLooperGui::ConfigPanel::on_button)

END_EVENT_TABLE()

	
// ctor(s)
	ConfigPanel::ConfigPanel(MainPanel * mainpan, wxWindow * parent, wxWindowID id,
		       const wxPoint& pos,
		       const wxSize& size,
		       long style ,
		       const wxString& name)

	: wxPanel ((wxWindow *)parent, id, pos, size, style, name), _parent (mainpan)
{
	init();
}

ConfigPanel::~ConfigPanel()
{

}


void ConfigPanel::init()
{
	wxBoxSizer * topsizer = new wxBoxSizer(wxVERTICAL);

	LoopControl::SpawnConfig & config = _parent->get_loop_control().get_default_spawn_config();
	LoopControl::SpawnConfig & currconfig = _parent->get_loop_control().get_spawn_config();
	
	wxStaticBox * shotBox = new wxStaticBox(this, -1, wxT("Current Connection"), wxDefaultPosition, wxDefaultSize);
        wxStaticBoxSizer * colsizer = new wxStaticBoxSizer(shotBox, wxVERTICAL);

	wxBoxSizer * rowsizer = new wxBoxSizer(wxHORIZONTAL);
	
	wxStaticText * statText = new wxStaticText(this, -1, wxT("Host:"));
	rowsizer->Add (statText, 0, wxALIGN_CENTRE_VERTICAL|wxALL, 2);
	_host_text = new wxTextCtrl(this, -1,  wxT(""), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("KeyAware"));
	_host_text->SetValue (wxString::FromAscii(config.host.c_str()));
	_host_text->SetToolTip(wxT("hostname of engine to attempt connection to --  leave empty for local machine"));
	rowsizer->Add (_host_text, 1, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	statText = new wxStaticText(this, -1,  wxT("Port:"));
	rowsizer->Add (statText, 0, wxALIGN_CENTRE_VERTICAL|wxALL, 2);
	_port_text = new wxTextCtrl(this, -1,  wxT(""), wxDefaultPosition, wxSize(70, -1), 0, wxDefaultValidator, wxT("KeyAware"));
	_port_text->SetValue (wxString::Format(wxT("%ld"), config.port));
	rowsizer->Add (_port_text, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	wxButton * butt = new wxButton(this, ID_DefaultButton, wxT("Defaults"));
	rowsizer->Add (butt, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	colsizer->Add (rowsizer, 0 , wxEXPAND|wxALL, 1);
	
	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	statText = new wxStaticText(this, -1,  wxT("Status:"));
	rowsizer->Add (statText, 0, wxALIGN_CENTRE_VERTICAL|wxALL, 2);
	_status_text = new wxTextCtrl(this, -1,  wxT(""), wxDefaultPosition, wxSize(100, -1), wxTE_READONLY, wxDefaultValidator, wxT("KeyAware"));
	rowsizer->Add (_status_text, 1, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	colsizer->Add (rowsizer, 0 , wxEXPAND|wxALL, 1);

	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	_connect_button = new wxButton(this, ID_ConnectButton, wxT("Connect"));
	rowsizer->Add (_connect_button, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	_force_spawn = new wxCheckBox(this, -1, wxT("start new engine"));
	rowsizer->Add (_force_spawn, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);
	cerr << "never spawn: " << config.never_spawn << endl;
	if (currconfig.never_spawn) {
		_force_spawn->Enable(false);
	}
	
	colsizer->Add (rowsizer, 0 , wxEXPAND|wxALL, 1);


	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	
	_disconnect_button = new wxButton(this, ID_DisconnectButton, wxT("Disconnect"));
	rowsizer->Add (_disconnect_button, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);
	_shutdown_check = new wxCheckBox(this, -1, wxT("shutdown engine"));
	rowsizer->Add (_shutdown_check, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);
	if (currconfig.never_spawn) {
		_shutdown_check->Enable(false);
	}
	
	colsizer->Add (rowsizer, 0 , wxEXPAND|wxALL, 1);
	

	topsizer->Add (colsizer, 0, wxEXPAND|wxALL, 4);


	shotBox = new wxStaticBox(this, -1, wxT("Startup Default Configuration"), wxDefaultPosition, wxDefaultSize);
        colsizer = new wxStaticBoxSizer(shotBox, wxVERTICAL);
	

	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	
	statText = new wxStaticText(this, -1, wxT("Host:"));
	rowsizer->Add (statText, 0, wxALIGN_CENTRE_VERTICAL|wxALL, 2);
	_def_host_text = new wxTextCtrl(this, -1,  wxT(""), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("KeyAware"));
	_def_host_text->SetValue (wxString::FromAscii(config.host.c_str()));
	_def_host_text->SetToolTip(wxT("hostname of engine to attempt connection to --  leave empty for local machine"));
	rowsizer->Add (_def_host_text, 1, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	statText = new wxStaticText(this, -1,  wxT("Port:"));
	rowsizer->Add (statText, 0, wxALIGN_CENTRE_VERTICAL|wxALL, 2);
	_def_port_text = new wxTextCtrl(this, -1,  wxT(""), wxDefaultPosition, wxSize(70, -1), 0, wxDefaultValidator, wxT("KeyAware"));
	_def_port_text->SetValue (wxString::Format(wxT("%ld"), config.port));
	rowsizer->Add (_def_port_text, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	_def_force_spawn = new wxCheckBox(this, -1, wxT("always start new engine"));
	rowsizer->Add (_def_force_spawn, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);
	
	colsizer->Add (rowsizer, 0 , wxEXPAND|wxALL, 1);
	
	
	wxFlexGridSizer * setsizer = new wxFlexGridSizer(0, 2, 4, 4);


	statText = new wxStaticText(this, -1, wxT("# Loops:"));
	setsizer->Add (statText, 0, wxALIGN_RIGHT|wxALIGN_CENTRE_VERTICAL);
	_num_loops_spin = new wxSpinCtrl(this, -1, wxT("1"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 16, config.num_loops, wxT("KeyAware"));
	_num_loops_spin->SetValue (config.num_loops);
	setsizer->Add (_num_loops_spin, 0);

	statText = new wxStaticText(this, -1, wxT("# Channels per loop:"));
	setsizer->Add (statText, 0, wxALIGN_RIGHT|wxALIGN_CENTRE_VERTICAL);

	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	_num_channels_spin = new wxSpinCtrl(this, -1, wxT("2"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 16, config.num_channels, wxT("KeyAware"));
	_num_channels_spin->SetValue (config.num_channels);
	rowsizer->Add (_num_channels_spin, 0, wxRIGHT, 3);
	_discrete_io_check = new wxCheckBox(this, -1, wxT("Separate I/O ports"));
	rowsizer->Add (_discrete_io_check, 0, wxALIGN_CENTRE_VERTICAL);
	setsizer->Add (rowsizer, 0);
	
	statText = new wxStaticText(this, -1, wxT("Loop time (secs minimum):"));
	setsizer->Add (statText, 0, wxALIGN_RIGHT|wxALIGN_CENTRE_VERTICAL);
	_secs_per_channel_spin = new wxSpinCtrl(this, -1, wxT("40"), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 4, 1000, (int)config.mem_secs, wxT("KeyAware"));
	_secs_per_channel_spin->SetValue ((int)config.mem_secs);
	setsizer->Add (_secs_per_channel_spin, 0);

	statText = new wxStaticText(this, -1, wxT("JACK client name:"));
	setsizer->Add (statText, 0, wxALIGN_RIGHT|wxALIGN_CENTRE_VERTICAL);
	_def_jack_name_text = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(100, -1), 0, wxDefaultValidator, wxT("KeyAware"));
	_def_jack_name_text->SetToolTip(wxT("JACK client base name -- leave blank for default"));
	_def_jack_name_text->SetValue (wxString::FromAscii(config.jack_name.c_str()));
	setsizer->Add (_def_jack_name_text, 0, wxALIGN_RIGHT|wxALIGN_CENTRE_VERTICAL);

	
	colsizer->Add (setsizer, 0, wxEXPAND|wxALL, 3);


	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	statText = new wxStaticText(this, -1, wxT("Default loaded session:"));
	rowsizer->Add (statText, 0, wxALIGN_CENTRE_VERTICAL|wxALL, 2);
	_def_session_text = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("KeyAware"));
	_def_session_text->SetValue (wxString::FromAscii(config.session_path.c_str()));
	rowsizer->Add (_def_session_text, 1, wxALL|wxALIGN_CENTRE_VERTICAL, 2);
	_session_browse_button = new wxButton(this, ID_SessionBrowseButton, wxT("Browse..."));
	rowsizer->Add (_session_browse_button, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	colsizer->Add (rowsizer, 0, wxEXPAND|wxALL, 3);
	
	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	statText = new wxStaticText(this, -1, wxT("Default MIDI bindings:"));
	rowsizer->Add (statText, 0, wxALIGN_CENTRE_VERTICAL|wxALL, 2);
	_def_midi_bind_text = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator, wxT("KeyAware"));
	_def_midi_bind_text->SetValue (wxString::FromAscii(config.midi_bind_path.c_str()));
	rowsizer->Add (_def_midi_bind_text, 1, wxALL|wxALIGN_CENTRE_VERTICAL, 2);
	_midi_browse_button = new wxButton(this, ID_MidiBrowseButton, wxT("Browse..."));
	rowsizer->Add (_midi_browse_button, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	colsizer->Add (rowsizer, 0, wxEXPAND|wxALL, 3);

	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	rowsizer->Add (1,1,1);
	butt = new wxButton(this, ID_RevertButton, wxT("Revert"));
	rowsizer->Add (butt, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);
	butt = new wxButton(this, ID_CommitButton, wxT("Commit Changes"));
	rowsizer->Add (butt, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);
	rowsizer->Add (1,1,1);

	colsizer->Add (rowsizer, 0, wxEXPAND|wxALL, 3);
	
	
	topsizer->Add (colsizer, 0, wxEXPAND|wxALL, 4);

	
	_parent->get_loop_control().LooperConnected.connect(mem_fun (*this, &ConfigPanel::looper_connected));
	_parent->get_loop_control().Disconnected.connect(mem_fun (*this, &ConfigPanel::refresh_state));
	
	refresh_state();
	refresh_defaults();
	
	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( topsizer );      // actually set the sizer
	//topsizer->Fit( this );            // set size to minimum size as calculated by the sizer
	//topsizer->SetSizeHints( this );   // set size hints to honour mininum size
}

void ConfigPanel::refresh_state()
{

	LoopControl & loopctrl = _parent->get_loop_control();

	// refresh current connection
	_host_text->SetValue (loopctrl.get_engine_host());
	_port_text->SetValue (wxString::Format(wxT("%d"), loopctrl.get_engine_port()));

	if (loopctrl.connected()) {
		_status_text->SetValue(wxT("Connected"));
	}
	else {
		_status_text->SetValue(wxT("Not Connected"));
	}

}

void ConfigPanel::refresh_defaults()
{
	LoopControl & loopctrl = _parent->get_loop_control();
	LoopControl::SpawnConfig & def_config = loopctrl.get_default_spawn_config();

	// refresh defaults
	_def_host_text->SetValue (wxString::FromAscii(def_config.host.c_str()));
	_def_port_text->SetValue (wxString::Format(wxT("%ld"), def_config.port));
	_def_force_spawn->SetValue (def_config.force_spawn);
	_num_loops_spin->SetValue( (int) def_config.num_loops);
	_num_channels_spin->SetValue( (int) def_config.num_channels);
	_discrete_io_check->SetValue (def_config.discrete_io);
	_secs_per_channel_spin->SetValue( (int) def_config.mem_secs);
	_def_jack_name_text->SetValue (wxString::FromAscii(def_config.jack_name.c_str()));
	_def_midi_bind_text->SetValue (wxString::FromAscii(def_config.midi_bind_path.c_str()));
	_def_session_text->SetValue (wxString::FromAscii(def_config.session_path.c_str()));
}

void ConfigPanel::looper_connected(int num)
{
	// just refresh state
	refresh_state();
}


void ConfigPanel::commit_changes()
{
	LoopControl & loopctrl = _parent->get_loop_control();
	LoopControl::SpawnConfig & config = loopctrl.get_default_spawn_config();

	config.host = _def_host_text->GetValue().ToAscii();
	_def_port_text->GetValue().ToLong(&config.port);
	config.num_loops = (long) _num_loops_spin->GetValue();
	config.num_channels = (long) _num_channels_spin->GetValue();
	config.discrete_io = (bool) _discrete_io_check->GetValue();
	config.mem_secs = (double) _secs_per_channel_spin->GetValue();

	config.jack_name = _def_jack_name_text->GetValue().ToAscii();
	config.midi_bind_path = _def_midi_bind_text->GetValue().ToAscii();
	config.session_path = _def_session_text->GetValue().ToAscii();

	config.force_spawn = _def_force_spawn->GetValue();

	_parent->save_rc();
}


void ConfigPanel::on_button (wxCommandEvent &ev)
{
	if (ev.GetId() == ID_CloseButton) {
		Show(false);
	}
	else if (ev.GetId() == ID_DisconnectButton) {
		wxString host = _host_text->GetValue();
		wxString portstr = _port_text->GetValue();
		_parent->get_loop_control().disconnect (_shutdown_check->GetValue());

		// restore them
		_host_text->SetValue(host);
		_port_text->SetValue(portstr);
	}
	else if (ev.GetId() == ID_ConnectButton) {
		// set up info
		LoopControl & loopctrl = _parent->get_loop_control();
		LoopControl::SpawnConfig & config = loopctrl.get_spawn_config();

		long tmplong;
		wxString tmpval = _port_text->GetValue();
		if (tmpval.ToLong (&tmplong)) {
			config.port = tmplong;
		}

		
		loopctrl.disconnect(); // first
		
		loopctrl.get_spawn_config().host = _host_text->GetValue().ToAscii();

		
		config.num_loops = (long) _num_loops_spin->GetValue();
		config.num_channels = (long) _num_channels_spin->GetValue();
		config.mem_secs = (double) _secs_per_channel_spin->GetValue();
		config.jack_name = _def_jack_name_text->GetValue().ToAscii();
		config.midi_bind_path = _def_midi_bind_text->GetValue().ToAscii();
		config.session_path = _def_session_text->GetValue().ToAscii();
		config.force_spawn = false;
		config.never_spawn = !_force_spawn->GetValue();

		loopctrl.connect ();
		//refresh_state();
	}
	else if (ev.GetId() == ID_DefaultButton) {
		_host_text->SetValue(wxT(""));
		_port_text->SetValue (wxString::Format(wxT("%d"), DEFAULT_OSC_PORT));
	}
	else if (ev.GetId() == ID_CommitButton) {
		commit_changes();
	}
	else if (ev.GetId() == ID_RevertButton) {
		refresh_defaults();
	}
	else if (ev.GetId() == ID_MidiBrowseButton) {
		
		_parent->get_keyboard().set_enabled(false);
		wxString filename = _parent->do_file_selector(wxT("Choose midi binding file to use"), wxT(""), wxT("*.slb"), wxFD_OPEN|wxFD_CHANGE_DIR);
		_parent->get_keyboard().set_enabled(true);
		
		if ( !filename.empty() )
		{
			_def_midi_bind_text->SetValue(filename);
		}
	}
	else if (ev.GetId() == ID_SessionBrowseButton) {
		
		_parent->get_keyboard().set_enabled(false);
		wxString filename = _parent->do_file_selector(wxT("Choose session file to use"), wxT(""), wxT("*.slsess"), wxFD_OPEN|wxFD_CHANGE_DIR);
		//wxString filename = wxFileSelector(wxT("Choose session file to use"), wxT(""), wxT(""), wxT(""), wxT("*.slsess"), wxOPEN|wxCHANGE_DIR);
		_parent->get_keyboard().set_enabled(true);
		
		if ( !filename.empty() )
		{
			_def_session_text->SetValue(filename);
		}
	}
	else {
		ev.Skip();
	}
}
