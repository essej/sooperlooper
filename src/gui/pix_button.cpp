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

#include "pix_button.hpp"



using namespace SooperLooperGui;
using namespace std;


BEGIN_EVENT_TABLE(PixButton, wxWindow)

	EVT_SIZE(PixButton::OnSize)
	EVT_PAINT(PixButton::OnPaint)
	EVT_MOUSE_EVENTS(PixButton::OnMouseEvents)
	
END_EVENT_TABLE()

PixButton::PixButton(wxWindow * parent, wxWindowID id, const wxPoint& pos, const wxSize& size)
	: wxWindow(parent, id, pos, size)
{
	_bgcolor = *wxBLACK;
	_bgbrush.SetColour (_bgcolor);
	_bstate = Normal;
	_estate = Outside;
	SetBackgroundColour (_bgcolor);
	SetThemeEnabled(false);
}

PixButton::~PixButton()
{

}

void PixButton::set_normal_bitmap (const wxBitmap & bm)
{
	if (!bm.Ok()) return;

	_normal_bitmap = bm;
	SetSizeHints (bm.GetWidth(), bm.GetHeight());
	SetClientSize (bm.GetWidth(), bm.GetHeight());
	Refresh();
}

void PixButton::set_focus_bitmap (const wxBitmap & bm)
{
	if (!bm.Ok()) return;

	_focus_bitmap = bm;
	SetSizeHints (bm.GetWidth(), bm.GetHeight());
	SetClientSize (bm.GetWidth(), bm.GetHeight());
	Refresh();
}

void PixButton::set_selected_bitmap (const wxBitmap & bm)
{
	if (!bm.Ok()) return;

	_selected_bitmap = bm;
	SetSizeHints (bm.GetWidth(), bm.GetHeight());
	SetClientSize (bm.GetWidth(), bm.GetHeight());
	Refresh();
}

void PixButton::set_disabled_bitmap (const wxBitmap & bm)
{
	if (!bm.Ok()) return;

	_disabled_bitmap = bm;
	SetSizeHints (bm.GetWidth(), bm.GetHeight());
	SetClientSize (bm.GetWidth(), bm.GetHeight());
	Refresh();
}


void PixButton::set_bg_color (const wxColour & col)
{
	_bgcolor = col;
	_bgbrush.SetColour (col);
	SetBackgroundColour (col);
	Refresh();
}

void
PixButton::OnSize(wxSizeEvent & event)
{
	GetClientSize(&_width, &_height);

	event.Skip();
}

void PixButton::OnPaint(wxPaintEvent & event)
{
	wxPaintDC pdc(this);

	
	draw_area(pdc);
}


void
PixButton::OnMouseEvents (wxMouseEvent &ev)
{
	if (!IsEnabled()) {
		ev.Skip();
		return;
	}

	if (ev.Moving()) {
		// do nothing
	}
	else if (ev.ButtonDown())
	{
		_bstate = Selected;
		pressed (); // emit
		CaptureMouse();
		Refresh();
	}
	else if (ev.ButtonUp())
	{
		_bstate = Normal;
		ReleaseMouse();
		released (); // emit
		Refresh();
	}
	else if (ev.ButtonDClick()) {
		_bstate = Selected;
		pressed (); // emit
		Refresh();
	}
	else if (ev.Entering())
	{
		_estate = Inside;
		Refresh();
	}
	else if (ev.Leaving())
	{
		_estate = Outside;
		Refresh();
	}
	
	ev.Skip();
}


void PixButton::draw_area(wxDC & dc)
{
	dc.SetBackground(_bgbrush);
	// dc.Clear();
	
	switch (_bstate) {
	case Normal:
		if (_estate == Outside) {
			dc.DrawBitmap (_normal_bitmap, 0, 0);
		}
		else {
			dc.DrawBitmap (_focus_bitmap, 0, 0);
		}
		break;

	case Selected:
		dc.DrawBitmap (_selected_bitmap, 0, 0);
		break;

	case Disabled:
		dc.DrawBitmap (_disabled_bitmap, 0, 0);
		break;
		
		
	}

}
