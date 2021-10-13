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

#include "gui_app.hpp"
#include "app_frame.hpp"
#include "main_panel.hpp"
#include "loop_control.hpp"

using namespace SooperLooperGui;
using namespace std;

// Create a new application object: this macro will allow wxWindows to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also declares the accessor function
// wxGetApp() which will return the reference of the right type (i.e. MyApp and
// not wxApp)
IMPLEMENT_APP(SooperLooperGui::GuiApp)


BEGIN_EVENT_TABLE(SooperLooperGui::GuiApp, wxApp)
   EVT_KEY_DOWN (GuiApp::process_key_event)
   EVT_KEY_UP (GuiApp::process_key_event)

END_EVENT_TABLE()

	
// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

#define DEFAULT_OSC_PORT 9951
#define DEFAULT_LOOP_TIME 200.0f


static const wxCmdLineEntryDesc cmdLineDesc[] =
{
	{ wxCMD_LINE_SWITCH, wxT_2("h"), wxT_2("help"), wxT_2("show this help"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
	{ wxCMD_LINE_SWITCH, wxT_2("V"), wxT_2("version"), wxT_2("show version only"), wxCMD_LINE_VAL_NONE },
	{ wxCMD_LINE_OPTION, wxT_2("l"), wxT_2("loopcount"), wxT_2("number of loopers to create (default is 1)"), wxCMD_LINE_VAL_NUMBER },
	{ wxCMD_LINE_OPTION, wxT_2("c"), wxT_2("channels"), wxT_2("channel count for each looper (default is 2)"), wxCMD_LINE_VAL_NUMBER },
	{ wxCMD_LINE_OPTION, wxT_2("t"), wxT_2("looptime"), wxT_2("number of seconds of loop memory per channel"), wxCMD_LINE_VAL_NUMBER },
	{ wxCMD_LINE_OPTION, wxT_2("H"), wxT_2("connect-host"), wxT_2("connect to sooperlooper engine on given host (default is localhost)")},
	{ wxCMD_LINE_OPTION, wxT_2("P"), wxT_2("connect-port"), wxT_2("connect to sooperlooper engine on given port (default is 9951)"), wxCMD_LINE_VAL_NUMBER },
	{ wxCMD_LINE_OPTION, wxT_2("m"), wxT_2("load-midi-binding"), wxT_2("loads midi binding from file")},
	{ wxCMD_LINE_OPTION, wxT_2("L"), wxT_2("load-session"), wxT_2("load session from file")},
	{ wxCMD_LINE_SWITCH, wxT_2("s"), wxT_2("force-spawn"), wxT_2("force the execution of a new engine")},
	{ wxCMD_LINE_SWITCH, wxT_2("N"), wxT_2("never-spawn"), wxT_2("never start a new engine"), wxCMD_LINE_VAL_NONE },
	{ wxCMD_LINE_OPTION, wxT_2("E"), wxT_2("exec-name"), wxT_2("use name as binary to execute as sooperlooper engine (default is sooperlooper)")},
	{ wxCMD_LINE_OPTION, wxT_2("J"), wxT_2("jack-name"), wxT_2("jack client name, default is sooperlooper_1")},
	{ wxCMD_LINE_OPTION, wxT_2("S"), wxT_2("jack-server-name"), wxT_2("specify JACK server name")},
	{ wxCMD_LINE_SWITCH, wxT_2("T"), wxT_2("stay-on-top"), wxT_2("keep main window on top of other applications")},
	{ wxCMD_LINE_OPTION, wxT_2("X"), wxT_2("screen-location"), wxT_2("location on screen to place the window, use format <xpos>:<ypos> (default 100:100)") },
	{ wxCMD_LINE_SWITCH, wxT_2("n"), wxT_2("never-timeout"), wxT_2("Never timeout if an engine stops responding")},
	{ wxCMD_LINE_NONE }
};
	

bool
GuiApp::parse_options (int argc, wxChar **argv)
{
	wxCmdLineParser parser(argc, argv);
	parser.SetDesc(cmdLineDesc);

	wxString logotext = wxT("SooperLooper ") +
		wxString::FromAscii (sooperlooper_version) +
		wxT("\nCopyright 2008 Jesse Chappell\n")
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
		fprintf(stderr, "%s\n", (const char *) logotext.ToAscii());
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
	parser.Found (wxT("L"), &_load_session);

	_force_spawn = parser.Found (wxT("s"));
	_never_spawn = parser.Found (wxT("N"));
	parser.Found (wxT("E"), &_exec_name);

	_never_timeout = parser.Found (wxT("n"));

	parser.Found (wxT("S"), &_server_name);
	parser.Found (wxT("J"), &_client_name);

	_stay_on_top = parser.Found (wxT("T"));

        wxString posstr;
	if (parser.Found (wxT("X"), &posstr)) {
            // parse X:Y
            wxArrayString strarr = wxSplit(posstr, wxChar(':'));
            if (strarr.size() == 2) {
                long x = 100, y = 100;
                strarr[0].ToLong(&x);
                strarr[1].ToLong(&y);
                _screen_pos = wxPoint(x,y);
                _override_screenpos = true;
            }
        }

        
	return true;
}
	
	
GuiApp::GuiApp()
	: _frame(0),
          //_host(wxT("localhost")),
          _host(wxT("127.0.0.1")),
          _port(0)
{
	_show_usage = 0;
	_show_version = 0;
	_exec_name = wxT("");
	_force_spawn = false;
	_loop_count = 0;
	_channels = 0;
	_mem_secs = 0.0f;
	_stay_on_top = false;
        _override_screenpos = false;
        _screen_pos = wxPoint(100, 100);
	_never_timeout = false;
	_inited = false;
}

GuiApp::~GuiApp()
{
}


void GuiApp::MacOpenFile(const wxString &fileName)
{
	//cerr << "OPEN SESSION: " << (const char *) fileName.ToAscii() << endl;
	_load_session = fileName;
	
	if (_frame) {
		LoopControl & loopctrl = _frame->get_main_panel()->get_loop_control();
		loopctrl.get_spawn_config().session_path = (const char *) _load_session.ToAscii();
		if (_inited) {
			// load session now
			//cerr << "loading now" << endl;
			LoopControl & loopctrl = _frame->get_main_panel()->get_loop_control();
			loopctrl.post_load_session (_load_session);
		}
	}
}

// `Main program' equivalent: the program execution "starts" here
bool GuiApp::OnInit()
{
	wxString jackname;
	wxString preset;
	wxString rcdir;
	wxString jackdir;
	
	SetExitOnFrameDelete(TRUE);

	
	// use stderr as log
	wxLog *logger=new wxLogStderr();
	logger->SetTimestamp(wxT(""));
	wxLog::SetActiveTarget(logger);
	
	
	if (!parse_options(argc, argv)) {
		// do not continue on non macs
#ifndef __WXMAC__
		return FALSE;
#endif
	}

#if wxCHECK_VERSION(2,9,0)
    SetAppDisplayName(wxT("SooperLooper"));
#endif

	// Create the main application window
	_frame = new AppFrame (wxString::Format(wxT("SooperLooper v %s"), wxString::FromAscii(sooperlooper_version).c_str()), _screen_pos, wxDefaultSize, _stay_on_top);

#ifdef __WXMAC__
	if (_exec_name.empty()) {
		_exec_name = GetExecutablePath(argv[0]) + wxT("sooperlooper");
	}
#endif
	// escape all spaces with
	_exec_name.Replace (wxT(" "), wxT("\\ "), true);
	
	
	// override defaults
	LoopControl & loopctrl = _frame->get_main_panel()->get_loop_control();
	if (!_host.empty()) {
		loopctrl.get_spawn_config().host = _host.ToAscii();
	}
	if (_port != 0) {
		loopctrl.get_spawn_config().port = _port;
	}
	if (_force_spawn) {
		loopctrl.get_spawn_config().force_spawn = _force_spawn;
	}
	if (_never_spawn) {
		loopctrl.get_spawn_config().never_spawn = _never_spawn;
	}
	if (!_client_name.empty()) {
		loopctrl.get_spawn_config().jack_name = _client_name.ToAscii();
	}
	if (!_server_name.empty()) {
		loopctrl.get_spawn_config().jack_serv_name = _server_name.ToAscii();
	}
	if (!_exec_name.empty()) {
		loopctrl.get_spawn_config().exec_name = _exec_name.ToAscii();
	}
	if (!_midi_bind_file.empty()) {
		loopctrl.get_spawn_config().midi_bind_path = _midi_bind_file.ToAscii();
	}
	if (!_load_session.empty()) {
		loopctrl.get_spawn_config().session_path = (const char *) _load_session.ToAscii();
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
        if (_override_screenpos) {
                _frame->get_main_panel()->set_default_position(_screen_pos);
        }
        else {
                _screen_pos = _frame->get_main_panel()->get_default_position();
        }
        
	loopctrl.get_spawn_config().never_timeout = _never_timeout;
	_frame->get_main_panel()->set_never_timeout(_never_timeout);
	
    
    if (_host == wxT("127.0.0.1") && _never_spawn) {
        // special case, force the mainpanel to be local, to avoid some weirdness with network configs
        _frame->get_main_panel()->set_force_local(true);
    }
    
	//cerr << "OnInit" << endl;
	// connect
	//loopctrl.connect();

	// Show it with minimum size and tell the application that it's our main window
	_frame->SetSize(_screen_pos.x, _screen_pos.y, _frame->GetMinWidth(), _frame->GetMinHeight());
	SetTopWindow(_frame);
	_frame->Show(FALSE);
	_frame->Raise();
	_frame->Show(TRUE);

	// success: wxApp::OnRun() will be called which will enter the main message
	// loop and the application will run. If we returned FALSE here, the
	// application would exit immediately.


	return TRUE;
}

int GuiApp::OnRun()
{
	LoopControl & loopctrl = _frame->get_main_panel()->get_loop_control();

	loopctrl.connect();
	_inited = true;

	return wxApp::OnRun();
}

void
GuiApp::process_key_event (wxKeyEvent &ev)
{
	// this recieves all key events first

	_frame->get_main_panel()->process_key_event (ev);
}

#ifdef __WXMAC__
wxString
GetExecutablePath(wxString argv0)
{
    wxString path;

    if (wxIsAbsolutePath(argv0))
        path = argv0;
    else {
        wxPathList pathlist;
        pathlist.AddEnvList(wxT("PATH"));
        path = pathlist.FindAbsoluteValidPath(argv0);
    }

    wxFileName filename(path);
    filename.Normalize();

    path = filename.GetFullPath();
    path = path.BeforeLast('/') + wxT("/");

    return path;
}
#endif // __WXMAC__

