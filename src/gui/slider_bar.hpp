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
	SliderBar(wxWindow * parent, wxWindowID id=-1, float lb=0.0f, float ub=1.0f, float val=0.5f,
		  const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
	virtual ~SliderBar();

	enum BarStyle {
		FromLeftStyle=0,
		CenterStyle,
		FromRightStyle
	};

	enum ScaleMode {
		LinearMode = 0,
		ZeroGainMode
	};

	enum SnapMode {
		NoSnap = 0,
		IntegerSnap
	};
	
	void set_style (BarStyle md);
	BarStyle get_style () { return _bar_style;}

	void set_bounds (float lb, float ub);
	void get_bounds (float &lb, float &ub) { lb = _lower_bound; ub = _upper_bound; }

	void set_value (float val);
	float get_value ();

	void set_label (const wxString &label);
	wxString get_label () { return _label_str; }

	void set_units (const wxString &units);
	wxString get_units () { return _units_str; }
	
	void set_scale_mode (ScaleMode mode);
	ScaleMode get_scale_mode () { return _scale_mode; }
	
	void set_snap_mode (SnapMode mode);
	SnapMode get_snap_mode () { return _snap_mode; }
	
	
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

	void set_decimal_digits (int num);
	int get_decimal_digits () { return _decimal_digits; }
	
	
	SigC::Signal1<void, float> value_changed;
	
  protected:

	void OnPaint (wxPaintEvent &ev);
	void OnSize (wxSizeEvent &ev);
	void OnMouseEvents (wxMouseEvent &ev);

	void draw_area (wxDC & dc);

	void show_text_ctrl ();
	void hide_text_ctrl ();
	void on_text_event (wxCommandEvent &ev);
	
	void update_value_str();

	wxString get_precise_value_str();
	
	int _width, _height;
	wxBitmap * _backing_store;
	wxMemoryDC _memdc;

	
	wxColour _bgcolor;
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
	float _lower_bound, _upper_bound;

	wxString _value_str;
	wxString _label_str;
	wxString _units_str;

	BarStyle _bar_style;

	class HidingTextCtrl : public wxTextCtrl {
	   public:
		HidingTextCtrl (wxWindow* par, wxWindowID id, const wxString & value = "", const wxPoint & pos = wxDefaultPosition,
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
	
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


};


#endif
