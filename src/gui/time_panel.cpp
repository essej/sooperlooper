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

#include <iostream>

#include "time_panel.hpp"
#include "loop_control.hpp"

using namespace SooperLooperGui;
using namespace std;

BEGIN_EVENT_TABLE(TimePanel, wxPanel)
	EVT_PAINT(TimePanel::OnPaint)
END_EVENT_TABLE()

TimePanel::TimePanel(LoopControl * control, wxWindow * parent, wxWindowID id, const wxPoint& pos, const wxSize& size)
	: wxPanel(parent, id, pos, size), _loop_control(control), _index(0)
{
	init();
}

TimePanel::~TimePanel()
{

}

void
TimePanel::init()
{
	SetThemeEnabled(false);
	SetBackgroundColour(wxColour(34, 49, 71));

	_pos_font.SetFamily(wxSWISS);
	_pos_font.SetPointSize(14);
	_pos_font.SetStyle(wxBOLD);

	_pos_color.Set(244, 255, 158);
	
	_pos_str = "--:--.--";
	
}

bool
TimePanel::update_time()
{
	float val;
	bool ret = false;
	
	if (_loop_control->is_updated(_index, "loop_pos")) {
		_loop_control->get_value(_index, "loop_pos", val);
		// seconds
		int minutes = (int) (val / 60.0f);
		float secs = val - minutes*60.0f;
		
		_pos_str.Printf("%02d:%05.2f", minutes, secs);
		ret = true;
	}
	
	return ret;
}


void
TimePanel::OnPaint(wxPaintEvent &ev)
{
	wxPaintDC pdc(this);

	
	draw_area(pdc);
}

void
TimePanel::draw_area(wxDC & dc)
{

	dc.SetFont(_pos_font);
	dc.SetTextForeground(_pos_color);
	
	dc.DrawText (_pos_str, 5, 5);
	
}
