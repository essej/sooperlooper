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

#include "spin_box.hpp"



using namespace SooperLooperGui;
using namespace std;

// Convert a value in dB's to a coefficent
#define DB_CO(g) ((g) > -144.0 ? pow(10.0, (g) * 0.05) : 0.0)
#define CO_DB(v) (20.0 * log10(v))

static inline double 
gain_to_slider_position (double g)
{
	if (g == 0) return 0;
	//return pow((6.0*log(g)/log(2.0)+192.0)/198.0, 8.0);
	return pow((6.0*log(g)/log(2.0)+198.0)/198.0, 8.0);

}

static inline double 
slider_position_to_gain (double pos)
{
	if (pos == 0) {
		return 0.0;
	}
	/* XXX Marcus writes: this doesn't seem right to me. but i don't have a better answer ... */
	//return pow (2.0,(sqrt(sqrt(sqrt(pos)))*198.0-192.0)/6.0);
	return pow (2.0,(sqrt(sqrt(sqrt(pos)))*198.0-198.0)/6.0);
}


enum {
	ID_TextCtrl = 8000,
	ID_EditMenuOp,
	ID_DefaultMenuOp,
	ID_BindMenuOp,
	ID_UpdateTimer

};

BEGIN_EVENT_TABLE(SpinBox, wxWindow)

	EVT_SIZE(SpinBox::OnSize)
	EVT_PAINT(SpinBox::OnPaint)
	EVT_MOUSE_EVENTS(SpinBox::OnMouseEvents)
	EVT_MOUSEWHEEL (SpinBox::OnMouseEvents)
	//EVT_TEXT (ID_TextCtrl, SpinBox::on_text_event)
	EVT_TEXT_ENTER (ID_TextCtrl, SpinBox::on_text_event)
	EVT_MENU (ID_EditMenuOp , SpinBox::on_menu_events)
	EVT_MENU (ID_DefaultMenuOp , SpinBox::on_menu_events)
	EVT_MENU (ID_BindMenuOp , SpinBox::on_menu_events)
	EVT_TIMER (ID_UpdateTimer, SpinBox::on_update_timer)
	EVT_KILL_FOCUS (SpinBox::OnFocusEvent)
	
END_EVENT_TABLE()

bool SpinBox::s_use_wheel_def = false;


SpinBox::SpinBox(wxWindow * parent, wxWindowID id,  float lb, float ub, float val, bool midibindable, const wxPoint& pos, const wxSize& size)
	: wxWindow(parent, id, pos, size)
{
	_lower_bound = lb;
	_upper_bound = ub;
	_default_val = _value = val;
	_backing_store = 0;
	_dragging = false;
	_decimal_digits = 1;
	_text_ctrl = 0;
	_ignoretext = false;
	_oob_flag = false;
	_showval_flag = true;
	_increment = 1.0f;
	_direction = 0.0f;

        _override_def_use_wheel = false;
        _use_wheel = s_use_wheel_def;
        
	_bgcolor.Set(30,30,30);
	_disabled_bgcolor.Set(60,60,60);
	_bgbrush.SetColour (_bgcolor);
	SetBackgroundColour (_bgcolor);
	SetThemeEnabled(false);

	_valuecolor.Set(244, 255, 178);

	_textcolor = *wxWHITE;
	_barcolor.Set(14, 50, 100);
	_overbarcolor.Set(20, 50, 80);

	_barbrush.SetColour(_bgcolor);
	
	//_bgbordercolor.Set(30,30,30);
	_bordercolor.Set(67, 83, 103);
	_borderpen.SetColour(_bordercolor);
	_borderpen.SetWidth(1);
	_borderbrush.SetColour(_bgcolor);

	_linebrush.SetColour(wxColour(154, 245, 168));
	
	_scale_mode = LinearMode;
	_snap_mode = NoSnap;

	_popup_menu = new wxMenu(wxT(""));
	_popup_menu->Append ( new wxMenuItem(_popup_menu, ID_EditMenuOp, wxT("Edit")));
	_popup_menu->Append ( new wxMenuItem(_popup_menu, ID_DefaultMenuOp, wxT("Set to default")));
	if (midibindable) {
		_popup_menu->AppendSeparator();
		_popup_menu->Append ( new wxMenuItem(_popup_menu, ID_BindMenuOp, wxT("Learn MIDI Binding")));
	}

	_update_timer = new wxTimer(this, ID_UpdateTimer);
	
	update_size();
}

SpinBox::~SpinBox()
{
	_memdc.SelectObject(wxNullBitmap);
	if (_backing_store) {
		delete _backing_store;
	}
}

bool
SpinBox::SetFont(const wxFont & fnt)
{
	bool ret = wxWindow::SetFont(fnt);
	_memdc.SetFont(fnt);
	return ret;
}



void
SpinBox::set_snap_mode (SnapMode md)
{
	if (md != _snap_mode) {
		_snap_mode = md;
	}
}

void
SpinBox::set_scale_mode (ScaleMode mode)
{
	if (mode != _scale_mode) {
		_scale_mode = mode;
		update_value_str();
		Refresh(false);
	}
}

void
SpinBox::set_bounds (float lb, float ub)
{
	if (_lower_bound != lb || _upper_bound != ub) {
		_lower_bound = lb;
		_upper_bound = ub;

		// force value to within
		if (_value < _lower_bound) {
			_value = _lower_bound;
			update_value_str();
			Refresh(false);
		}
		else if (_value > _upper_bound) {
			_value = _upper_bound;
			update_value_str();
			Refresh(false);
		}
	}
}

void
SpinBox::set_label (const wxString & label)
{
	_label_str = label;
	Refresh(false);	
}

void
SpinBox::set_units (const wxString & units)
{
	_units_str = units;
	update_value_str();
	Refresh(false);	
}

void
SpinBox::set_decimal_digits (int val)
{
	_decimal_digits = val;
	update_value_str();
	Refresh(false);	
}

void
SpinBox::set_value (float val)
{
	float newval = val;

	if (_scale_mode == ZeroGainMode) {
		newval = gain_to_slider_position (val);
	}

// 	if (_snap_mode == IntegerSnap) {
// 		newval = nearbyintf (newval);
// 	}

	if (!_oob_flag) {
		newval = min (newval, _upper_bound);
		newval = max (newval, _lower_bound);
	}
	
	if (newval != _value) {
		_value = newval;
		update_value_str();
		Refresh(false);
	}
}

float
SpinBox::get_value ()
{
	if (_scale_mode == ZeroGainMode) {
		return slider_position_to_gain(_value);
	}
	else {
		return _value;
	}
}
	

void
SpinBox::update_value_str()
{
	if (_scale_mode == ZeroGainMode) {
		float gain = slider_position_to_gain(_value);
		if (gain == 0) {
			_value_str.Printf(wxT("-inf %s"), _units_str.c_str());
		}
		else {
			_value_str.Printf(wxT("%.*f %s"), _decimal_digits, CO_DB(gain),  _units_str.c_str());
		}
	}
	else {
		_value_str.Printf(wxT("%.*f %s"), _decimal_digits, _value, _units_str.c_str());
	}
}


wxString SpinBox::get_precise_value_str()
{
	wxString valstr;
	
	if (_scale_mode == ZeroGainMode) {
		float gain = slider_position_to_gain(_value);
		if (gain == 0) {
			valstr.Printf(wxT("-inf"));
		}
		else if (_snap_mode == IntegerSnap) {
			valstr.Printf(wxT("%g"), CO_DB(gain));
		}
		else {
			valstr.Printf(wxT("%.8f"), CO_DB(gain));
		}
	}
	else {
		if (_snap_mode == IntegerSnap) {
			valstr.Printf(wxT("%g"), _value);
		}
		else {
			valstr.Printf(wxT("%.8f"), _value);
		}
	}

	return valstr;
}


void SpinBox::set_bg_color (const wxColour & col)
{
	_bgcolor = col;
	_bgbrush.SetColour (col);
	SetBackgroundColour (col);
	Refresh(false);
}

void SpinBox::set_disabled_bg_color (const wxColour & col)
{
	_disabled_bgcolor = col;
	if (!IsEnabled()) {
		_bgbrush.SetColour (col);
		SetBackgroundColour (col);
		Refresh(false);
	}
}

void SpinBox::set_text_color (const wxColour & col)
{
	_textcolor = col;
	Refresh(false);
}

void SpinBox::set_border_color (const wxColour & col)
{
	_bordercolor = col;
	_borderbrush.SetColour (col);
	Refresh(false);
}

void SpinBox::set_bar_color (const wxColour & col)
{
	_barcolor = col;
	_barbrush.SetColour (col);
	Refresh(false);
}

void
SpinBox::update_size()
{
	GetClientSize(&_width, &_height);

	if (_width > 0 && _height > 0) {
		_val_scale = (_upper_bound - _lower_bound) / (_width);
		
		_memdc.SelectObject (wxNullBitmap);
		if (_backing_store) {
			delete _backing_store;
		}
		_backing_store = new wxBitmap(_width, _height);
		
		_memdc.SelectObject(*_backing_store);
		_memdc.SetFont(GetFont());

		_border_shape[0].x = 0;  _border_shape[0].y = _height-3;
		_border_shape[1].x = 0;  _border_shape[1].y = 2;
		_border_shape[2].x = 2;  _border_shape[2].y = 0;
		_border_shape[3].x = _width - 3;  _border_shape[3].y = 0;
		_border_shape[4].x = _width -1;  _border_shape[4].y = 2;
		_border_shape[5].x = _width -1;  _border_shape[5].y = _height - 3;
		_border_shape[6].x = _width -3;  _border_shape[6].y = _height - 1;
		_border_shape[7].x = 2;  _border_shape[7].y = _height - 1;

		update_bar_shape();
	}
}

void
SpinBox::update_bar_shape()
{
	if (_direction < 0) {
		_bar_shape[0].x = 1;   _bar_shape[0].y = 1;
		_bar_shape[1].x = _width/2 - 1;   _bar_shape[1].y = 1;
		_bar_shape[2].x = _width/2 - _height/2;   _bar_shape[2].y = _height/2;
		_bar_shape[3].x = _width/2 - 1;   _bar_shape[3].y = _height - 1;
		_bar_shape[4].x = 1;   _bar_shape[4].y = _height - 1;
	}
	else {
		_bar_shape[0].x = _width - 1;   _bar_shape[0].y = 1;
		_bar_shape[1].x = _width/2 ;   _bar_shape[1].y = 1;
		_bar_shape[2].x = _width/2 + _height/2 - 1;   _bar_shape[2].y = _height/2;
		_bar_shape[3].x = _width/2;   _bar_shape[3].y = _height - 1;
		_bar_shape[4].x = _width - 1;   _bar_shape[4].y = _height - 1;
	}
}

void
SpinBox::OnSize(wxSizeEvent & event)
{
	update_size();
	event.Skip();
}

void SpinBox::OnPaint(wxPaintEvent & event)
{
 	wxPaintDC pdc(this);

	if (!_backing_store) {
		return;
	}
	
 	draw_area(_memdc);

 	pdc.Blit(0, 0, _width, _height, &_memdc, 0, 0);
}

void
SpinBox::on_update_timer (wxTimerEvent &ev)
{
	// update value with current adjust
	float newval = _value;
	long deltatime = ::wxGetLocalTime() - _press_time;
	
	if (deltatime > 2) {
		newval += _curr_adjust * deltatime * deltatime * deltatime;
	}
	else {
		newval += _curr_adjust;
	}

	if (_snap_mode == IntegerSnap) {
		newval = nearbyintf (newval);
	}
	
	newval = max (min (newval, _upper_bound), _lower_bound);

	_value = newval;
	value_changed (get_value()); // emit
	
	update_value_str();
	Refresh(false);

	_update_timer->Start (_curr_timeout, true);
}


void
SpinBox::OnMouseEvents (wxMouseEvent &ev)
{
	if (!IsEnabled()) {
		ev.Skip();
		return;
	}

	if (ev.Entering() && !_dragging) {
		//_borderbrush.SetColour(_overbarcolor);
		_direction = ev.GetX() < _width/2 ? -1.0f : 1.0f; 
		update_bar_shape();
		_barbrush.SetColour(_overbarcolor);
		Refresh(false);
	}
	else if (ev.Leaving() && !_dragging) {
		_barbrush.SetColour(_bgcolor);
		//_borderbrush.SetColour(_bgcolor);
		_direction = 0;
		Refresh(false);
	}
	

	
	if (ev.Dragging() && _dragging)
	{
		ev.Skip();
	}
	else if (ev.Moving()) {
		// do nothing
		float dirct = ev.GetX() < _width/2 ? -1.0f : 1.0f; 

		if (dirct != _direction) {
			_direction = dirct;
			update_bar_shape();
			Refresh(false);
		}
		
	}
	else if (ev.GetEventType() == wxEVT_MOUSEWHEEL)
	{
            if ((_override_def_use_wheel ? _use_wheel : s_use_wheel_def )) {
		float fscale = (ev.ShiftDown() ? 10.0f : 1.0f) * (ev.ControlDown() ? 2.0f: 1.0f);
		float newval;
		
		if (ev.GetWheelRotation() > 0) {
			//newval = _value + (_upper_bound - _lower_bound) * fscale;
			newval = _value + _increment * fscale;
		}
		else {
			newval = _value - _increment * fscale;			
		}


		if (_snap_mode == IntegerSnap) {
			newval = nearbyintf (newval);
		}

		newval = max (min (newval, _upper_bound), _lower_bound);

		
		_value = newval;
		
		value_changed (get_value()); // emit
		
		update_value_str();
		Refresh(false);
            }
	}
	else if (ev.RightDown()) {
		this->PopupMenu ( _popup_menu, ev.GetX(), ev.GetY());
	}
	else if (ev.RightUp()) {
		//this->PopupMenu ( _popup_menu, ev.GetX(), ev.GetY());
	}
	else if (ev.ButtonDown() || ev.LeftDClick())
	{
		CaptureMouse();
		_dragging = true;
		_curr_timeout = -1;
		_barbrush.SetColour(_barcolor);

		pressed(); // emit

		if (ev.MiddleDown() && !ev.ControlDown()) {
			// start editing
			show_text_ctrl ();
		}
		else if (ev.LeftDown() && ev.ShiftDown()) {
			// set to default
			_value = max (min (_default_val, _upper_bound), _lower_bound);

			value_changed(get_value());
			update_value_str();
		}
		else {
			float newval = _value;
			
			_direction = ev.GetX() < _width/2 ? -1.0f : 1.0f;
			_curr_adjust = _increment * _direction;
			
			newval += _curr_adjust;
			
			if (_snap_mode == IntegerSnap) {
				newval = nearbyintf (newval);
			}
			
			newval = max (min (newval, _upper_bound), _lower_bound);
			
			_value = newval;
			value_changed (get_value()); // emit
			update_value_str();

			_press_time = ::wxGetLocalTime(); // seconds
			_curr_timeout = 20; // for now
			_update_timer->Start(400, true); // oneshot

			update_bar_shape();
		}
		
		Refresh(false);
	}
	else if (ev.ButtonUp())
	{
		_dragging = false;
		_update_timer->Stop();
		_curr_timeout = -1;
		ReleaseMouse();

		if (ev.GetX() >= _width || ev.GetX() < 0
		    || ev.GetY() < 0 || ev.GetY() > _height) {
			//_borderbrush.SetColour(_bgcolor);
			_barbrush.SetColour(_bgcolor);
			Refresh(false);
		}
		else {
			//_borderbrush.SetColour(_overbarcolor);
			_barbrush.SetColour(_overbarcolor);
			Refresh(false);
		}

		if (ev.MiddleUp() && ev.ControlDown())
		{
			// binding click
			bind_request(); // emit
		}

		released(); // emit		
	}
	else {
		ev.Skip();
	}
}

void SpinBox::OnFocusEvent (wxFocusEvent &ev)
{
	if (ev.GetEventType() == wxEVT_KILL_FOCUS) {
		// focus kill
		_borderbrush.SetColour(_bgcolor);
		_barbrush.SetColour(_bgcolor);
		Refresh(false);
	}

	ev.Skip();
}


void SpinBox::on_menu_events (wxCommandEvent &ev)
{
	if (ev.GetId() == ID_BindMenuOp) {
		bind_request(); // emit
	}
	else if (ev.GetId() == ID_EditMenuOp) {
		show_text_ctrl ();
	}
	else if (ev.GetId() == ID_DefaultMenuOp) {
		_value = max (min (_default_val, _upper_bound), _lower_bound);
		
		value_changed(get_value());
		update_value_str();
		Refresh(false);
	}
}


void SpinBox::draw_area(wxDC & dc)
{
	wxCoord w,h;

	dc.SetBackground(_bgbrush);
	dc.Clear();

	dc.SetBrush(_borderbrush);
	dc.SetPen(_borderpen);
	//dc.DrawRectangle (0, 0, _width, _height);

	dc.DrawPolygon (8, _border_shape);
	
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.SetBrush(_barbrush);

	if (_direction != 0.0f) {
		dc.DrawPolygon (5, _bar_shape);
	}
	
	dc.SetTextForeground(_textcolor);
	dc.GetTextExtent(_label_str, &w, &h);
	dc.DrawText (_label_str, 3, _height - h - 3);

	if (_showval_flag) {
		dc.SetTextForeground(_valuecolor);
		dc.GetTextExtent(_value_str, &w, &h);
		dc.DrawText (_value_str, _width - w - 3, _height - h - 3);
	}

}


void SpinBox::show_text_ctrl ()
{
	wxString valstr = get_precise_value_str();
	
	if (!_text_ctrl) {
		_text_ctrl = new HidingTextCtrl(this, ID_TextCtrl, valstr, wxPoint(1,1), wxSize(_width - 2, _height - 2),
						wxTE_PROCESS_ENTER|wxTE_RIGHT);
		_text_ctrl->SetName (wxT("KeyAware"));
		_text_ctrl->SetFont(GetFont());
	}

	_text_ctrl->SetValue (valstr);
	
	_text_ctrl->SetSelection (-1, -1);
	
	_text_ctrl->SetSize (_width - 2, _height - 2);
	_text_ctrl->Show(true);
	_text_ctrl->SetFocus();
}

void SpinBox::hide_text_ctrl ()
{
	if (_text_ctrl && _text_ctrl->IsShown()) {
		_text_ctrl->Show(false);

		SetFocus();
	}
}

void SpinBox::on_text_event (wxCommandEvent &ev)
{
	if (ev.GetEventType() == wxEVT_COMMAND_TEXT_ENTER) {
		
		// commit change
		bool good = false;
		bool neginf = false;
		double newval = 0.0;
		
		if (_scale_mode == ZeroGainMode && _text_ctrl->GetValue().Strip(wxString::both) == wxT("-inf")) {
			newval = 0.0;
			good = neginf = true;
		}
		else if (_text_ctrl->GetValue().ToDouble(&newval)) {
			good = true;
		}
		
		if (good) {
			if (_scale_mode == ZeroGainMode && !neginf) {
				newval = DB_CO(newval);
			}

			set_value ((float) newval);
			value_changed (get_value()); // emit
		}
		
		hide_text_ctrl();
	}
}

BEGIN_EVENT_TABLE(SpinBox::HidingTextCtrl, wxTextCtrl)
	EVT_KILL_FOCUS (SpinBox::HidingTextCtrl::on_focus_event)
END_EVENT_TABLE()
	
void SpinBox::HidingTextCtrl::on_focus_event (wxFocusEvent & ev)
{
	if (ev.GetEventType() == wxEVT_KILL_FOCUS) {
		Show(false);
	}
	ev.Skip();
}
