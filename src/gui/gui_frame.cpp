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

#include "gui_frame.hpp"
#include "gui_app.hpp"



using namespace SooperLooperGui;
using namespace std;


BEGIN_EVENT_TABLE(GuiFrame, wxFrame)

	EVT_IDLE(GuiFrame::OnIdle)
	EVT_SIZE(GuiFrame::OnSize)
	EVT_PAINT(GuiFrame::OnPaint)
	
END_EVENT_TABLE()

GuiFrame::GuiFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
	: wxFrame((wxFrame *)NULL, -1, title, pos, size, wxDEFAULT_FRAME_STYLE, "sooperlooper")
{

	init();
}

GuiFrame::~GuiFrame()
{

}

void
GuiFrame::init()
{
	wxBoxSizer * mainSizer = new wxBoxSizer(wxVERTICAL);

	wxBoxSizer * rowsizer = new wxBoxSizer(wxHORIZONTAL);


// 	wxBitmapButton * bitbutt = new wxBitmapButton(this, -1, wxBitmap(undo_normal_xpm), wxDefaultPosition, wxDefaultSize, 0);
// 	bitbutt->SetBitmapSelected (wxBitmap(undo_selected_xpm));
// 	bitbutt->SetBitmapFocus (wxBitmap(undo_focus_xpm));
// 	bitbutt->SetBitmapDisabled (wxBitmap(undo_disabled_xpm));

// 	bitbutt->SetThemeEnabled(false);
	
// 	rowsizer->Add (bitbutt, 0);

	mainSizer->Add (rowsizer, 0, wxEXPAND|wxALL, 5);
	
	
	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( mainSizer );      // actually set the sizer
	mainSizer->Fit( this );            // set size to minimum size as calculated by the sizer
	mainSizer->SetSizeHints( this );   // set size hints to honour mininum size
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
