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
#include <cmath>

#include "check_box.hpp"



using namespace SooperLooperGui;
using namespace std;


enum {
	ID_PopupBase  = 8000
	
};

BEGIN_EVENT_TABLE(CheckBox, wxWindow)

	EVT_SIZE(CheckBox::OnSize)
	EVT_PAINT(CheckBox::OnPaint)
	EVT_MOUSE_EVENTS(CheckBox::OnMouseEvents)
	EVT_MOUSEWHEEL (CheckBox::OnMouseEvents)
	EVT_KILL_FOCUS (CheckBox::OnFocusEvent)
	
END_EVENT_TABLE()

CheckBox::CheckBox(wxWindow * parent, wxWindowID id, const wxString & label, const wxPoint& pos, const wxSize& size)
	: wxWindow(parent, id, pos, size)
{
	_value = false;
	_backing_store = 0;
	_label_str = label;
	_boxsize = 14;
	
	_bgcolor.Set(0,0,0);
	_bgbrush.SetColour (_bgcolor);
	SetBackgroundColour (_bgcolor);
	SetThemeEnabled(false);

	_valuecolor.Set(244, 255, 178);

	_textcolor = *wxWHITE;
	//_barcolor.Set(14, 50, 89);
	_barcolor.Set(20, 65, 104);
	_pressbarcolor.Set(30, 85, 144);
	_overbarcolor.Set(20, 40, 50);
	_barbrush.SetColour(_barcolor);
	
	_bgbordercolor.Set(0,0,0);
	//_bordercolor.Set(67, 83, 103);
	_bordercolor.Set(0, 0, 0);
	_borderpen.SetColour(_bordercolor);
	_borderpen.SetWidth(1);
	_borderbrush.SetColour(_bgbordercolor);

	_valuebrush.SetColour(_valuecolor);

	int w,h;
	GetTextExtent(_label_str, &w, &h);
	SetVirtualSizeHints (6 + _boxsize + w, max(_boxsize, h));
	SetVirtualSize (6 + _boxsize + w, max(_boxsize, h));
	
}

CheckBox::~CheckBox()
{

}


void
CheckBox::set_value (bool val)
{
	if (_value != val) {
		_value = val;
		Refresh(false);
	}
}

void
CheckBox::set_label (const wxString & label)
{
	int w,h;
	
	_label_str = label;

	GetTextExtent(_label_str, &w, &h);

	SetVirtualSize (6 + _boxsize + w, max(_boxsize, h));
	SetVirtualSizeHints (6 + _boxsize + w, max(_boxsize, h));
	Refresh(false);	
}


void CheckBox::set_bg_color (const wxColour & col)
{
	_bgcolor = col;
	_bgbrush.SetColour (col);
	SetBackgroundColour (col);
	Refresh(false);
}

void CheckBox::set_label_color (const wxColour & col)
{
	_textcolor = col;
	Refresh(false);
}

void CheckBox::set_border_color (const wxColour & col)
{
	_bordercolor = col;
	_borderpen.SetColour (col);
	Refresh(false);
}

void CheckBox::set_bg_border_color (const wxColour & col)
{
	_bgbordercolor = col;
	_borderbrush.SetColour (col);
	Refresh(false);
}


void
CheckBox::OnSize(wxSizeEvent & event)
{
	GetClientSize(&_width, &_height);

	if (_backing_store) {
		delete _backing_store;
	}
	_backing_store = new wxBitmap(_width, _height);

	
	event.Skip();
}

void CheckBox::OnPaint(wxPaintEvent & event)
{
	wxPaintDC pdc(this);
	wxMemoryDC dc;

	if (!_backing_store) {
		return;
	}
   
	dc.SelectObject(*_backing_store);
	
	draw_area(dc);

	pdc.Blit(0, 0, _width, _height, &dc, 0, 0);
}


void
CheckBox::OnMouseEvents (wxMouseEvent &ev)
{
	if (!IsEnabled()) {
		ev.Skip();
		return;
	}

	if (ev.Entering()) {
		_borderbrush.SetColour(_overbarcolor);
		Refresh(false);
	}
	else if (ev.Leaving()) {
		_borderbrush.SetColour(_bgbordercolor);
		_barbrush.SetColour (_barcolor);
		Refresh(false);
	}
	
	wxRect bounds (0,0, _width, _height);
	
	if (ev.GetEventType() == wxEVT_MOUSEWHEEL)
	{
		// don't get the events right now
		
	}
	else if (ev.LeftDown())
	{
		_barbrush.SetColour (_pressbarcolor);
		Refresh(false);
	}
	else if (ev.LeftUp())
	{
		if (bounds.Inside(ev.GetPosition())) {
			// toggle value
			_value = !_value;
		
			value_changed (_value); // emit
		}
		
		_barbrush.SetColour (_barcolor);
		
		Refresh(false);
	}

	ev.Skip();
}

void CheckBox::OnFocusEvent (wxFocusEvent &ev)
{
	if (ev.GetEventType() == wxEVT_KILL_FOCUS) {
		// focus kill
		_borderbrush.SetColour(_bgbordercolor);
		Refresh(false);
	}

	ev.Skip();
}



void CheckBox::draw_area(wxDC & dc)
{
	wxCoord w,h, y;
	
	dc.SetFont(GetFont());
	dc.SetBackground(_bgbrush);
	dc.Clear();

	dc.SetBrush(_borderbrush);
	dc.SetPen(_borderpen);
	dc.DrawRectangle (0, 0, _width, _height);
	
	dc.SetPen(*wxTRANSPARENT_PEN);

	y = (_height - _boxsize) / 2;
	
	// draw check square
	dc.SetBrush(_barbrush);
	dc.DrawRectangle (3, y, _boxsize, _boxsize);
	dc.SetBrush(_borderbrush);
	dc.DrawRectangle (6, y+3, _boxsize - 6, _boxsize - 6);

	if (_value) {
		dc.SetBrush(_valuebrush);
		dc.DrawRectangle (7, y + 4, _boxsize - 8, _boxsize - 8);
	}
	
	
	dc.SetTextForeground(_textcolor);
	dc.GetTextExtent(_label_str, &w, &h);
	dc.DrawText (_label_str, 6 + _boxsize, (_height - h)/2);

// 	dc.SetTextForeground(_valuecolor);
// 	dc.GetTextExtent(_value_str, &w, &h);
// 	dc.DrawText (_value_str, _width - w - 3, _height - h - 3);
	

}
