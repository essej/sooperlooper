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

#include "pix_button.hpp"

enum
{
	ID_EditMenuOp = 9000,
	ID_DefaultMenuOp,
	ID_BindMenuOp
};

using namespace SooperLooperGui;
using namespace std;


BEGIN_EVENT_TABLE(PixButton, wxWindow)

	EVT_SIZE(PixButton::OnSize)
	EVT_PAINT(PixButton::OnPaint)
	EVT_MOUSE_EVENTS(PixButton::OnMouseEvents)

	EVT_MENU (ID_BindMenuOp , PixButton::on_menu_events)
	
END_EVENT_TABLE()

PixButton::PixButton(wxWindow * parent, wxWindowID id, bool bindable, const wxPoint& pos, const wxSize& size)
	: wxWindow(parent, id, pos, size)
{
	_bgcolor = *wxBLACK;
	_bgbrush.SetColour (_bgcolor);
	_bstate = Normal;
	_estate = Outside;
	_backing_store = 0;
	_active = false;

	if (bindable) {
		_popup_menu = new wxMenu(wxT(""));
		//_popup_menu->Append ( new wxMenuItem(_popup_menu, ID_EditMenuOp, wxT("Edit")));
		//_popup_menu->Append ( new wxMenuItem(_popup_menu, ID_DefaultMenuOp, wxT("Set to default")));
		//_popup_menu->AppendSeparator();
		_popup_menu->Append ( new wxMenuItem(_popup_menu, ID_BindMenuOp, wxT("Learn MIDI Binding")));
	}
	else {
		_popup_menu = 0;
	}
	
	SetBackgroundColour (_bgcolor);
	SetThemeEnabled(false);

	update_size();
}

PixButton::~PixButton()
{
	_memdc.SelectObject(wxNullBitmap);
	if (_backing_store) {
		delete _backing_store;
	}
}

void PixButton::set_normal_bitmap (const wxBitmap & bm)
{
	if (!bm.Ok()) return;

	_normal_bitmap = bm;
	SetSizeHints (bm.GetWidth(), bm.GetHeight());
	SetClientSize (bm.GetWidth(), bm.GetHeight());
	Refresh(false);
}

void PixButton::set_focus_bitmap (const wxBitmap & bm)
{
	if (!bm.Ok()) return;

	_focus_bitmap = bm;
	SetSizeHints (bm.GetWidth(), bm.GetHeight());
	SetClientSize (bm.GetWidth(), bm.GetHeight());
	Refresh(false);
}

void PixButton::set_selected_bitmap (const wxBitmap & bm)
{
	if (!bm.Ok()) return;

	_selected_bitmap = bm;
	SetSizeHints (bm.GetWidth(), bm.GetHeight());
	SetClientSize (bm.GetWidth(), bm.GetHeight());
	Refresh(false);
}

void PixButton::set_disabled_bitmap (const wxBitmap & bm)
{
	if (!bm.Ok()) return;

	_disabled_bitmap = bm;
	SetSizeHints (bm.GetWidth(), bm.GetHeight());
	SetClientSize (bm.GetWidth(), bm.GetHeight());
	Refresh(false);
}

void PixButton::set_active_bitmap (const wxBitmap & bm)
{
	if (!bm.Ok()) return;

	_active_bitmap = bm;
	SetSizeHints (bm.GetWidth(), bm.GetHeight());
	SetClientSize (bm.GetWidth(), bm.GetHeight());
	Refresh(false);
}


void PixButton::set_bg_color (const wxColour & col)
{
	_bgcolor = col;
	_bgbrush.SetColour (col);
	SetBackgroundColour (col);
	Refresh(false);
}

void PixButton::set_active(bool flag)
{
	if (_active != flag) {
		_active = flag;
		Refresh(false);
	}
}

void
PixButton::update_size()
{
	GetClientSize(&_width, &_height);

	if (_width > 0 && _height > 0) {
		_memdc.SelectObject (wxNullBitmap);
		if (_backing_store) {
			delete _backing_store;
		}
		_backing_store = new wxBitmap(_width, _height);
		
		_memdc.SelectObject(*_backing_store);
	}
}

void
PixButton::OnSize(wxSizeEvent & event)
{
	update_size();
	event.Skip();
}

void PixButton::OnPaint(wxPaintEvent & event)
{
 	wxPaintDC pdc(this);

	if (!_backing_store) {
		return;
	}
	
 	draw_area(_memdc);

 	pdc.Blit(0, 0, _width, _height, &_memdc, 0, 0);
}

static inline int get_mouse_up_button(const wxMouseEvent &ev)
{
	if (ev.LeftUp()) return (int) PixButton::LeftButton;
	else if (ev.MiddleUp()) return (int) PixButton::MiddleButton;
	else if (ev.RightUp()) return (int) PixButton::RightButton;
	else return 0;
}

static inline int get_mouse_button(const wxMouseEvent &ev)
{
	if (ev.ButtonDown()) {
		if (ev.LeftDown()) return (int) PixButton::LeftButton;
		else if (ev.MiddleDown()) return (int) PixButton::MiddleButton;
		else if (ev.RightDown()) return (int) PixButton::RightButton;
		else return 0;
	}
	else if (ev.ButtonUp()) {
		if (ev.LeftUp()) return (int) PixButton::LeftButton;
		else if (ev.MiddleUp()) return (int) PixButton::MiddleButton;
		else if (ev.RightUp()) return (int) PixButton::RightButton;
		else return 0;
	}
	else return 0;
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
	else if (ev.RightDown()) {
		if (_popup_menu) {
			this->PopupMenu ( _popup_menu, ev.GetX(), ev.GetY());
		}
	}
	else if (ev.RightUp()) {
		//this->PopupMenu ( _popup_menu, ev.GetX(), ev.GetY());
	}
	else if (ev.ButtonDown())
	{
		if (!(ev.MiddleDown() && ev.ControlDown())) {
			_bstate = Selected;
			pressed (get_mouse_button(ev)); // emit
			CaptureMouse();
            _pressed = true;
			Refresh(false);
		}
	}
	else if (ev.ButtonUp())
	{
		_bstate = Normal;
        if (_pressed) {
            ReleaseMouse();
        }
		released (get_mouse_button(ev)); // emit

        _pressed = false;
        
		wxPoint pt = ev.GetPosition();
		wxRect bounds = GetRect();
		pt.x += bounds.x;
		pt.y += bounds.y;

		if (bounds.Contains(pt)) {
			clicked (get_mouse_button(ev)); // emit

			if (ev.MiddleUp() && ev.ControlDown()) {
				bind_request(); // emit
			}
		}
		
		Refresh(false);
	}
	else if (ev.ButtonDClick()) {
		_bstate = Selected;
		pressed (get_mouse_button(ev)); // emit
		Refresh(false);
	}
	else if (ev.Entering())
	{
		_estate = Inside;
		enter(); // emit
		Refresh(false);
	}
	else if (ev.Leaving())
	{
		_estate = Outside;
		leave(); // emit
		Refresh(false);
	}
	
	ev.Skip();
}

void PixButton::on_menu_events (wxCommandEvent &ev)
{
	if (ev.GetId() == ID_BindMenuOp) {
		bind_request(); // emit
	}
}


void PixButton::draw_area(wxDC & dc)
{
	dc.SetBackground(_bgbrush);
	dc.Clear();

	if (!IsEnabled()) {
		dc.DrawBitmap (_disabled_bitmap, 0, 0);
	}
	else if (_active) {
		dc.DrawBitmap (_active_bitmap, 0, 0);
	}
	else {
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
}
