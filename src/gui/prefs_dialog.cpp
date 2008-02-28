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
#include <wx/notebook.h>

#include <iostream>
#include <string>
#include <vector>

#include "main_panel.hpp"
#include "prefs_dialog.hpp"
#include "config_panel.hpp"
#include "keys_panel.hpp"
#include "midi_bind_panel.hpp"
#include "latency_panel.hpp"

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
	ID_RevertButton
};

BEGIN_EVENT_TABLE(SooperLooperGui::PrefsDialog, wxFrame)
	EVT_CLOSE(SooperLooperGui::PrefsDialog::on_close)

END_EVENT_TABLE()

	
// ctor(s)
PrefsDialog::PrefsDialog(MainPanel * parent, wxWindowID id, const wxString& title,
		       const wxPoint& pos,
		       const wxSize& size,
		       long style ,
		       const wxString& name)

	: wxFrame ((wxWindow *)parent, id, title, pos, size, style, name), _parent (parent)
{
	init();
}

PrefsDialog::~PrefsDialog()
{

}


void PrefsDialog::init()
{
	wxBoxSizer * topsizer = new wxBoxSizer(wxVERTICAL);
	
	_notebook = new wxNotebook (this, -1);
#if wxCHECK_VERSION(2,6,0)
#else
	wxNotebookSizer *nbs = new wxNotebookSizer( _notebook );
#endif
	
	_config_panel = new ConfigPanel (_parent, _notebook, -1);
	_notebook->AddPage (_config_panel, wxT("Connection"), true);
	
	_keys_panel = new KeysPanel(_parent, _notebook, -1);
	_notebook->AddPage (_keys_panel, wxT("Key Bindings"), false);

	_midi_panel = new MidiBindPanel(_parent, _notebook, -1);
	_notebook->AddPage (_midi_panel, wxT("MIDI Bindings"), false);

	_latency_panel = new LatencyPanel(_parent, _notebook, -1);
	_notebook->AddPage (_latency_panel, wxT("Latency/Misc"), false);
	
#if wxCHECK_VERSION(2,6,0)
	topsizer->Add (_notebook, 1, wxEXPAND|wxALL, 6);
#else
	topsizer->Add (nbs, 1, wxEXPAND|wxALL, 6);
#endif

	
	
	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( topsizer );      // actually set the sizer
	topsizer->Fit( this );            // set size to minimum size as calculated by the sizer
	topsizer->SetSizeHints( this );   // set size hints to honour mininum size
}

void PrefsDialog::refresh_state ()
{
	_config_panel->refresh_state();
	_keys_panel->refresh_state();
	_midi_panel->refresh_state();
	_latency_panel->refresh_state();
}


void PrefsDialog::on_close (wxCloseEvent &ev)
{
	if (!ev.CanVeto()) {
		
		Destroy();
	}
	else {
		ev.Veto();
		
		Show(false);
	}
}
