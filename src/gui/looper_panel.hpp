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

#ifndef __sooperlooper_gui_looper_panel__
#define __sooperlooper_gui_looper_panel__


#include <wx/wx.h>


#include <sigc++/sigc++.h>

#include "loop_control.hpp"


namespace SooperLooperGui {

class PixButton;
class LoopControl;
class TimePanel;
class SliderBar;
	
class LooperPanel
	: public wxPanel, public SigC::Object
{
  public:
	
	// ctor(s)
	LooperPanel (LoopControl * control, wxWindow * parent, wxWindowID id=-1,  const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
	virtual ~LooperPanel();

	void set_index(int ind);
	int get_index() { return _index; }

	void update_controls();
	
  protected:

	void init();

	bool load_bitmaps (PixButton * butt, wxString namebase);
	wxString get_pixmap_path (const wxString & namebase);

	void slider_events(float val, int id);
	void check_events(wxCommandEvent &ev);
	
	void post_control_event (wxString ctrl, float val);
	
	void update_state();
	void bind_events();

	void pressed_events (wxString cmd);
	void released_events (wxString cmd);
	void clicked_events (wxString cmd);
	void tap_button_event ();
	void rate_button_event();
	
	PixButton * _undo_button;
	PixButton * _redo_button;
	PixButton * _record_button;
	PixButton * _overdub_button;
	PixButton * _multiply_button;
	PixButton * _replace_button;
	PixButton * _tap_button;
	PixButton * _insert_button;
	PixButton * _reverse_button;
	PixButton * _rate_button;
	PixButton * _mute_button;
	PixButton * _scratch_button;
	PixButton * _load_button;
	PixButton * _save_button;


	SliderBar * _thresh_control;
	SliderBar * _feedback_control;
	SliderBar * _dry_control;
	SliderBar * _wet_control;
	SliderBar * _scratch_control;
	SliderBar * _rate_control;

	wxCheckBox * _quantize_check;
	wxCheckBox * _round_check;

	wxStaticText *_index_text;
	
	TimePanel * _time_panel;
	
	LoopControl * _loop_control;

	int _index;

	LooperState _last_state;
	float _tap_val;
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


};

#endif
