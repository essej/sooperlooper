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


enum {
	ID_PopupBase  = 8000,
	ID_BindMenuOp = 9000

};

BEGIN_EVENT_TABLE(ChoiceBox, wxWindow)

	EVT_SIZE(ChoiceBox::OnSize)
	EVT_PAINT(ChoiceBox::OnPaint)
	EVT_MOUSE_EVENTS(ChoiceBox::OnMouseEvents)
	EVT_MOUSEWHEEL (ChoiceBox::OnMouseEvents)
	EVT_KILL_FOCUS (ChoiceBox::OnFocusEvent)
	EVT_MENU (ID_BindMenuOp , ChoiceBox::on_menu_item)
	
END_EVENT_TABLE()

bool ChoiceBox::s_use_wheel_def = false;

ChoiceBox::ChoiceBox(wxWindow * parent, wxWindowID id, bool bindable, const wxPoint& pos, const wxSize& size)
	: wxWindow(parent, id, pos, size)
{
	_curr_index = 0;
	_backing_store = 0;
	_dragging = false;
	_popup_menu = 0;
	_data_value = 0;
	_bindable = bindable;
	
	_bgcolor.Set(0,0,0);
	_bgbrush.SetColour (_bgcolor);
	SetBackgroundColour (_bgcolor);
	SetThemeEnabled(false);

	_valuecolor.Set(244, 255, 178);

	_textcolor = *wxWHITE;
	_barcolor.Set(14, 50, 89);
	_overbarcolor.Set(20, 40, 50);
	_barbrush.SetColour(_bgcolor);
	
	_bgbordercolor.Set(30,30,30);
	_bordercolor.Set(67, 83, 103);
	_borderpen.SetColour(_bordercolor);
	_borderpen.SetWidth(1);
	_borderbrush.SetColour(_bgbordercolor);

	_linebrush.SetColour(wxColour(154, 245, 168));

        _override_def_use_wheel = false;
        _use_wheel = s_use_wheel_def;
        
	update_size();
}

ChoiceBox::~ChoiceBox()
{
	_memdc.SelectObject(wxNullBitmap);
	if (_backing_store) {
		delete _backing_store;
	}
}

bool
ChoiceBox::SetFont(const wxFont & fnt)
{
	bool ret = wxWindow::SetFont(fnt);
	_memdc.SetFont(fnt);
	return ret;
}


void
ChoiceBox::append_choice (const wxString & item, long data)
{
	_choices.push_back(ChoiceItem(item, data));

	if (_choices.size() == 1) {
		// first one
		_curr_index = 0;
		update_value_str();
		Refresh(false);
	}

	ensure_popup (true);
}


void ChoiceBox::clear_choices ()
{
	_choices.clear();
	_curr_index = -1;
	ensure_popup (true);
	update_value_str();
	Refresh(false);
}

void ChoiceBox::get_choices (ChoiceList & clist)
{
	clist.insert (clist.begin(), _choices.begin(), _choices.end());
}

void ChoiceBox::ensure_popup (bool force_build)
{
	bool building = false;
	
	if (_popup_menu && force_build) {
		delete _popup_menu;
		_popup_menu = 0;
	}

	if (!_popup_menu) {
		_popup_menu = new wxMenu();
		building = true;
	}
	
	int id = ID_PopupBase;

	
	for (ChoiceList::iterator iter =  _choices.begin(); iter != _choices.end(); ++iter) {
		if (building) {
			_popup_menu->AppendCheckItem (id, (*iter).first);

			this->Connect( id,  wxEVT_COMMAND_MENU_SELECTED,
				       (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
				       &ChoiceBox::on_menu_item, 0);
		}
		
		if (_value_str == (*iter).first) {
			_popup_menu->Check (id, true);
		}
		else {
			_popup_menu->Check (id, false);
		}

		++id;
	}

	if (building) {
		_popup_menu->AppendSeparator();
		_popup_menu->Append (ID_BindMenuOp, wxT("Learn MIDI Binding"));
	}

}

void ChoiceBox::on_menu_item (wxCommandEvent &ev)
{
	int id = ev.GetId();
	int index = id - (int) ID_PopupBase;

	if (id == ID_BindMenuOp) {
		bind_request(); // emit
	}
	else {
		set_index_value (index);
		
		value_changed (_curr_index, _value_str); // emit
	}
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
		if ((*iter).first == val) {
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

long
ChoiceBox::get_data_value ()
{
	return _data_value;
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
		_value_str = _choices[_curr_index].first;
		_data_value = _choices[_curr_index].second;
	}
	else {
		_value_str = wxT("");
		_data_value = 0;
	}
}


void ChoiceBox::set_bg_color (const wxColour & col)
{
	_bgcolor = col;
	_bgbrush.SetColour (col);
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

void ChoiceBox::set_bg_border_color (const wxColour & col)
{
	_bgbordercolor = col;
	_borderbrush.SetColour (col);
	Refresh(false);
}

void
ChoiceBox::update_size()
{
	GetClientSize(&_width, &_height);

	if (_width > 0 && _height > 0) {
		_memdc.SelectObject (wxNullBitmap);
		if (_backing_store) {
			delete _backing_store;
		}
		_backing_store = new wxBitmap(_width, _height);
		_memdc.SelectObject(*_backing_store);
		_memdc.SetFont(GetFont());
	}
}

void
ChoiceBox::OnSize(wxSizeEvent & event)
{
	update_size();
	event.Skip();
}

void ChoiceBox::OnPaint(wxPaintEvent & event)
{
 	wxPaintDC pdc(this);

	if (!_backing_store) {
		return;
	}
	
 	draw_area(_memdc);

 	pdc.Blit(0, 0, _width, _height, &_memdc, 0, 0);
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
		_borderbrush.SetColour(_bgbordercolor);
		Refresh(false);
	}
	

	
	if (ev.GetEventType() == wxEVT_MOUSEWHEEL)
	{
		// don't get the events right now
            if ((_override_def_use_wheel ? _use_wheel : s_use_wheel_def )) {
                                
		
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
	}
	else if (ev.RightDown())
	{
		ensure_popup ();
		
		PopupMenu (_popup_menu, ev.GetX(), ev.GetY());
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

	ev.Skip();
}

void ChoiceBox::OnFocusEvent (wxFocusEvent &ev)
{
	if (ev.GetEventType() == wxEVT_KILL_FOCUS) {
		// focus kill
		_borderbrush.SetColour(_bgbordercolor);
		Refresh(false);
	}

	ev.Skip();
}



void ChoiceBox::draw_area(wxDC & dc)
{
	wxCoord w,h;
	wxPoint shape[6];
	
	dc.SetBackground(_bgbrush);
	dc.Clear();

	dc.SetBrush(_borderbrush);
	dc.SetPen(_borderpen);
//	dc.DrawRectangle (0, 0, _width, _height);

	shape[0].x = 0;  shape[0].y = _height-1;
	shape[1].x = 0;  shape[1].y = 2;
	shape[2].x = 2;  shape[2].y = 0;
	shape[3].x = _width - 3;  shape[3].y = 0;
	shape[4].x = _width -1;  shape[4].y = 2;
	shape[5].x = _width -1;  shape[5].y = _height -1;

	dc.DrawPolygon (6, shape);
	
	dc.SetPen(*wxTRANSPARENT_PEN);

	
	dc.SetTextForeground(_textcolor);
	dc.GetTextExtent(_label_str, &w, &h);
	dc.DrawText (_label_str, 3, _height - h - 3);

	dc.SetTextForeground(_valuecolor);
	dc.GetTextExtent(_value_str, &w, &h);
	dc.DrawText (_value_str, _width - w - 3, _height - h - 3);
	

}
