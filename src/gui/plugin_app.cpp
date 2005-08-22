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


#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <signal.h>
#include <pthread.h>

#include <cstdio>
#include <iostream>
#include <cstring>

#ifdef __WXMAC__
#include <wx/filename.h>
wxString GetExecutablePath(wxString argv0);
#endif

//#include <wx/wx.h>

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWindows headers
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include <wx/cmdline.h>

#include "version.h"

#include "plugin_app.hpp"
#include "main_panel.hpp"
#include "loop_control.hpp"

using namespace SooperLooperGui;
using namespace std;

// Create a new application object: this macro will allow wxWindows to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also declares the accessor function
// wxGetApp() which will return the reference of the right type (i.e. MyApp and
// not wxApp)
IMPLEMENT_APP_NO_MAIN(SooperLooperGui::PluginApp)


BEGIN_EVENT_TABLE(SooperLooperGui::PluginApp, wxApp)
   EVT_KEY_DOWN (PluginApp::process_key_event)
   EVT_KEY_UP (PluginApp::process_key_event)

END_EVENT_TABLE()

	
// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

#define DEFAULT_OSC_PORT 9951
#define DEFAULT_LOOP_TIME 40.0f


static const wxCmdLineEntryDesc cmdLineDesc[] =
{
	{ wxCMD_LINE_SWITCH, wxT("h"), wxT("help"), wxT("show this help"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
	{ wxCMD_LINE_SWITCH, wxT("V"), wxT("version"), wxT("show version only"), wxCMD_LINE_VAL_NONE },
	{ wxCMD_LINE_OPTION, wxT("l"), wxT("loopcount"), wxT("number of loopers to create (default is 1)"), wxCMD_LINE_VAL_NUMBER },
	{ wxCMD_LINE_OPTION, wxT("c"), wxT("channels"), wxT("channel count for each looper (default is 2)"), wxCMD_LINE_VAL_NUMBER },
	{ wxCMD_LINE_OPTION, wxT("t"), wxT("looptime"), wxT("number of seconds of loop memory per channel"), wxCMD_LINE_VAL_NUMBER },
	{ wxCMD_LINE_OPTION, wxT("H"), wxT("connect-host"), wxT("connect to sooperlooper engine on given host (default is localhost)")},
	{ wxCMD_LINE_OPTION, wxT("P"), wxT("connect-port"), wxT("connect to sooperlooper engine on given port (default is 9951)"), wxCMD_LINE_VAL_NUMBER },
	{ wxCMD_LINE_OPTION, wxT("m"), wxT("load-midi-binding"), wxT("loads midi binding from file")},
	{ wxCMD_LINE_SWITCH, wxT("s"), wxT("force-spawn"), wxT("force the execution of a new engine")},
	{ wxCMD_LINE_SWITCH, wxT("N"), wxT("never-spawn"), wxT("never start a new engine"), wxCMD_LINE_VAL_NONE },
	{ wxCMD_LINE_OPTION, wxT("E"), wxT("exec-name"), wxT("use name as binary to execute as sooperlooper engine (default is sooperlooper)")},
	{ wxCMD_LINE_OPTION, wxT("J"), wxT("jack-name"), wxT("jack client name, default is sooperlooper_1")},
	{ wxCMD_LINE_OPTION, wxT("S"), wxT("jack-server-name"), wxT("specify JACK server name")},
	{ wxCMD_LINE_NONE }
};
	

bool
PluginApp::parse_options (int argc, wxChar **argv)
{
	wxCmdLineParser parser(argc, argv);
	parser.SetDesc(cmdLineDesc);

	wxString logotext = wxT("SooperLooper ") +
		wxString::FromAscii (sooperlooper_version) +
		wxT("\nCopyright 2005 Jesse Chappell\n")
		wxT("SooperLooper comes with ABSOLUTELY NO WARRANTY\n")
		wxT("This is free software, and you are welcome to redistribute it\n")
		wxT("under certain conditions; see the file COPYING for details\n");

	parser.SetLogo (logotext);


	int ret = parser.Parse();

	if (ret != 0) {
		// help or error
		return false;
	}

	wxString strval;
	long longval;

	if (parser.Found (wxT("V"))) {
		cerr << logotext << endl;
		return false;
	}
	
	if (parser.Found (wxT("c"), &longval)) {
		if (longval < 1) {
			fprintf(stderr, "Error: channel count must be > 0\n");
			parser.Usage();
			return false;
		}
		_channels = longval;
	}
	if (parser.Found (wxT("l"), &longval)) {
		if (longval < 0) {
			fprintf(stderr, "Error: loop count must be >= 0\n");
			parser.Usage();
			return false;
		}
		_loop_count = longval;
	}
	if (parser.Found (wxT("t"), &longval)) {
		if (longval < 1) {
			fprintf(stderr, "Error: loop memory must be > 0\n");
			parser.Usage();
			return false;
		}
		_mem_secs = (float) longval;
	}

	parser.Found (wxT("H"), &_host);

	if (parser.Found (wxT("P"), &longval)) {
		_port = longval;
	}

	parser.Found (wxT("m"), &_midi_bind_file);

	_force_spawn = parser.Found (wxT("s"));
	_never_spawn = parser.Found (wxT("N"));
	parser.Found (wxT("E"), &_exec_name);

	parser.Found (wxT("S"), &_server_name);
	parser.Found (wxT("J"), &_client_name);

	return true;
}
	
	
PluginApp::PluginApp()
	: _mainpanel(0), _host(wxT("")), _port(0)
{
	_show_usage = 0;
	_show_version = 0;
	_exec_name = wxT("");
	_force_spawn = false;
	_loop_count = 0;
	_channels = 0;
	_mem_secs = 0.0f;
}

PluginApp::~PluginApp()
{
	cerr << "DESTURCT" << endl;
}




// `Main program' equivalent: the program execution "starts" here
bool PluginApp::OnInit()
{

	
	wxString jackname;
	wxString preset;
	wxString rcdir;
	wxString jackdir;
	
	SetExitOnFrameDelete(TRUE);

	
	// use stderr as log
	wxLog *logger=new wxLogStderr();
	logger->SetTimestamp(NULL);
	wxLog::SetActiveTarget(logger);
	
	
	if (!parse_options(argc, argv)) {
		// do not continue
		return FALSE;
	}

	cerr << "INITING APP" << endl;

	// Create the main application window
	_frame =  new wxFrame(NULL, -1, "blah");
	wxBoxSizer * sizer = new wxBoxSizer(wxVERTICAL);
	_mainpanel = new MainPanel(_frame, -1, wxDefaultPosition, wxDefaultSize);

	sizer->Add (_mainpanel, 1, wxEXPAND);
	
	_frame->SetSizer(sizer);
	_frame->SetAutoLayout(true);
	sizer->Fit(_frame);
	sizer->SetSizeHints(_frame);
	
	_frame->SetSize(800,215);
	_frame->Show();

	

		// override defaults
	LoopControl & loopctrl = _mainpanel->get_loop_control();
	
	loopctrl.get_spawn_config().never_spawn = true;
	loopctrl.get_spawn_config().force_spawn = false;
		
	if (!_host.empty()) {
		loopctrl.get_spawn_config().host = _host.ToAscii();
	}
	if (_port != 0) {
		loopctrl.get_spawn_config().port = _port;
	}
	if (!_midi_bind_file.empty()) {
		loopctrl.get_spawn_config().midi_bind_path = _midi_bind_file.ToAscii();
	}
	if (_loop_count != 0) {
		loopctrl.get_spawn_config().num_loops = _loop_count;
	}
	if (_channels != 0) {
		loopctrl.get_spawn_config().num_channels = _channels;
	}
	if (_mem_secs != 0) {
		loopctrl.get_spawn_config().mem_secs = _mem_secs;
	}
	
	
	// connect
	loopctrl.connect();

	// success: wxApp::OnRun() will be called which will enter the main message
	// loop and the application will run. If we returned FALSE here, the
	// application would exit immediately.
	return TRUE;
}

int PluginApp::OnRun()
{
	// normall this calls the mainloop
	// were not
	cerr << "Run called" << endl;
	//while (m_keepGoing)
    //{
    //    MacDoOneEvent() ;
    //}
	
	MainLoop();
	cerr << "Run done" << endl;
	return 0;
}

void
PluginApp::process_key_event (wxKeyEvent &ev)
{
	// this recieves all key events first

	_mainpanel->process_key_event (ev);
}

