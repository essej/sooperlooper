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

#ifndef __sooperlooper_gui_pix_button__
#define __sooperlooper_gui_pix_button__


#include <wx/wx.h>


#include <sigc++/sigc++.h>


namespace SooperLooperGui {


class PixButton
	: public wxWindow
{
  public:
	
	// ctor(s)
	PixButton(wxWindow * parent, wxWindowID id=-1, bool midibindable=true, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
	virtual ~PixButton();


	void set_normal_bitmap (const wxBitmap & bm);
	void set_focus_bitmap (const wxBitmap & bm);
	void set_selected_bitmap (const wxBitmap & bm);
	void set_disabled_bitmap (const wxBitmap & bm);
	void set_active_bitmap (const wxBitmap & bm);

	wxBitmap & get_normal_bitmap() { return _normal_bitmap;}
	wxBitmap & get_focus_bitmap() { return _focus_bitmap; }
	wxBitmap & get_selected_bitmap() { return _selected_bitmap; }
	wxBitmap & get_disabled_bitmap() { return _disabled_bitmap; }
	wxBitmap & get_active_bitmap() { return _active_bitmap; }


	void set_active(bool flag);
	bool get_active() { return _active; }
	
	void set_bg_color (const wxColour & col);
	wxColour & get_bg_color () { return _bgcolor; }

	enum MouseButton {
		LeftButton=1,
		MiddleButton,
		RightButton
	};

	// int argument is mouse button as above
	sigc::signal1<void,int> pressed;
	sigc::signal1<void,int> released;
	sigc::signal1<void,int> clicked;
	
	sigc::signal0<void> enter;
	sigc::signal0<void> leave;

	sigc::signal0<void> bind_request;
	
  protected:

	enum ButtonState {
		Normal,
		Selected,
		Disabled
	};

	enum EnterState {
		Inside,
		Outside
	};

	
	void OnPaint (wxPaintEvent &ev);
	void OnSize (wxSizeEvent &ev);
	void OnMouseEvents (wxMouseEvent &ev);
	void on_menu_events (wxCommandEvent &ev);
	
	void draw_area (wxDC & dc);
	void update_size();
	
	wxBitmap _normal_bitmap;
	wxBitmap _focus_bitmap;
	wxBitmap _selected_bitmap;
	wxBitmap _disabled_bitmap;
	wxBitmap _active_bitmap;

	ButtonState _bstate;
	EnterState _estate;
	bool _active;
	bool _pressed;
    
	int _width, _height;
	
	wxColour _bgcolor;
	wxBrush  _bgbrush;

	wxBitmap * _backing_store;
	wxMemoryDC _memdc;

	wxMenu * _popup_menu;

  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


};


#endif
