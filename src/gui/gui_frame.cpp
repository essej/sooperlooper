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

using namespace SooperLooperGui;
using namespace std;

enum {
	ID_UpdateTimer = 9000

};


BEGIN_EVENT_TABLE(GuiFrame, wxFrame)

	EVT_IDLE(GuiFrame::OnIdle)
	EVT_SIZE(GuiFrame::OnSize)
	EVT_PAINT(GuiFrame::OnPaint)
	EVT_TIMER(ID_UpdateTimer, GuiFrame::OnUpdateTimer)
	
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

}

void
GuiFrame::init()
{
	_main_sizer = new wxBoxSizer(wxVERTICAL);

	//wxBoxSizer * rowsizer = new wxBoxSizer(wxHORIZONTAL);
	
	//SetBackgroundColour(*wxBLACK);

	
	
	_loop_control = new LoopControl(::wxGetApp().get_host(), ::wxGetApp().get_port());

	// todo request how many loopers to construct based on connection
	init_loopers(1);
	
	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( _main_sizer );      // actually set the sizer
	_main_sizer->Fit( this );            // set size to minimum size as calculated by the sizer
	_main_sizer->SetSizeHints( this );   // set size hints to honour mininum size
}

void
GuiFrame::init_loopers (unsigned int count)
{
	LooperPanel * looperpan;	

	if (count > _looper_panels.size()) {
		while (count > _looper_panels.size()) {
			looperpan = new LooperPanel(_loop_control, this, -1);
			looperpan->set_index(_looper_panels.size());
			_main_sizer->Add (looperpan, 0, wxEXPAND|wxALL, 5);
			_looper_panels.push_back (looperpan);
		}
	}
	else if (count < _looper_panels.size()) {
		while (count < _looper_panels.size()) {
			looperpan = _looper_panels.back();
			_looper_panels.pop_back();
			_main_sizer->Remove(looperpan);
			looperpan->Destroy();
		}
	}
		
	_main_sizer->Layout();
	
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
	
}


void
GuiFrame::OnQuit(wxCommandEvent& event)
{

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
