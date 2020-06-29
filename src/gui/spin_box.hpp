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

#ifndef __sooperlooper_gui_spin_box__
#define __sooperlooper_gui_spin_box__


#include <wx/wx.h>


#include <sigc++/sigc++.h>


namespace SooperLooperGui {


class SpinBox
	: public wxWindow
{
  public:
	
	// ctor(s)
	SpinBox(wxWindow * parent, wxWindowID id=-1, float lb=0.0f, float ub=1.0f, float val=0.5f, bool midibindable=true,
		  const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
	virtual ~SpinBox();

	enum ScaleMode {
		LinearMode = 0,
		ZeroGainMode
	};

	enum SnapMode {
		NoSnap = 0,
		IntegerSnap
	};


	virtual bool SetFont(const wxFont & fnt);
	
	void set_bounds (float lb, float ub);
	void get_bounds (float &lb, float &ub) { lb = _lower_bound; ub = _upper_bound; }

	void set_value (float val);
	float get_value ();

	void set_default_value (float val) { _default_val = val; }
	float get_default_value () { return _default_val; }
	
	void set_label (const wxString &label);
	wxString get_label () { return _label_str; }

	void set_units (const wxString &units);
	wxString get_units () { return _units_str; }
	
	void set_scale_mode (ScaleMode mode);
	ScaleMode get_scale_mode () { return _scale_mode; }
	
	void set_snap_mode (SnapMode mode);
	SnapMode get_snap_mode () { return _snap_mode; }
	
	void set_allow_outside_bounds (bool val) { _oob_flag = val; }
	bool get_allow_outside_bounds () { return _oob_flag; }

	void set_show_value (bool val) { _showval_flag = val; }
	bool get_show_value () { return _showval_flag; }
	
 	void set_bg_color (const wxColour & col);
	wxColour & get_bg_color () { return _bgcolor; }

	void set_disabled_bg_color (const wxColour & col);
	wxColour & get_disabled_bg_color () { return _disabled_bgcolor; }

 	void set_bar_color (const wxColour & col);
	wxColour & get_bar_color () { return _barcolor; }

 	void set_text_color (const wxColour & col);
	wxColour & get_text_color () { return _textcolor; }

 	void set_value_color (const wxColour & col);
	wxColour & get_value_color () { return _valuecolor; }
	
 	void set_border_color (const wxColour & col);
	wxColour & get_border_color () { return _bordercolor; }

	void set_decimal_digits (int num);
	int get_decimal_digits () { return _decimal_digits; }
	
	float get_increment () { return _increment; }
	void set_increment (float val) { _increment = val; }

    void set_use_mousewheel (bool flag) { _use_wheel = flag; _override_def_use_wheel = true; }
    bool get_use_mousewheel () const { return _use_wheel; }
    
    static void set_use_mousewheel_default (bool flag) { s_use_wheel_def = flag; }
    static bool get_use_mousewheel_default () { return s_use_wheel_def; }

    
	sigc::signal0<void> pressed;
	sigc::signal0<void> released;
	sigc::signal1<void, float> value_changed;
	sigc::signal0<void> bind_request;
	
  protected:

	void OnPaint (wxPaintEvent &ev);
	void OnSize (wxSizeEvent &ev);
	void OnMouseEvents (wxMouseEvent &ev);
	void OnFocusEvent (wxFocusEvent &ev);

	void draw_area (wxDC & dc);
	void update_size();
	void update_bar_shape();
	
	void show_text_ctrl ();
	void hide_text_ctrl ();
	void on_text_event (wxCommandEvent &ev);
	void on_menu_events (wxCommandEvent &ev);
	void on_update_timer (wxTimerEvent &ev);
	
	void update_value_str();

	wxString get_precise_value_str();
	
	int _width, _height;
	wxBitmap * _backing_store;
	wxMemoryDC _memdc;

	wxMenu * _popup_menu;
	
	wxColour _bgcolor;
	wxColour _disabled_bgcolor;
	wxBrush  _bgbrush;

	wxColour _barcolor;
	wxColour _overbarcolor;
	wxBrush  _barbrush;
	wxColour _bordercolor;
	wxBrush  _borderbrush;
	wxPen    _borderpen;  
	wxBrush    _linebrush;  
	
	wxColour _textcolor;
	wxColour _valuecolor;
	
	float _value;
	float _default_val;
	float _lower_bound, _upper_bound;
	float _increment;
	float _direction;
	
	wxString _value_str;
	wxString _label_str;
	wxString _units_str;

	class HidingTextCtrl : public wxTextCtrl {
	   public:
		HidingTextCtrl (wxWindow* par, wxWindowID id, const wxString & value = wxT(""), const wxPoint & pos = wxDefaultPosition,
				const wxSize & size = wxDefaultSize, long style = 0)
			: wxTextCtrl (par, id, value, pos, size, style) {}

		virtual ~HidingTextCtrl() {}

		void on_focus_event(wxFocusEvent &ev);
	   private:
		DECLARE_EVENT_TABLE()
	};
	
	HidingTextCtrl * _text_ctrl;
	
	bool _dragging;
	bool _ignoretext;
	long _press_time;

     bool _override_def_use_wheel;
    bool _use_wheel;
    static bool s_use_wheel_def;
    
	float _val_scale;
	ScaleMode  _scale_mode;
	SnapMode   _snap_mode;
	int        _decimal_digits;
	bool       _oob_flag;
	bool       _showval_flag;

	wxTimer *_update_timer;
	float   _curr_adjust;
	long    _curr_timeout;

	wxPoint _border_shape[8];
	wxPoint _bar_shape[8];
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


};


#endif
