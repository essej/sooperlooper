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

#include <wx/wx.h>
#include <wx/listctrl.h>

#include <iostream>
#include <string>
#include <vector>

#include "gui_frame.hpp"
#include "midi_bind_dialog.hpp"
#include "loop_control.hpp"
#include <midi_bind.hpp>

using namespace SooperLooperGui;
using namespace SooperLooper;
using namespace std;

enum {
	ID_ListCtrl = 8000,
	ID_CloseButton,
	ID_LearnButton

};

BEGIN_EVENT_TABLE(SooperLooperGui::MidiBindDialog, wxFrame)
	EVT_CLOSE(SooperLooperGui::MidiBindDialog::on_close)
	EVT_LIST_ITEM_ACTIVATED (ID_ListCtrl, SooperLooperGui::MidiBindDialog::item_activated)

	EVT_BUTTON (ID_CloseButton, SooperLooperGui::MidiBindDialog::on_button)
	EVT_BUTTON (ID_LearnButton, SooperLooperGui::MidiBindDialog::on_button)

	EVT_SIZE (SooperLooperGui::MidiBindDialog::onSize)
	EVT_PAINT (SooperLooperGui::MidiBindDialog::onPaint)
	
END_EVENT_TABLE()

static int wxCALLBACK list_sort_callback (long item1, long item2, long sortData)
{
	wxListCtrl * lctrl = (wxListCtrl * )sortData;
	wxListItem i1, i2;
	i1.SetId(item1);
	i2.SetId(item2);
	
	lctrl->GetItem(i1);
	lctrl->GetItem(i2);
	
	return i1.GetText().compare(i2.GetText());
}

	
// ctor(s)
MidiBindDialog::MidiBindDialog(GuiFrame * parent, wxWindowID id, const wxString& title,
		       const wxPoint& pos,
		       const wxSize& size,
		       long style ,
		       const wxString& name)

	: wxFrame ((wxWindow *)parent, id, title, pos, size, style, name), _parent (parent)
{
	_justResized = false;
	init();
}

MidiBindDialog::~MidiBindDialog()
{

}

void MidiBindDialog::onSize(wxSizeEvent &ev)
{

	_justResized = true;
	ev.Skip();
}

void MidiBindDialog::onPaint(wxPaintEvent &ev)
{
	if (_justResized) {
		int width,height, cwidth;

		_justResized = false;
		
		_listctrl->GetClientSize(&width, &height);

		cwidth = _listctrl->GetColumnWidth(0);
		_listctrl->SetColumnWidth(3, width-cwidth);
		
	}

	ev.Skip();
}


void MidiBindDialog::init()
{
	wxBoxSizer * topsizer = new wxBoxSizer(wxVERTICAL);

	_listctrl = new wxListCtrl(this, ID_ListCtrl, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxLC_SINGLE_SEL|wxSUNKEN_BORDER);
	_listctrl->InsertColumn(0, wxT("Control"));
	_listctrl->InsertColumn(1, wxT("Loop#"));
	_listctrl->InsertColumn(2, wxT("Midi Event"));
	_listctrl->InsertColumn(3, wxT("Range"));

	topsizer->Add (_listctrl, 1, wxEXPAND|wxALL, 4);

	wxPanel * editpanel = new wxPanel(this, -1);
	wxBoxSizer * editsizer = new wxBoxSizer(wxHORIZONTAL);

	_learn_button = new wxButton (editpanel, ID_LearnButton, wxT("Learn Selected"), wxDefaultPosition, wxSize(120, -1));
	editsizer->Add (_learn_button, 1, wxALL, 3);

	//buttsizer->Add (1,1, 1, wxALL, 0);

	wxButton * butt = new wxButton (editpanel, ID_CloseButton, wxT("Close"));
	editsizer->Add (butt, 0, wxALL|wxALIGN_CENTRE, 3);


	editpanel->SetAutoLayout( true );     // tell dialog to use sizer
	editpanel->SetSizer( editsizer );      // actually set the sizer
	editsizer->SetSizeHints( editpanel );   // set size hints to honour mininum size
	editsizer->Fit(editpanel);
	
	topsizer->Add (editpanel, 0, wxEXPAND|wxALL, 1);


	refresh_state();
	
	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( topsizer );      // actually set the sizer
	topsizer->Fit( this );            // set size to minimum size as calculated by the sizer
	topsizer->SetSizeHints( this );   // set size hints to honour mininum size
}

void MidiBindDialog::refresh_state()
{
	_listctrl->DeleteAllItems();

	wxListItem item;
	item.SetMask (wxLIST_MASK_TEXT|wxLIST_MASK_DATA);
	item.SetColumn(0);
	item.SetWidth(wxLIST_AUTOSIZE);

	_bind_list.clear();

	_parent->get_loop_control().midi_bindings().get_bindings(_bind_list);
	
	int itemid = 0;
	
	for (MidiBindings::BindingList::iterator biter = _bind_list.begin(); biter != _bind_list.end(); ++biter)
	{
		MidiBindInfo & info = (*biter);

		// control
		item.SetId(itemid++);
		item.SetColumn(0);
		item.SetData((void *) &info);
		item.SetText (wxString::FromAscii (info.control.c_str()));
		_listctrl->InsertItem (item);

		// loop #
		item.SetColumn(1);
		item.SetText (wxString::Format(wxT("%d"), info.instance + 1));
		if (info.instance < 0) {
			item.SetText (wxT("All"));
		}
		_listctrl->SetItem (item);

		// midi event
		item.SetColumn(2);
		item.SetText (wxString::Format(wxT("Ch%d - %s - %d"), info.channel, info.type.c_str(), info.param));
		_listctrl->SetItem (item);

		// range
		item.SetColumn(3);
		if (info.command == "set") {
			item.SetText (wxString::Format(wxT("%g - %g %s"), info.lbound, info.ubound, info.style == MidiBindInfo::GainStyle ? wxT("gain") : wxT("")));
		}
		else {
			item.SetText(wxT(""));
		}
		_listctrl->SetItem (item);
		
		
	}

	_listctrl->SortItems (list_sort_callback, (unsigned) _listctrl);
	
}


void MidiBindDialog::update_entry_area()
{


}


void MidiBindDialog::on_button (wxCommandEvent &ev)
{
	if (ev.GetId() == ID_CloseButton) {
		Show(false);
	}
	else if (ev.GetId() == ID_LearnButton) {
// 		if (_parent->get_keyboard().is_learning()) {
// 			// cancel learn
// 			_parent->get_keyboard().stop_learning (true);
// 		}
// 		else {
// 			// start learn selected
// 			int item = _listctrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

// 			if (item > -1) {
// 				_learn_button->SetLabel (wxT("Cancel Learn"));
// 				_learn_button->SetForegroundColour (*wxRED);
				
			
// 				_parent->get_keyboard().start_learning (string(_listctrl->GetItemText(item).c_str()));
// 				//cerr << "start learning" << endl;
// 			}
// 		}
	}
	else {
		ev.Skip();
	}
}
				   
void MidiBindDialog::on_close (wxCloseEvent &ev)
{
	if (!ev.CanVeto()) {
		
		Destroy();
	}
	else {
		ev.Veto();
		
		Show(false);
	}
}


void MidiBindDialog::item_activated (wxListEvent & ev)
{
	// cerr << "item " << ev.GetText() << " activated" << endl;

	//KeyboardTarget & keyb = _parent->get_keyboard();

	//_learn_button->SetLabel (wxT("Cancel Learn"));
	//_learn_button->SetForegroundColour (*wxRED);

	//keyb.start_learning (ev.GetText().c_str());

}

void MidiBindDialog::learning_stopped ()
{
	// cerr << "learning stopped " << endl;
	//_learn_button->SetLabel (wxT("Learn Selected"));
	//_learn_button->SetForegroundColour (*wxBLACK);

	//refresh_state();

	//_listctrl->SetFocus();
}
	
	
