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

#include "utils.hpp"
#include "slider_bar.hpp"


using SooperLooper::f_min;
using SooperLooper::f_max;
using namespace SooperLooperGui;
using namespace std;

// Convert a value in dB's to a coefficent
#undef DB_CO
#define DB_CO(g) ((g) > -144.0 ? pow(10.0, (g) * 0.05) : 0.0)
#undef CO_DB
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
	ID_BindMenuOp

};

BEGIN_EVENT_TABLE(SliderBar, wxWindow)

	EVT_SIZE(SliderBar::OnSize)
	EVT_PAINT(SliderBar::OnPaint)
	EVT_MOUSE_EVENTS(SliderBar::OnMouseEvents)
	EVT_MOUSEWHEEL (SliderBar::OnMouseEvents)
	//EVT_TEXT (ID_TextCtrl, SliderBar::on_text_event)
	EVT_TEXT_ENTER (ID_TextCtrl, SliderBar::on_text_event)
	EVT_MENU (ID_EditMenuOp , SliderBar::on_menu_events)
	EVT_MENU (ID_DefaultMenuOp , SliderBar::on_menu_events)
	EVT_MENU (ID_BindMenuOp , SliderBar::on_menu_events)
	
END_EVENT_TABLE()


bool SliderBar::s_use_wheel_def = false;

SliderBar::SliderBar(wxWindow * parent, wxWindowID id,  float lb, float ub, float val, bool midibindable, const wxPoint& pos, const wxSize& size)
	: wxWindow(parent, id, pos, size)
{
	_lower_bound = lb;
	_upper_bound = ub;
	_default_val = _value = val;
	_backing_store = 0;
	_indbm = 0;
	_dragging = false;
	_decimal_digits = 1;
	_text_ctrl = 0;
	_ignoretext = false;
	_oob_flag = false;
	_showval_flag = true;
	_show_ind_bar = false;
	_ind_value = 0.0f;
	_use_pending = false;
	_pending_val = 0.0f;
        _override_def_use_wheel = false;
        _use_wheel = s_use_wheel_def;
        
	_bgcolor.Set(30,30,30);
	_bgbrush.SetColour (_bgcolor);
	SetBackgroundColour (_bgcolor);
	SetThemeEnabled(false);

	_valuecolor.Set(244, 255, 178);

	_textcolor = *wxWHITE;
	_barcolor.Set(14, 50, 89);
	_overbarcolor.Set(20, 65, 104);
	_barbrush.SetColour(_barcolor);
	
	_bordercolor.Set(67, 83, 103);
	_borderpen.SetColour(_bordercolor);
	_borderpen.SetWidth(1);
	_borderbrush.SetColour(_bgcolor);

	_linebrush.SetColour(wxColour(154, 245, 168));
	
	_bar_style = FromLeftStyle;
	_scale_mode = LinearMode;
	_snap_mode = NoSnap;

	_popup_menu = new wxMenu(wxT(""));
	_popup_menu->Append ( new wxMenuItem(_popup_menu, ID_EditMenuOp, wxT("Edit")));
	_popup_menu->Append ( new wxMenuItem(_popup_menu, ID_DefaultMenuOp, wxT("Set to default")));
	if (midibindable) {
		_popup_menu->AppendSeparator();
		_popup_menu->Append ( new wxMenuItem(_popup_menu, ID_BindMenuOp, wxT("Learn MIDI Binding")));
	}

	_indcolor.Set(47, 149, 133);
	_indbrush.SetColour(_indcolor);
	_indmaxcolor.Set(200, 20, 20);
	_indmaxbrush.SetColour(_indmaxcolor);
	
	update_size();
}

SliderBar::~SliderBar()
{
	_memdc.SelectObject(wxNullBitmap);
	if (_backing_store) {
		delete _backing_store;
	}

	_inddc.SelectObject(wxNullBitmap);
	if (_indbm) {
		delete _indbm;
	}

}

bool
SliderBar::SetFont(const wxFont & fnt)
{
	bool ret = wxWindow::SetFont(fnt);
	_memdc.SetFont(fnt);
	do_redraw();
	return ret;
}


void
SliderBar::set_style (BarStyle md)
{
	if (md != _bar_style) {
		_bar_style = md;
		do_redraw();
	}
}

void
SliderBar::set_snap_mode (SnapMode md)
{
	if (md != _snap_mode) {
		_snap_mode = md;
	}
}

void
SliderBar::set_scale_mode (ScaleMode mode)
{
	if (mode != _scale_mode) {
		_scale_mode = mode;
		update_value_str();
		do_redraw();
	}
}

void
SliderBar::set_bounds (float lb, float ub)
{
	if (_lower_bound != lb || _upper_bound != ub) {
		_lower_bound = lb;
		_upper_bound = ub;

		// force value to within
		if (_value < _lower_bound) {
			_value = _lower_bound;
			update_value_str();
			do_redraw();
		}
		else if (_value > _upper_bound) {
			_value = _upper_bound;
			update_value_str();
			do_redraw();
		}
	}
}

void
SliderBar::set_label (const wxString & label)
{
	_label_str = label;
	do_redraw();	
}

void
SliderBar::set_units (const wxString & units)
{
	_units_str = units.Strip(wxString::both);
	if (!_units_str.empty()) {
		_units_str = wxT(" ") + _units_str;
	}
	update_value_str();
	do_redraw();	
}

void
SliderBar::set_decimal_digits (int val)
{
	_decimal_digits = val;
	update_value_str();
	do_redraw();	
}

void
SliderBar::set_value (float val, bool refresh)
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

	if (_dragging) {
		// don't update current value if mid drag
		_use_pending = true;
		_pending_val = newval;
		return;
	}
		
	if (newval != _value) {
		_value = newval;
		update_value_str();

		if (refresh) {
			do_redraw();
		}
	}
}

void
SliderBar::set_indicator_value (float val)
{
	float newval = val;

	if (_scale_mode == ZeroGainMode) {
		newval = gain_to_slider_position (val);
	}
	
	if (!_oob_flag) {
		newval = f_min (newval, _upper_bound);
		newval = f_max (newval, _lower_bound);
	}
	
	if (newval != _ind_value) {
		_ind_value = newval;
		Refresh(false);
	}
}


float
SliderBar::get_value ()
{
	if (_scale_mode == ZeroGainMode) {
		return slider_position_to_gain(_value);
	}
	else {
		return _value;
	}
}
	
float
SliderBar::get_indicator_value ()
{
	if (_scale_mode == ZeroGainMode) {
		return slider_position_to_gain(_ind_value);
	}
	else {
		return _ind_value;
	}
}

void
SliderBar::update_value_str()
{
	if (_scale_mode == ZeroGainMode) {
		float gain = slider_position_to_gain(_value);
		if (gain == 0) {
			_value_str.Printf(wxT("-inf%s"), _units_str.c_str());
		}
		else {
			_value_str.Printf(wxT("%.*f%s"), _decimal_digits, CO_DB(gain),  _units_str.c_str());
		}
	}
	else {
		_value_str.Printf(wxT("%.*f%s"), _decimal_digits, _value, _units_str.c_str());
	}
}


wxString SliderBar::get_precise_value_str()
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


void SliderBar::set_bg_color (const wxColour & col)
{
	_bgcolor = col;
	_bgbrush.SetColour (col);
	SetBackgroundColour (col);
	do_redraw();
}

void SliderBar::set_text_color (const wxColour & col)
{
	_textcolor = col;
	do_redraw();
}

void SliderBar::set_border_color (const wxColour & col)
{
	_bordercolor = col;
	_borderbrush.SetColour (col);
	do_redraw();
}

void SliderBar::set_indicator_bar_color (const wxColour & col)
{
	_indcolor = col;
	_indbrush.SetColour (col);
	do_redraw();
}

void SliderBar::set_indicator_max_bar_color (const wxColour & col)
{
	_indmaxcolor = col;
	_indmaxbrush.SetColour (col);
	do_redraw();
}

void SliderBar::set_bar_color (const wxColour & col)
{
	_barcolor = col;
	_barbrush.SetColour (col);
	do_redraw();
}

void
SliderBar::update_size()
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

		_inddc.SelectObject (wxNullBitmap);
		if (_indbm) {
			delete _indbm;
		}
		_indbm = new wxBitmap(_width, _height);
		
		_inddc.SelectObject(*_indbm);

	}
}

void
SliderBar::OnSize(wxSizeEvent & event)
{
	update_size();
	do_redraw();
	event.Skip();
}

void SliderBar::do_redraw ()
{
	if (!_backing_store) {
		return;
	}

	draw_area(_memdc);
	Refresh(false);
}

void SliderBar::OnPaint(wxPaintEvent & event)
{
 	wxPaintDC pdc(this);

 	//draw_area(_memdc);

	if (_show_ind_bar) {
		// first blit memdc into the inddc
		_inddc.Blit(0, 0, _width, _height, &_memdc, 0, 0);

		// draw the indicator 
		draw_ind (_inddc);
		
		// final blit
		pdc.Blit(0, 0, _width, _height, &_inddc, 0, 0);
	}
	else {
		pdc.Blit(0, 0, _width, _height, &_memdc, 0, 0);
	}
}


void
SliderBar::OnMouseEvents (wxMouseEvent &ev)
{
	if (!IsEnabled()) {
		ev.Skip();
		return;
	}

	if (ev.Entering() && !_dragging) {
		_barbrush.SetColour(_overbarcolor);
		do_redraw();
	}
	else if (ev.Leaving() && !_dragging) {
		_barbrush.SetColour(_barcolor);
		do_redraw();
	}
	

	
	if (ev.Dragging() && _dragging)
	{
		int delta = ev.GetX() - _last_x;
		float fdelta = delta * _val_scale;

		if (ev.ControlDown()) {
			fdelta *= 0.5f;
			if (ev.ShiftDown()) {
				fdelta *= 0.5f;
			}
		}

		float newval = _value + fdelta;

		//cerr << "dragging: " << ev.GetX() << "  " << delta << "  " << fdelta << "  "  << newval << endl;

		if (_snap_mode == IntegerSnap) {
			newval = nearbyintf (newval);
		}

		newval = max (min (newval, _upper_bound), _lower_bound);
		
		if (newval != _value) {
			_value = newval;
			
			value_changed (get_value()); // emit

			update_value_str();
			do_redraw();
			//cerr << "new val is: " << _value << endl;

		
			_last_x = ev.GetX();
		}
	}
	else if (ev.Moving()) {
		// do nothing
	}
	else if (ev.GetEventType() == wxEVT_MOUSEWHEEL)
	{
            if ((_override_def_use_wheel ? _use_wheel : s_use_wheel_def )) {
		float fscale = 0.02f * (ev.ControlDown() ? 0.5f: 1.0f);
		float newval;
		
		if (_snap_mode == IntegerSnap) {
                   newval = _value + ((ev.GetWheelRotation() > 0) ? 1 : -1);
		}
		else {
			if (ev.GetWheelRotation() > 0) {
				newval = _value + (_upper_bound - _lower_bound) * fscale;			
			}
			else {
				newval = _value - (_upper_bound - _lower_bound) * fscale;			
			}
		}


		if (_snap_mode == IntegerSnap) {
			newval = nearbyintf (newval);
		}

		newval = max (min (newval, _upper_bound), _lower_bound);

		
		_value = newval;
		
		value_changed (get_value()); // emit
		
		update_value_str();
		do_redraw();
            }
	}
	else if (ev.RightDown()) {
		this->PopupMenu ( _popup_menu, ev.GetX(), ev.GetY());
	}
	else if (ev.RightUp()) {
		//this->PopupMenu ( _popup_menu, ev.GetX(), ev.GetY());
	}
	else if (ev.ButtonDown())
	{
		CaptureMouse();
		_dragging = true;
		_last_x = ev.GetX();
		pressed(); // emit
		
		if (ev.MiddleDown() && !ev.ControlDown()) {
			// set immediately
			float newval = (ev.GetX() * _val_scale) + _lower_bound;

			if (_snap_mode == IntegerSnap) {
				newval = nearbyintf (newval);
			}

			_value = newval;
			
			value_changed (get_value()); // emit

			update_value_str();
			do_redraw();
		}
		else if (ev.LeftDown() && ev.ShiftDown()) {
			// set to default
			_value = max (min (_default_val, _upper_bound), _lower_bound);

			value_changed(get_value());
			update_value_str();
			do_redraw();
		}
	}
	else if (ev.ButtonUp())
	{
        if (_dragging) {
            ReleaseMouse();
        }
        
		_dragging = false;


		if (_use_pending) {
			// This didn't really work
			//if (_pending_val != _value) {
			//	_value = _pending_val;
			//	update_value_str();
			//}
			_use_pending = false;
		}
		
		
		if (ev.GetX() >= _width || ev.GetX() < 0
		    || ev.GetY() < 0 || ev.GetY() > _height) {
			_barbrush.SetColour(_barcolor);
			do_redraw();
		}
		else {
			_barbrush.SetColour(_overbarcolor);
			do_redraw();
		}

		if (ev.MiddleUp() && ev.ControlDown())
		{
			// binding click
			bind_request(); // emit
		}

		released(); // emit		
	}
	else if (ev.ButtonDClick()) {
		// this got annoying
		//show_text_ctrl ();
	}
	else {
		ev.Skip();
	}
}


void SliderBar::on_menu_events (wxCommandEvent &ev)
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
		do_redraw();
	}
}


void SliderBar::draw_area(wxDC & dc)
{
	wxCoord w,h;
	int pixw;
	
	dc.SetBackground(_bgbrush);
	dc.Clear();

	dc.SetBrush(_borderbrush);
	dc.SetPen(_borderpen);
	dc.DrawRectangle (0, 0, _width, _height);
	
	
	dc.SetPen(*wxTRANSPARENT_PEN);
	dc.SetBrush(_barbrush);

	if (_bar_style == FromLeftStyle)
	{
		pixw = (int) ((_value - _lower_bound) / _val_scale);
		dc.DrawRectangle (1, 1, pixw-1, _height-2);

	}
	else if (_bar_style == FromRightStyle)
	{
		pixw = (int) ((_upper_bound - _value) / _val_scale);
		dc.DrawRectangle (pixw, 1, _width - pixw - 1, _height-2);

	}

	if (_bar_style != HiddenStyle)
	{
		dc.SetBrush(_linebrush);
		pixw = (int) ((_value - _lower_bound) / _val_scale);
		dc.DrawRectangle (pixw - 1, 1, 2, _height-2);
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

void SliderBar::draw_ind(wxDC & dc)
{
	int pixw;

	dc.SetPen(*wxTRANSPARENT_PEN);
	
	if (_bar_style == FromLeftStyle)
	{
		pixw = (int) ((_ind_value - _lower_bound) / _val_scale);
		if (pixw > 0) {
			if (_ind_value >= _upper_bound) {
				dc.SetBrush(_indmaxbrush);
			}
			else {
				dc.SetBrush(_indbrush);
			}
			dc.DrawRectangle (1, 1, pixw-1, 1);
			dc.DrawRectangle (1, _height - 2, pixw-1, 1);
		}
		
	}
	else if (_bar_style == FromRightStyle)
	{
		pixw = (int) ((_upper_bound - _ind_value) / _val_scale);
		if (pixw < _width) {
			if (_ind_value >= _upper_bound) {
				dc.SetBrush(_indmaxbrush);
			}
			else {
				dc.SetBrush(_indbrush);
			}
			dc.DrawRectangle (pixw, 1, _width - pixw -1, 2);
			dc.DrawRectangle (pixw, _height - 2, _width - pixw - 1, 1);
		}
	}
	
	pixw = (int) ((_ind_value - _lower_bound) / _val_scale);
	if (pixw > 0) {
		if (_ind_value >= _upper_bound) {
			dc.SetBrush(_indmaxbrush);
		}
		else {
			dc.SetBrush(_indbrush);
		}
		dc.DrawRectangle (pixw - 2, 1, 2, _height-2);
	}
}

void SliderBar::show_text_ctrl ()
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

void SliderBar::hide_text_ctrl ()
{
	if (_text_ctrl && _text_ctrl->IsShown()) {
		_text_ctrl->Show(false);

		SetFocus();
	}
}

void SliderBar::on_text_event (wxCommandEvent &ev)
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

BEGIN_EVENT_TABLE(SliderBar::HidingTextCtrl, wxTextCtrl)
	EVT_KILL_FOCUS (SliderBar::HidingTextCtrl::on_focus_event)
END_EVENT_TABLE()
	
void SliderBar::HidingTextCtrl::on_focus_event (wxFocusEvent & ev)
{
	if (ev.GetEventType() == wxEVT_KILL_FOCUS) {
		Show(false);
	}
	ev.Skip();
}
