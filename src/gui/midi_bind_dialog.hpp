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

#ifndef __sooperlooper_midi_binding_dialog__
#define __sooperlooper_midi_binding_dialog__


#include <wx/wx.h>
#include <wx/listbase.h>

#include <string>
#include <vector>
#include <sigc++/object.h>

#include <midi_bind.hpp>

class wxListCtrl;


namespace SooperLooperGui {

class GuiFrame;
class KeyboardTarget;
	
class MidiBindDialog
	: public wxFrame,  public SigC::Object
{
  public:
	
	// ctor(s)
	MidiBindDialog(GuiFrame * parent, wxWindowID id, const wxString& title,
		   const wxPoint& pos = wxDefaultPosition,
		   const wxSize& size = wxSize(400,600),
		   long style = wxDEFAULT_FRAME_STYLE,
		   const wxString& name = wxT("MidiBindingsDialog"));

	virtual ~MidiBindDialog();

	void refresh_state();


   protected:

	void init();

	void item_activated (wxListEvent & ev);
	void on_close (wxCloseEvent &ev);
	void on_button (wxCommandEvent &ev);
	void learning_stopped ();

	void update_entry_area();
	
	void onSize(wxSizeEvent &ev);
	void onPaint(wxPaintEvent &ev);
	
	
	wxListCtrl * _listctrl;
	wxButton * _learn_button;

	wxComboBox * _control_combo;
	wxSpinCtrl * _loopnum_spin;
	wxSpinCtrl * _chan_spin;
	wxComboBox * _type_combo;
	wxSpinCtrl * _param_spin;
	wxTextCtrl * _lbound_ctrl;
	wxTextCtrl * _ubound_ctrl;
	
	GuiFrame * _parent;
	bool       _justResized;

	SooperLooper::MidiBindings::BindingList _bind_list;
	
  private:
    // any class wishing to process wxWindows events must use this macro
    DECLARE_EVENT_TABLE()
	
};

};

#endif
