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
#include <wx/sysopt.h>

#include <iostream>
#include <string>
#include <vector>

#include "main_panel.hpp"
#include "keys_panel.hpp"
#include "keyboard_target.hpp"

using namespace SooperLooperGui;
using namespace std;

enum {
	ID_ListCtrl = 8000,
	ID_LearnButton

};

BEGIN_EVENT_TABLE(SooperLooperGui::KeysPanel, wxPanel)
	EVT_LIST_ITEM_ACTIVATED (ID_ListCtrl, SooperLooperGui::KeysPanel::item_activated)

	EVT_BUTTON (ID_LearnButton, SooperLooperGui::KeysPanel::on_button)

	EVT_SIZE (SooperLooperGui::KeysPanel::onSize)
	EVT_PAINT (SooperLooperGui::KeysPanel::onPaint)
	
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
KeysPanel::KeysPanel(MainPanel * mainpan, wxWindow * parent, wxWindowID id,
		       const wxPoint& pos,
		       const wxSize& size,
		       long style ,
		       const wxString& name)

	: wxPanel (parent, id, pos, size, style, name), _parent (mainpan)
{
	_justResized = false;
	init();
}

KeysPanel::~KeysPanel()
{

}

void KeysPanel::onSize(wxSizeEvent &ev)
{

	_justResized = true;
	ev.Skip();
}

void KeysPanel::onPaint(wxPaintEvent &ev)
{
	if (_justResized) {
		int width,height, cwidth;

		_justResized = false;
		
		_listctrl->GetClientSize(&width, &height);

		cwidth = _listctrl->GetColumnWidth(0);
		_listctrl->SetColumnWidth(1, width-cwidth);
		
	}

	ev.Skip();
}


void KeysPanel::init()
{
	wxBoxSizer * topsizer = new wxBoxSizer(wxVERTICAL);

	wxSystemOptions::SetOption(wxT("mac.listctrl.always_use_generic"), 1);

	_listctrl = new wxListCtrl(this, ID_ListCtrl, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxLC_SINGLE_SEL|wxSUNKEN_BORDER);
	_listctrl->InsertColumn(0, wxT("Command"));
	_listctrl->InsertColumn(1, wxT("Key Binding"));

	_listctrl->SetColumnWidth(0, 120);
	
	topsizer->Add (_listctrl, 1, wxEXPAND|wxALL, 4);

	wxBoxSizer * buttsizer = new wxBoxSizer(wxHORIZONTAL);
	
	_learn_button = new wxButton (this, ID_LearnButton, wxT("Learn Selected"), wxDefaultPosition, wxSize(120, -1));
	buttsizer->Add (_learn_button, 1, wxALL, 3);

	//buttsizer->Add (1,1, 1, wxALL, 0);
	topsizer->Add (buttsizer, 0, wxEXPAND|wxALL, 1);


	_parent->get_keyboard().LearningStopped.connect (mem_fun (*this, &KeysPanel::learning_stopped));
	
	refresh_state();
	
	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( topsizer );      // actually set the sizer
	//topsizer->Fit( this );            // set size to minimum size as calculated by the sizer
	//topsizer->SetSizeHints( this );   // set size hints to honour mininum size
}

void KeysPanel::refresh_state()
{
	int selitem = (int) _listctrl->GetNextItem(-1, wxLIST_NEXT_ALL,  wxLIST_STATE_SELECTED);

	_listctrl->DeleteAllItems();

	wxListItem item;
	item.SetMask (wxLIST_MASK_TEXT|wxLIST_MASK_DATA);
	item.SetColumn(0);
	//item.SetWidth(wxLIST_AUTOSIZE);
	
	
	KeyboardTarget::ActionNameList alist;
	KeyboardTarget & keyb = _parent->get_keyboard();
	keyb.get_action_names (alist);

	int itemid = 0;
	
	for (KeyboardTarget::ActionNameList::iterator actname = alist.begin(); actname != alist.end(); ++actname)
	{
		item.SetId(itemid++);
		item.SetColumn(0);
		item.SetText (wxString::FromAscii (actname->c_str()));
		//item.SetData ((unsigned) );
		_listctrl->InsertItem (item);

		item.SetColumn(1);
		item.SetText (wxString::FromAscii (keyb.get_binding (*actname).c_str()));
		if (item.GetText() == wxT(" ")) {
			item.SetText (wxT("SPACE"));
		}
		_listctrl->SetItem (item);
		
	}

	_listctrl->SortItems (list_sort_callback, (long) _listctrl);

	if (selitem >= 0) {
		_listctrl->EnsureVisible(selitem);
		_listctrl->SetItemState (selitem, wxLIST_STATE_SELECTED, wxLIST_MASK_STATE);
	}
}



void KeysPanel::on_button (wxCommandEvent &ev)
{
	if (ev.GetId() == ID_LearnButton) {
		if (_parent->get_keyboard().is_learning()) {
			// cancel learn
			_parent->get_keyboard().stop_learning (true);
			_parent->get_keyboard().set_enabled(false);
		}
		else {
			// start learn selected
			int item = (int) _listctrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

			if (item > -1) {
				_learn_button->SetLabel (wxT("Cancel Learn"));
				_learn_button->SetForegroundColour (*wxRED);
				
				_parent->get_keyboard().set_enabled(true);
				_parent->get_keyboard().start_learning (string((const char *)_listctrl->GetItemText(item).ToAscii()));
				//cerr << "start learning" << endl;
			}
		}
	}
	else {
		ev.Skip();
	}
}

void KeysPanel::item_activated (wxListEvent & ev)
{
	// cerr << "item " << ev.GetText() << " activated" << endl;

	KeyboardTarget & keyb = _parent->get_keyboard();

	_learn_button->SetLabel (wxT("Cancel Learn"));
	_learn_button->SetForegroundColour (*wxRED);

	keyb.set_enabled(true);
	keyb.start_learning ((const char *)ev.GetText().ToAscii());

}

void KeysPanel::learning_stopped ()
{
	// cerr << "learning stopped " << endl;
	_learn_button->SetLabel (wxT("Learn Selected"));
	_learn_button->SetForegroundColour (*wxBLACK);

	KeyboardTarget & keyb = _parent->get_keyboard();
	keyb.set_enabled(false);
	
	refresh_state();

	_listctrl->SetFocus();
}
	
	
