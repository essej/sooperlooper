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

#ifndef __sooperlooper_gui_choice_box__
#define __sooperlooper_gui_choice_box__


#include <wx/wx.h>

#include <vector>
#include <sigc++/sigc++.h>


namespace SooperLooperGui {


class ChoiceBox
	: public wxWindow
{
  public:
	
	// ctor(s)
	ChoiceBox(wxWindow * parent, wxWindowID id=-1, bool bindable=false,
		  const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
	virtual ~ChoiceBox();

	typedef std::pair<wxString, long> ChoiceItem;
	
	typedef std::vector<ChoiceItem> ChoiceList;
	
	void append_choice (const wxString & item, long data=0);
	void clear_choices ();
	void get_choices (ChoiceList & clist);
	
	virtual bool SetFont(const wxFont & fnt);
	
	void set_label (const wxString &label);
	wxString get_label () { return _label_str; }

	void set_string_value (const wxString &val);
	wxString get_string_value ();
	long get_data_value ();
	
	void set_index_value (int ind);
        int get_index_value () { return _curr_index; }
	
 	void set_bg_color (const wxColour & col);
	wxColour & get_bg_color () { return _bgcolor; }

 	void set_bg_border_color (const wxColour & col);
	wxColour & get_bg_border_color () { return _bgbordercolor; }
	
 	void set_label_color (const wxColour & col);
	wxColour & get_label_color () { return _textcolor; }

 	void set_value_color (const wxColour & col);
	wxColour & get_value_color () { return _valuecolor; }
	
 	void set_border_color (const wxColour & col);
	wxColour & get_border_color () { return _bordercolor; }
	
    void set_use_mousewheel (bool flag) { _use_wheel = flag; _override_def_use_wheel = true; }
    bool get_use_mousewheel () const { return _use_wheel; }
    
    static void set_use_mousewheel_default (bool flag) { s_use_wheel_def = flag; }
    static bool get_use_mousewheel_default () { return s_use_wheel_def; }

    
	sigc::signal2<void, int, wxString> value_changed;
	sigc::signal0<void> bind_request;
	
  protected:

	void OnPaint (wxPaintEvent &ev);
	void OnSize (wxSizeEvent &ev);
	void OnMouseEvents (wxMouseEvent &ev);
	void OnFocusEvent (wxFocusEvent &ev);
	
	void draw_area (wxDC & dc);

	void ensure_popup (bool force_build=false);
	void on_menu_item (wxCommandEvent &ev);

	void update_size();

	void update_value_str();
	
	int _width, _height;
	wxBitmap * _backing_store;
	wxMemoryDC _memdc;
	
	wxColour _bgcolor;
	wxBrush  _bgbrush;

	wxColour _barcolor;
	wxColour _overbarcolor;
	wxBrush  _barbrush;
	wxColour _bordercolor;
	wxColour _bgbordercolor;
	wxBrush  _borderbrush;
	wxPen    _borderpen;  
	wxBrush    _linebrush;  
	
	wxColour _textcolor;
	wxColour _valuecolor;
	
	float _value;
	float _lower_bound, _upper_bound;

	int _curr_index;
	wxString _value_str;
	wxString _label_str;
	long  _data_value;

     bool _override_def_use_wheel;
    bool _use_wheel;
    static bool s_use_wheel_def;
    
	ChoiceList _choices;
	wxMenu * _popup_menu;
	bool _bindable;

	bool _dragging;
	int _last_x;
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


};


#endif
