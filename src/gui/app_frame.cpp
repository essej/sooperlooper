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

#include "app_frame.hpp"
#include "main_panel.hpp"
#include "gui_app.hpp"
#include "keyboard_target.hpp"
#include "help_window.hpp"
#include "prefs_dialog.hpp"
#include "loop_control.hpp"

#include "pixmaps/sl_splash.xpm"

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
	ID_SaveSessionAudio,
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
	ID_StayOnTop,
};


BEGIN_EVENT_TABLE(AppFrame, wxFrame)

	EVT_CLOSE(AppFrame::OnClose)

	EVT_ACTIVATE (AppFrame::OnActivate)
	EVT_ACTIVATE_APP (AppFrame::OnActivate)
	
	EVT_MENU(ID_Quit, AppFrame::OnQuit)
	EVT_MENU(ID_QuitStop, AppFrame::OnQuit)

	EVT_MENU(ID_AboutMenu, AppFrame::on_about)
	EVT_MENU(ID_HelpTipsMenu, AppFrame::on_help)

	EVT_MENU(ID_AddLoop, AppFrame::on_add_loop)
	EVT_MENU(ID_AddMonoLoop, AppFrame::on_add_loop)
	EVT_MENU(ID_AddStereoLoop, AppFrame::on_add_loop)
	EVT_MENU(ID_AddCustomLoop, AppFrame::on_add_custom_loop)
	EVT_MENU(ID_RemoveLoop, AppFrame::on_remove_loop)

	EVT_MENU(ID_PreferencesMenu, AppFrame::on_view_menu)
	EVT_MENU(ID_KeybindingsMenu, AppFrame::on_view_menu)
	EVT_MENU(ID_MidiBindingsMenu, AppFrame::on_view_menu)
	EVT_MENU(ID_ConnectionMenu, AppFrame::on_view_menu)

	EVT_MENU(ID_LoadSession, AppFrame::on_load_session)
	EVT_MENU(ID_SaveSession, AppFrame::on_save_session)
	EVT_MENU(ID_SaveSessionAudio, AppFrame::on_save_session_audio)

	EVT_MENU(ID_StayOnTop, AppFrame::on_view_menu)
	
END_EVENT_TABLE()

AppFrame::AppFrame(const wxString& title, const wxPoint& pos, const wxSize& size, bool stay_on_top, bool embedded)
: wxFrame((wxFrame *)NULL, -1, title, pos, size,
          //          embedded ? (stay_on_top ? wxSTAY_ON_TOP : 0) : (wxDEFAULT_FRAME_STYLE | (stay_on_top ? wxSTAY_ON_TOP : 0))
                    embedded ? (wxRESIZE_BORDER | wxCAPTION | (stay_on_top ? wxSTAY_ON_TOP : 0)) : (wxDEFAULT_FRAME_STYLE | (stay_on_top ? wxSTAY_ON_TOP : 0))
         //  (wxDEFAULT_FRAME_STYLE | (stay_on_top ? wxSTAY_ON_TOP : 0))
          , wxT("sooperlooper")), _embedded(embedded)

{
	_prefs_dialog = 0;
	_help_window = 0;
	_toolbar = 0;

#ifdef __WXMAC__
    if (!_embedded) {
        wxApp::s_macAboutMenuItemId = ID_AboutMenu;
        wxApp::s_macPreferencesMenuItemId = ID_PreferencesMenu;
        wxApp::s_macExitMenuItemId = ID_QuitStop;
        wxApp::s_macHelpMenuTitleName = wxT("&Help");
    }
#endif

	init();
}

AppFrame::~AppFrame()
{
	get_main_panel()->set_default_position(GetScreenPosition());
                
	_mainpanel->save_rc();
    
    //wxLogWarning(wxT("appframe destroy"));

    
}

void
AppFrame::init()
{
	_topsizer = new wxBoxSizer(wxVERTICAL);

	SetBackgroundColour(*wxBLACK);
	SetThemeEnabled(false);
	
	wxFont sliderFont = *wxSMALL_FONT;

	_mainpanel = new MainPanel(this, -1, wxDefaultPosition, wxDefaultSize);

	_mainpanel->set_force_local(_embedded);
	
    if (!_embedded) {
	
        wxMenuBar *menuBar = new wxMenuBar();
        
        wxMenu *menuFile = new wxMenu(wxT(""));
        
        menuFile->Append(ID_LoadSession, wxT("Load Session\tCtrl-L"), wxT("Load session"));
        menuFile->Append(ID_SaveSession, wxT("Save Session\tCtrl-E"), wxT("Save session"));
        menuFile->Append(ID_SaveSessionAudio, wxT("Save Session with Audio\tCtrl-Shift-P"), wxT("Save session with Audio"));
        
        menuFile->AppendSeparator();
        
        //menuFile->Append(ID_AddLoop, wxT("Add Default Loop"), wxT("Add one default loop"));
        menuFile->Append(ID_AddMonoLoop, wxT("Add Mono Loop\tCtrl-1"), wxT("Add one default mono loop"));
        menuFile->Append(ID_AddStereoLoop, wxT("Add Stereo Loop\tCtrl-2"), wxT("Add one default stereo loop"));
        menuFile->Append(ID_AddCustomLoop, wxT("Add Custom Loop(s)...\tCtrl-A"), wxT("Add one or more custom loops, where loop memory can be specified"));
        menuFile->Append(ID_RemoveLoop, wxT("Remove Last Loop\tCtrl-D"), wxT("Remove last loop"));
        
        menuFile->AppendSeparator();

        menuFile->Append(ID_PreferencesMenu, wxT("&Preferences...\tCtrl-,"), wxT("Preferences..."));
        
#ifndef __WXMAC__
        menuFile->AppendSeparator();
#endif
        
        menuFile->Append(ID_Quit, wxT("Quit but Leave Engine Running\tCtrl-Alt-Q"), wxT("Exit from GUI and leave engine running"));
        menuFile->Append(ID_QuitStop, wxT("Quit and Stop Engine\tCtrl-Q"), wxT("Exit from GUI and stop engine"));
        
        menuBar->Append(menuFile, wxT("&Session"));
        

        //wxMenu *menuView = new wxMenu(wxT(""));
        //menuBar->Append(menuView, wxT("&View"));
        
        
        wxMenu *menuHelp = new wxMenu(wxT(""));
        menuHelp->Append(ID_HelpTipsMenu, wxT("&Usage Tips..."), wxT("Show Usage Tips window"));
        menuHelp->Append(ID_AboutMenu, wxT("&About..."), wxT("Show about dialog"));
        menuBar->Append(menuHelp, wxT("&Help"));
        
        // ... and attach this menu bar to the frame
        SetMenuBar(menuBar);
    }
    else {
        int xSize = 8;
        int ySize = 8;
        wxImage * image = new wxImage(xSize, ySize);
        if (!image->HasAlpha()){
            image->SetAlpha();
        }
        
        memset((void*)image->GetAlpha(), 0x0, xSize * ySize );
        wxBitmap bitmap(*image, 32);
        delete image;
        
        wxToolBar * toolbar = new wxToolBar(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 24), wxTB_TEXT/*|wxTB_NOICONS*/);
        toolbar->SetToolSeparation(10);
        
        toolbar->AddTool(ID_AddMonoLoop, wxT("Add Mono"), bitmap, wxNullBitmap, wxITEM_NORMAL, wxT("Add one default mono loop"));
        toolbar->AddTool(ID_AddStereoLoop, wxT("Add Stereo"), bitmap, wxNullBitmap, wxITEM_NORMAL, wxT("Add one default stereo loop"));
        toolbar->AddTool(ID_AddCustomLoop, wxT("Add Custom"), bitmap , wxNullBitmap, wxITEM_NORMAL, wxT("Add one or more custom loops, where loop memory can be specified"));
        toolbar->AddTool(ID_RemoveLoop, wxT("Remove Last"), bitmap, wxNullBitmap, wxITEM_NORMAL, wxT("Remove last loop"));
#if wxCHECK_VERSION(2,9,0)
        toolbar->AddStretchableSpace();
#endif
        toolbar->AddTool(ID_LoadSession, wxT("Load"), bitmap, wxNullBitmap, wxITEM_NORMAL, wxT("Load session..."));
        toolbar->AddTool(ID_SaveSession, wxT("Save"), bitmap, wxNullBitmap, wxITEM_NORMAL, wxT("Save session..."));
        toolbar->AddSeparator();
        toolbar->AddTool(ID_PreferencesMenu, wxT("Prefs"), bitmap, wxNullBitmap, wxITEM_NORMAL, wxT("Preferences..."));
        toolbar->Realize();
        //Connect(ID_AddMonoLoop, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler(AppFrame::on_add_loop));
        
        toolbar->SetSize(0, 0, -1, 24);
        _toolbar = toolbar;
        _topsizer->Add(toolbar, 0, wxEXPAND);
    }


    _topsizer->Add (_mainpanel, 1, wxEXPAND);
    
    SetSizerAndFit(_topsizer);
}


void
AppFrame::OnActivate(wxActivateEvent &ev)
{
	if (ev.GetActive()) {
		_mainpanel->get_keyboard().set_enabled (true);
	}
	else {
		_mainpanel->get_keyboard().set_enabled (false);
	}

	ev.Skip();
	
}

void
AppFrame::OnClose(wxCloseEvent &event)
{
	// send quit command to looper if we spawned this engine

	_mainpanel->do_close(_mainpanel->get_loop_control().we_spawned());

	Destroy();
}

void
AppFrame::OnQuit(wxCommandEvent& event)
{
	int id = event.GetId();

	if (id == ID_Quit) {
		_mainpanel->do_close(false);
		Destroy();
	}
	else if (id == ID_QuitStop) {
		// send quit command to looper
		_mainpanel->do_close(true);
		Destroy();
	}
}

void
AppFrame::on_add_loop (wxCommandEvent &ev)
{
	int id = ev.GetId();
	
	if (id == ID_AddLoop) {
		_mainpanel->do_add_loop ();
	}
	else if (id == ID_AddMonoLoop) {
		_mainpanel->do_add_loop ("mono");
	}
	else if (id == ID_AddStereoLoop) {
		_mainpanel->do_add_loop ("stereo");
	}
}

void
AppFrame::on_add_custom_loop (wxCommandEvent &ev)
{
	_mainpanel->do_add_custom_loop();
}

void
AppFrame::on_remove_loop (wxCommandEvent &ev)
{
	_mainpanel->do_remove_loop();
}


void AppFrame::on_view_menu (wxCommandEvent &ev)
{
	if (ev.GetId() == ID_PreferencesMenu) {
		if (!_prefs_dialog) {
			_prefs_dialog = new PrefsDialog(_mainpanel, -1, wxT("SooperLooper Preferences"), wxDefaultPosition, wxSize(230, 410));
			//_prefs_dialog->SetSize (230,410);
		}
		else if (!_prefs_dialog->IsShown()) {
			_prefs_dialog->refresh_state();
		}

		_prefs_dialog->Show(true);
		_prefs_dialog->Raise();
		
	}
}

void AppFrame::on_about (wxCommandEvent &ev)
{
	// construct splash
	wxBitmap bitmap(sl_splash_xpm);
	// add version info

    {
        wxMemoryDC mdc;
        mdc.SelectObject(bitmap);
        int w,h;
        wxString vstr = wxString::Format(wxT("v %s"), wxString::FromAscii(sooperlooper_version).c_str());
        //	mdc.SetFont(*wxSWISS_FONT);
        mdc.SetTextForeground(*wxWHITE);
        mdc.SetTextBackground(*wxBLACK);
        mdc.GetTextExtent(vstr, &w, &h);
        mdc.DrawText(vstr, bitmap.GetWidth() / 2 - (w/2), 148);
    }
	
	wxSplashScreen* splash = new wxSplashScreen(bitmap,
						    wxSPLASH_CENTRE_ON_PARENT|wxSPLASH_NO_TIMEOUT,
						    6000, this, -1, wxDefaultPosition, wxDefaultSize,
						    wxSTAY_ON_TOP);
	splash->SetTitle(wxT("About SooperLooper"));
	
}


void AppFrame::on_help (wxCommandEvent &ev)
{
	// for now just refer to website
	if (!_help_window) {
		_help_window = new HelpWindow(this, -1, wxT("Online Help"));
	}
	
	_help_window->Show(true);
	_help_window->Raise();

}


void AppFrame::on_load_session (wxCommandEvent &ev)
{
	_mainpanel->do_load_session();
}

void AppFrame::on_save_session (wxCommandEvent &ev)
{
	_mainpanel->do_save_session();
}

void AppFrame::on_save_session_audio (wxCommandEvent &ev)
{
	_mainpanel->do_save_session(true);
}

