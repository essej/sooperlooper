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

#include "choice_box.hpp"



using namespace SooperLooperGui;
using namespace std;


BEGIN_EVENT_TABLE(ChoiceBox, wxWindow)

	EVT_SIZE(ChoiceBox::OnSize)
	EVT_PAINT(ChoiceBox::OnPaint)
	EVT_MOUSE_EVENTS(ChoiceBox::OnMouseEvents)
	EVT_MOUSEWHEEL (ChoiceBox::OnMouseEvents)
	
END_EVENT_TABLE()

ChoiceBox::ChoiceBox(wxWindow * parent, wxWindowID id, const wxPoint& pos, const wxSize& size)
	: wxWindow(parent, id, pos, size)
{
	_curr_index = 0;
	_backing_store = 0;
	_dragging = false;
	
	_bgcolor.Set(30,30,30);
	_bgbrush.SetColour (_bgcolor);
	SetBackgroundColour (_bgcolor);
	SetThemeEnabled(false);

	_valuecolor.Set(244, 255, 178);

	_textcolor = *wxWHITE;
	_barcolor.Set(14, 50, 89);
	_overbarcolor.Set(20, 40, 50);
	_barbrush.SetColour(_bgcolor);
	
	_bordercolor.Set(67, 83, 103);
	_borderpen.SetColour(_bordercolor);
	_borderpen.SetWidth(1);
	_borderbrush.SetColour(_bgcolor);

	_linebrush.SetColour(wxColour(154, 245, 168));
	
}

ChoiceBox::~ChoiceBox()
{

}


void
ChoiceBox::append_choice (const wxString & val)
{
	_choices.push_back(val);

	if (_choices.size() == 1) {
		// first one
		_curr_index = 0;
		update_value_str();
		Refresh(false);
	}
}


void ChoiceBox::clear_choices ()
{
	_choices.clear();
	_curr_index = -1;
	update_value_str();
	Refresh(false);
}

void ChoiceBox::get_choices (ChoiceList & clist)
{
	clist.insert (clist.begin(), _choices.begin(), _choices.end());
}

void
ChoiceBox::set_label (const wxString & label)
{
	_label_str = label;
	Refresh(false);	
}


void
ChoiceBox::set_string_value (const wxString &val)
{
	// search through
	int n = 0;
	for (ChoiceList::iterator iter =  _choices.begin(); iter != _choices.end(); ++iter, n++) {
		if ((*iter) == val) {
			if (_curr_index != n) {
				_curr_index = n;
				update_value_str();
				Refresh(false);
			}
			break;
		}
	}
}

wxString
ChoiceBox::get_string_value ()
{

	return _value_str;
	
}

void
ChoiceBox::set_index_value (int ind)
{
	if (ind < (int) _choices.size() && ind >= 0) {
		if (_curr_index != ind) {
			_curr_index = ind;
			update_value_str();
			Refresh(false);
		}
	}
}


void
ChoiceBox::update_value_str()
{
	if (_curr_index >= 0 && _curr_index < (int) _choices.size()) {
		_value_str = _choices[_curr_index];
	}
	else {
		_value_str = wxT("");
	}
}


void ChoiceBox::set_bg_color (const wxColour & col)
{
	_bgcolor = col;
	_bgbrush.SetColour (col);
	_borderbrush.SetColour (col);
	SetBackgroundColour (col);
	Refresh(false);
}

void ChoiceBox::set_label_color (const wxColour & col)
{
	_textcolor = col;
	Refresh(false);
}

void ChoiceBox::set_border_color (const wxColour & col)
{
	_bordercolor = col;
	_borderpen.SetColour (col);
	Refresh(false);
}


void
ChoiceBox::OnSize(wxSizeEvent & event)
{
	GetClientSize(&_width, &_height);

	if (_backing_store) {
		delete _backing_store;
	}
	_backing_store = new wxBitmap(_width, _height);

	
	event.Skip();
}

void ChoiceBox::OnPaint(wxPaintEvent & event)
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
ChoiceBox::OnMouseEvents (wxMouseEvent &ev)
{
	if (!IsEnabled()) {
		ev.Skip();
		return;
	}

	if (ev.Entering() && !_dragging) {
		_borderbrush.SetColour(_overbarcolor);
		Refresh(false);
	}
	else if (ev.Leaving() && !_dragging) {
		_borderbrush.SetColour(_bgcolor);
		Refresh(false);
	}
	

	
	if (ev.GetEventType() == wxEVT_MOUSEWHEEL)
	{
		// don't get the events right now
		
		
		if (!_choices.empty()) {
			if (ev.GetWheelRotation() > 0) {
				_curr_index = (_curr_index+1) % _choices.size();
				
			}
			else {
				if (_curr_index <= 0) {
					_curr_index = _choices.size() - 1;
				} else {
					_curr_index -= 1;
				}
			}
			update_value_str();
			
			value_changed (_curr_index, _value_str); // emit
			
			Refresh(false);
		}
	}
	else if (ev.ButtonDown() || ev.ButtonDClick())
	{
		_last_x = ev.GetX();

		if (ev.LeftDown() || ev.LeftDClick()) {
			// next, cycle

			if (!_choices.empty()) {
				_curr_index = (_curr_index+1) % _choices.size();
				
				update_value_str();
				
				value_changed (_curr_index, _value_str); // emit
				
				Refresh(false);
			}
		}
	}
	else if (ev.RightUp())
	{
		// todo menu
	}

	ev.Skip();
}


void ChoiceBox::draw_area(wxDC & dc)
{
	wxCoord w,h;
	
	dc.SetFont(GetFont());
	dc.SetBackground(_bgbrush);
	dc.Clear();

	dc.SetBrush(_borderbrush);
	dc.SetPen(_borderpen);
	dc.DrawRectangle (0, 0, _width, _height);
	
	dc.SetPen(*wxTRANSPARENT_PEN);

	
	dc.SetTextForeground(_textcolor);
	dc.GetTextExtent(_label_str, &w, &h);
	dc.DrawText (_label_str, 3, _height - h - 3);

	dc.SetTextForeground(_valuecolor);
	dc.GetTextExtent(_value_str, &w, &h);
	dc.DrawText (_value_str, _width - w - 3, _height - h - 3);
	

}
