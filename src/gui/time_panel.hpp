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

#ifndef __sooperlooper_gui_time_panel__
#define __sooperlooper_gui_time_panel__


#include <wx/wx.h>


#include <sigc++/sigc++.h>


namespace SooperLooperGui {

class LoopControl;
	
class TimePanel
	: public wxPanel, public sigc::trackable
{
  public:
	
	// ctor(s)
	TimePanel (LoopControl * control, wxWindow * parent, wxWindowID id=-1,  const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(210, 60));
	virtual ~TimePanel();

	void set_index(int ind) { _index = ind; }
	int get_index() { return _index; }


	void OnSize (wxSizeEvent &ev);
	void OnPaint (wxPaintEvent &ev);

	bool update_time();
	
  protected:

	void init();
	
	void format_time(wxString & timestr, float val);
	void draw_area();
	void do_redraw();
	
	void update_cyc();

	void normalize_font_size(wxFont & fnt, int width, int height, wxString fitstr);

	void update_size();
	void calc_text_extents();

	void draw_other();
	void draw_pos();
	void draw_state();
	void draw_cycle();
	
	LoopControl * _loop_control;

	int _index;

	wxString _pos_str;
	wxString _tot_str;
	wxString _cyc_str;
	wxString _cyc_curr_str;
	wxString _cyc_cnt_str;
	wxString _state_str;
	wxString _rem_str;
	wxString _mem_str;
	bool     _waiting;
	
	wxFont  _pos_font;
	wxColour _pos_color;
	wxFont  _cyc_font;
	wxColour _cyc_color;

	wxFont  _state_font;
	wxColour _state_color;

	wxFont  _time_font;
	wxColour _time_color;

	wxFont  _legend_font;
	wxColour _legend_color;

	wxColour _bgcolor;
	wxBrush  _bgbrush;
	
	wxBrush  _transbrush;

	int _width, _height;
	wxBitmap * _backing_store;

 	wxMemoryDC _memdc;
	// i have to use all these to workaround a memory leak in wxGTK-2.4 against gtk2
	wxBitmap * _pos_bm;
	wxMemoryDC _posdc;
	wxBitmap * _state_bm;
	wxMemoryDC _statedc;
	wxBitmap * _wait_bm;
	wxMemoryDC _waitdc;
	wxBitmap * _other_bm;
	wxMemoryDC _otherdc;
	wxBitmap * _cyc_bm;
	wxMemoryDC _cycdc;

	wxCoord _sw, _sh, _tw, _th, _pw, _ph, _mh, _mw, _ww, _wh;

	int _curr_ccnt, _curr_currc;
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


};

#endif
