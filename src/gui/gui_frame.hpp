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

#ifndef __sooperlooper_gui_frame__
#define __sooperlooper_gui_frame__


#include <wx/wx.h>

#include <string>
#include <vector>

#include <sigc++/object.h>
#include <sigc++/signal.h>

namespace SooperLooperGui {

class LoopControl;
class LooperPanel;
	
class GuiFrame
	: public wxFrame,  public SigC::Object
{
public:
	
	// ctor(s)
	GuiFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	virtual ~GuiFrame();
	
	// event handlers (these functions should _not_ be virtual)
	void OnQuit(wxCommandEvent& event);
	void OnHide(wxCommandEvent &event);
	
	void OnSize(wxSizeEvent & event);
	void OnPaint(wxPaintEvent & event);
	
	void OnIdle(wxIdleEvent& event);
	void OnUpdateTimer(wxTimerEvent &ev);

protected:

	void init();

	void init_loopers (int count);

	
	LoopControl * _loop_control;

	std::vector<LooperPanel *> _looper_panels;
	
	wxTimer * _update_timer;
	wxBoxSizer * _main_sizer;
	
private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};

};

#endif
