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
	ChoiceBox(wxWindow * parent, wxWindowID id=-1,
		  const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
	virtual ~ChoiceBox();

	typedef std::vector<wxString> ChoiceList;
	
	void append_choice (const wxString & val);
	void clear_choices ();
	void get_choices (ChoiceList & clist);
	
	
	void set_label (const wxString &label);
	wxString get_label () { return _label_str; }

	void set_string_value (const wxString &val);
	wxString get_string_value ();
	
	void set_index_value (int ind);
        int get_index_value () { return _curr_index; }
	
 	void set_bg_color (const wxColour & col);
	wxColour & get_bg_color () { return _bgcolor; }

 	void set_label_color (const wxColour & col);
	wxColour & get_label_color () { return _textcolor; }

 	void set_value_color (const wxColour & col);
	wxColour & get_value_color () { return _valuecolor; }
	
 	void set_border_color (const wxColour & col);
	wxColour & get_border_color () { return _bordercolor; }
	
	
	SigC::Signal2<void, int, wxString> value_changed;
	
  protected:

	void OnPaint (wxPaintEvent &ev);
	void OnSize (wxSizeEvent &ev);
	void OnMouseEvents (wxMouseEvent &ev);

	void draw_area (wxDC & dc);
	

	void update_value_str();
	
	int _width, _height;
	wxBitmap * _backing_store;

	
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

	int _curr_index;
	wxString _value_str;
	wxString _label_str;

	ChoiceList _choices;
	
	bool _dragging;
	int _last_x;
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


};


#endif
