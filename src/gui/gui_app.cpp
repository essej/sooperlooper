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

//#include <wx/wx.h>

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWindows headers
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "version.h"

#include "gui_app.hpp"
#include "gui_frame.hpp"


using namespace SooperLooperGui;
using namespace std;

// Create a new application object: this macro will allow wxWindows to create
// the application object during program execution (it's better than using a
// static object for many reasons) and also declares the accessor function
// wxGetApp() which will return the reference of the right type (i.e. MyApp and
// not wxApp)
IMPLEMENT_APP(SooperLooperGui::GuiApp)

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// the application class
// ----------------------------------------------------------------------------

	
GuiApp::GuiApp()
	: _frame(0), _host(wxT("")), _port(9951)
{
}


static void* watchdog_thread(void* arg)
{
  sigset_t signalset;
  //struct ecasound_state* state = reinterpret_cast<struct ecasound_state*>(arg);
  int signalno;
  bool exiting = false;
  
  /* register cleanup routine */
  //atexit(&ecasound_atexit_cleanup);

  // cerr << "Watchdog-thread created, pid=" << getpid() << "." << endl;

  while (!exiting)
  {
	  sigemptyset(&signalset);
	  
	  /* handle the following signals explicitly */
	  sigaddset(&signalset, SIGTERM);
	  sigaddset(&signalset, SIGINT);
	  sigaddset(&signalset, SIGHUP);
	  sigaddset(&signalset, SIGPIPE);
	  
	  /* block until a signal received */
	  sigwait(&signalset, &signalno);
	  
	  //cerr << endl << "freqtweak: watchdog-thread received signal " << signalno << ". Cleaning up..." << endl;

	  if (signalno == SIGHUP) {
		  // reinit iosupport
// 		  cerr << "freqtweak got SIGHUP... reiniting" << endl;
// 		  wxThread::Sleep(200);

// 		  FTioSupport * iosup = FTioSupport::instance();
// 		  if (!iosup->isInited()) {
// 			  iosup->init();
// 			  if (iosup->startProcessing()) {
// 				  iosup->reinit();
// 			  }
// 		  }

// 		  if (::wxGetApp().getMainwin()) {
// 			  ::wxGetApp().getMainwin()->updateDisplay();
// 		  }
		  exiting = true;
	  }
	  else {
		  exiting = true;
	  }
  }

  ::wxGetApp().getFrame()->Close(TRUE);
  
  ::wxGetApp().ExitMainLoop();
  // printf ("bye bye, hope you had fun...\n");

  /* to keep the compilers happy; never actually executed */
  return(0);
}



/**
 * Sets up a signal mask with sigaction() that blocks 
 * all common signals, and then launces an watchdog
 * thread that waits on the blocked signals using
 * sigwait().
 */
void GuiApp::setupSignals()
{
  pthread_t watchdog;

  /* man pthread_sigmask:
   *  "...signal actions and signal handlers, as set with
   *   sigaction(2), are shared between all threads"
   */

  struct sigaction blockaction;
  blockaction.sa_handler = SIG_IGN;
  sigemptyset(&blockaction.sa_mask);
  blockaction.sa_flags = 0;

  /* ignore the following signals */
  sigaction(SIGTERM, &blockaction, 0);
  sigaction(SIGINT, &blockaction, 0);
  sigaction(SIGHUP, &blockaction, 0);
  sigaction(SIGPIPE, &blockaction, 0);

  int res = pthread_create(&watchdog, 
			   NULL, 
			   watchdog_thread, 
			   NULL);
  if (res != 0) {
    cerr << "freqtweak: Warning! Unable to create watchdog thread." << endl;
  }
}



// `Main program' equivalent: the program execution "starts" here
bool GuiApp::OnInit()
{

// 	signal (SIGTERM, onTerminate);
// 	signal (SIGINT, onTerminate);

// 	signal (SIGHUP, onHangup);

	
	wxString jackname;
	wxString preset;
	wxString rcdir;
	wxString jackdir;
	
	SetExitOnFrameDelete(TRUE);

	setupSignals();

	
	// use stderr as log
	wxLog *logger=new wxLogStderr();
	logger->SetTimestamp(NULL);
	wxLog::SetActiveTarget(logger);
	

	
	// Create the main application window
	_frame = new GuiFrame (wxT("SooperLooper"), wxPoint(100, 100), wxDefaultSize);

	
	// Show it and tell the application that it's our main window
	//_frame->SetSize(669,300);
	_frame->Show(TRUE);

	SetTopWindow(_frame);
	
		
	// success: wxApp::OnRun() will be called which will enter the main message
	// loop and the application will run. If we returned FALSE here, the
	// application would exit immediately.
	return TRUE;
}



