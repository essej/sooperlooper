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

#ifndef __sooperlooper_gui_slider_bar__
#define __sooperlooper_gui_slider_bar__


#include <wx/wx.h>


#include <sigc++/sigc++.h>


namespace SooperLooperGui {


class SliderBar
	: public wxWindow
{
  public:
	
	// ctor(s)
	SliderBar(wxWindow * parent, wxWindowID id=-1, float lb=0.0f, float ub=1.0f, float val=0.5f, bool midibindable=true,
		  const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
	virtual ~SliderBar();

	enum BarStyle {
		FromLeftStyle=0,
		CenterStyle,
		FromRightStyle,
		HiddenStyle
	};

	enum ScaleMode {
		LinearMode = 0,
		ZeroGainMode
	};

	enum SnapMode {
		NoSnap = 0,
		IntegerSnap
	};


	virtual bool SetFont(const wxFont & fnt);
	
	void set_style (BarStyle md);
	BarStyle get_style () { return _bar_style;}

	void set_bounds (float lb, float ub);
	void get_bounds (float &lb, float &ub) { lb = _lower_bound; ub = _upper_bound; }

	void set_value (float val, bool refresh=true);
	float get_value ();

	void set_show_indicator_bar (bool flag) { _show_ind_bar = flag; }
	bool get_show_indicator_bar () { return _show_ind_bar; }
	void set_indicator_value (float val);
	float get_indicator_value ();
	
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

 	void set_bar_color (const wxColour & col);
	wxColour & get_bar_color () { return _barcolor; }

 	void set_text_color (const wxColour & col);
	wxColour & get_text_color () { return _textcolor; }

 	void set_value_color (const wxColour & col);
	wxColour & get_value_color () { return _valuecolor; }
	
 	void set_border_color (const wxColour & col);
	wxColour & get_border_color () { return _bordercolor; }

 	void set_indicator_bar_color (const wxColour & col);
	wxColour & get_indicator_bar_color () { return _indcolor; }

 	void set_indicator_max_bar_color (const wxColour & col);
	wxColour & get_indicator_max_bar_color () { return _indmaxcolor; }
	
	void set_decimal_digits (int num);
	int get_decimal_digits () { return _decimal_digits; }

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

	void do_redraw ();
	
	void draw_area (wxDC & dc);
	void draw_ind (wxDC & dc);
	void update_size();
	
	void show_text_ctrl ();
	void hide_text_ctrl ();
	void on_text_event (wxCommandEvent &ev);
	void on_menu_events (wxCommandEvent &ev);
	
	void update_value_str();

	wxString get_precise_value_str();
	
	int _width, _height;
	wxBitmap * _backing_store;
	wxMemoryDC _memdc;

	wxMemoryDC _inddc;
	wxBitmap * _indbm;

	wxMenu * _popup_menu;
	
	wxColour _bgcolor;
	wxBrush  _bgbrush;

	wxColour _barcolor;
	wxColour _overbarcolor;
	wxBrush  _barbrush;
	wxColour _bordercolor;
	wxBrush  _borderbrush;
	wxPen    _borderpen;  
	wxBrush    _linebrush;  
	wxColor  _indcolor;
	wxColor  _indmaxcolor;
	wxBrush  _indbrush;
	wxBrush  _indmaxbrush;
	wxColour _textcolor;
	wxColour _valuecolor;
	
	float _value;
	float _default_val;
	bool  _show_ind_bar;
	float _ind_value;
	float _lower_bound, _upper_bound;

	bool _use_pending;
	float _pending_val;

    bool _override_def_use_wheel;
    bool _use_wheel;
    static bool s_use_wheel_def;
    
	wxString _value_str;
	wxString _label_str;
	wxString _units_str;

	BarStyle _bar_style;

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
	int _last_x;
	bool _ignoretext;
	
	float _val_scale;
	ScaleMode  _scale_mode;
	SnapMode   _snap_mode;
	int        _decimal_digits;
	bool       _oob_flag;
	bool       _showval_flag;
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


};


#endif
