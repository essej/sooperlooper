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

namespace SooperLooper {
	class MidiBindInfo;
};


namespace SooperLooperGui {

class PixButton;
class LoopControl;
class TimePanel;
class SliderBar;
class ChoiceBox;
class CheckBox;
class MainPanel;
class SpinBox;
	
class LooperPanel
	: public wxPanel, public sigc::trackable
{
  public:
	
	// ctor(s)
	LooperPanel (MainPanel * mainpan, LoopControl * control, wxWindow * parent, wxWindowID id=-1,  const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
	virtual ~LooperPanel();

	void set_index(int ind);
	int get_index() { return _index; }

	void update_controls();

	void set_selected (bool flag);

  protected:

	void init();
	void post_init();
	
	void create_buttons();
	
	void slider_events(float val, int id);
	void control_bind_events(int id);

	void pan_events(float val, int chan);
	void pan_bind_events(int chan);
	
	void check_events(bool val, wxString  which); 
	
	void post_control_event (wxString ctrl, float val);
	
	void update_state();
	void bind_events();

	void pressed_events (int button, wxString cmd);
	void released_events (int button, wxString cmd);
	void scratch_events (wxString cmd);
	void clicked_events (int button, wxString cmd);
	void button_bind_events (wxString cmd);
	void delay_button_press_event (int button);
	void delay_button_release_event (int button);
	void rate_button_event(int button, float rate);
	void rate_bind_events (float rate);
	void update_rate_buttons (float rate);
	
	void on_quantize_change (int index, wxString strval);

	void on_flash_timer (wxTimerEvent &ev);
	
	void got_binding_changed(SooperLooper::MidiBindInfo & info);
	void start_learning(SooperLooper::MidiBindInfo & info);
	void got_learn_canceled();
	
	wxPanel   * _leftSelbar;
	wxPanel   * _rightSelbar;
	wxPanel   * _topSelbar;
	wxPanel   * _bottomSelbar;

	wxColour   _bgcolor;
	wxColour   _selbgcolor;
	wxColour   _learnbgcolor;
	
	PixButton * _undo_button;
	PixButton * _redo_button;
	PixButton * _record_button;
	PixButton * _overdub_button;
	PixButton * _multiply_button;
	PixButton * _replace_button;
	PixButton * _delay_button;
	PixButton * _insert_button;
	PixButton * _reverse_button;
	PixButton * _substitute_button;
	PixButton * _mute_button;
	PixButton * _pause_button;
	PixButton * _solo_button;
	PixButton * _scratch_button;
	PixButton * _load_button;
	PixButton * _save_button;
	PixButton * _once_button;
	PixButton * _trig_button;
	PixButton * _1x_button;
	PixButton * _halfx_button;
	PixButton * _2x_button;


	SliderBar * _thresh_control;
	SliderBar * _feedback_control;
	SliderBar * _in_gain_control;
	SliderBar * _dry_control;
	SliderBar * _wet_control;
	SliderBar * _scratch_control;
	SliderBar * _rate_control;
	SliderBar * _stretch_control;
	SliderBar * _pitch_control;

	CheckBox * _quantize_check;
	CheckBox * _round_check;
	CheckBox * _sync_check;
	CheckBox * _play_feed_check;
	CheckBox * _play_sync_check;
	CheckBox * _tempo_stretch_check;
	CheckBox * _prefader_check;

	wxBoxSizer * _maininsizer;
	CheckBox * _use_main_in_check;

	ChoiceBox  * _quantize_choice;

	SpinBox * _inlatency_spin;
	SpinBox * _outlatency_spin;
	SpinBox * _triglatency_spin;

	
	wxBoxSizer * _toppansizer;
	wxBoxSizer * _botpansizer;
	SliderBar  ** _panners;
	
	wxStaticText *_index_text;
	
	TimePanel * _time_panel;
	
	LoopControl * _loop_control;
	MainPanel  * _mainpanel;
	
	int _index;
	int _chan_count;
	
	SooperLooper::LooperState _last_state;
	float _tap_val;

	bool _learning;
	bool _scratch_pressed;
	bool _has_discrete_io;
	bool _waiting;

	PixButton * _flashing_button;
	wxTimer * _flash_timer;
	
private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};


};

#endif
