/*
** Copyright (C) 2006 Jesse Chappell <jesse@essej.net>
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

#ifndef __sooperlooper_latency_panel__
#define __sooperlooper_latency_panel__


#include <wx/wx.h>
#include <wx/listbase.h>

#include <string>
#include <vector>
#include <sigc++/trackable.h>

class wxListCtrl;

namespace SooperLooperGui {

class MainPanel;
class SpinBox;
	
class LatencyPanel
	: public wxPanel,  public sigc::trackable
{
  public:
	
	// ctor(s)
	LatencyPanel(MainPanel * guiframe, wxWindow * parent, wxWindowID id,
		   const wxPoint& pos = wxDefaultPosition,
		   const wxSize& size = wxSize(400,600),
		   long style = wxDEFAULT_FRAME_STYLE,
		   const wxString& name = wxT("LatencyMiscPanel"));

	virtual ~LatencyPanel();

	void refresh_state();
	

   protected:

	void init();

	void on_check (wxCommandEvent &ev);

	void on_spin_change (float value, int id);
	
	//void learning_stopped ();
	
	void onSize(wxSizeEvent &ev);
	void onPaint(wxPaintEvent &ev);
	void OnUpdateTimer(wxTimerEvent &ev);
	
	
	wxCheckBox  * _auto_check;
	wxCheckBox  * _auto_disable_check;
	SpinBox     * _input_spin;
	SpinBox     * _output_spin;
	
	wxCheckBox  * _round_tempo_integer_check;
	wxCheckBox  * _jack_timebase_master_check;

	wxCheckBox  * _use_midi_start_check;
	wxCheckBox  * _use_midi_stop_check;
	wxCheckBox  * _send_midi_start_on_trigger_check;
	wxCheckBox * _output_clock_check;

	wxCheckBox * _slider_mousewheel_check;


    MainPanel * _parent;
	bool       _justResized;

	wxTimer   * _update_timer;
	bool        _do_request;
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};

};

#endif
