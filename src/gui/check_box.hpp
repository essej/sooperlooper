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

#ifndef __sooperlooper_gui_check_box__
#define __sooperlooper_gui_check_box__


#include <wx/wx.h>

#include <vector>
#include <sigc++/sigc++.h>


namespace SooperLooperGui {


class CheckBox
	: public wxWindow
{
  public:
	
	// ctor(s)
	CheckBox(wxWindow * parent, wxWindowID id=-1, const wxString & label = wxT(""), bool bindable=false,
		  const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
	virtual ~CheckBox();

	virtual bool SetFont(const wxFont & fnt);
	
	void set_value (bool val);
	bool get_value () { return _value; }
	
	void set_label (const wxString &label);
	wxString get_label () { return _label_str; }

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
	
	
	sigc::signal1<void, bool> value_changed;

	sigc::signal0<void> bind_request;
	
  protected:

	void OnPaint (wxPaintEvent &ev);
	void OnSize (wxSizeEvent &ev);
	void OnMouseEvents (wxMouseEvent &ev);
	void OnFocusEvent (wxFocusEvent &ev);
	void on_menu_events (wxCommandEvent &ev);
	
	void draw_area (wxDC & dc);
	void update_size();

	int _width, _height;
	wxBitmap * _backing_store;
	wxMemoryDC _memdc;
	
	wxString _label_str;
	wxColour _bgcolor;
	wxBrush  _bgbrush;

	wxColour _barcolor;
	wxColour _overbarcolor;
	wxBrush  _barbrush;
	wxColour _bordercolor;
	wxColour _bgbordercolor;
	wxBrush  _borderbrush;
	wxPen    _borderpen;  
	wxBrush    _valuebrush;  
	
	wxColour _textcolor;
	wxColour _pressbarcolor;
	wxColour _valuecolor;
	
	int   _boxsize;
	bool _value;

	wxMenu * _popup_menu;
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


};


#endif
