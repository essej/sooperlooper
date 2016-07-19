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

#ifndef __sooperlooper_midi_binding_panel__
#define __sooperlooper_midi_binding_panel__


#include <wx/wx.h>
#include <wx/listbase.h>

#include <string>
#include <vector>
#include <sigc++/trackable.h>
#include <list>

#include <midi_bind.hpp>

class wxListCtrl;
class wxSpinCtrl;

namespace SooperLooperGui {

class MainPanel;
class KeyboardTarget;
	
class MidiBindPanel
	: public wxPanel,  public sigc::trackable
{
  public:
	
	// ctor(s)
	MidiBindPanel(MainPanel * mainpan, wxWindow *parent, wxWindowID id,
		   const wxPoint& pos = wxDefaultPosition,
		   const wxSize& size = wxSize(400,600),
		   long style = wxDEFAULT_FRAME_STYLE,
		   const wxString& name = wxT("MidiBindingsPanel"));

	virtual ~MidiBindPanel();

	void refresh_state();


   protected:

	void init();
	void populate_controls();
	
	void item_selected (wxListEvent & ev);
	void on_button (wxCommandEvent &ev);
	void on_combo (wxCommandEvent &ev);

	void learning_stopped ();

	void got_binding_changed(SooperLooper::MidiBindInfo & info);
	void recvd_next_midi(SooperLooper::MidiBindInfo & info);
	void cancelled_next_midi();
	
	void update_entry_area(SooperLooper::MidiBindInfo * usethis=0);
	void update_curr_binding();
	
	void onSize(wxSizeEvent &ev);
	void onPaint(wxPaintEvent &ev);
	
	
	wxListCtrl * _listctrl;
	wxButton * _learn_button;

	wxPanel *    _edit_panel;
	wxChoice *   _control_combo;
	wxChoice *   _loopnum_combo;
	wxSpinCtrl * _chan_spin;
	wxChoice *   _type_combo;
	wxSpinCtrl * _param_spin;
	wxPanel   *  _range_panel;
	wxTextCtrl * _lbound_ctrl;
	wxTextCtrl * _ubound_ctrl;
	wxPanel   *  _data_range_panel;
	wxTextCtrl * _data_min_ctrl;
	wxTextCtrl * _data_max_ctrl;
	wxChoice *   _style_combo;
	wxCheckBox * _sus_check;

	wxCheckBox * _append_check;
	


	MainPanel * _parent;
	bool       _justResized;
	bool       _learning;
	
	SooperLooper::MidiBindings::BindingList _bind_list;
	std::list<std::string> _cmdlist;
	std::list<std::string> _ctrlist;
	
	SooperLooper::MidiBindInfo _currinfo;
	int     _selitem;
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};

};

#endif
