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
#include <wx/spinctrl.h>

#include <iostream>
#include <string>
#include <vector>
#include <list>

#include "main_panel.hpp"
#include "midi_bind_panel.hpp"
#include "loop_control.hpp"
#include "keyboard_target.hpp"

#include <midi_bind.hpp>
#include <command_map.hpp>

using namespace SooperLooperGui;
using namespace SooperLooper;
using namespace std;

enum {
	ID_ListCtrl = 8000,
	ID_CloseButton,
	ID_LearnButton,
	ID_ControlCombo,
	ID_LoopNumCombo,
	ID_ChanSpin,
	ID_TypeCombo,
	ID_ParamSpin,
	ID_LBoundCtrl,
	ID_UBoundCtrl,
	ID_StyleCombo,
	ID_AddButton,
	ID_ModifyButton,
	ID_RemoveButton,
	ID_ClearAllButton,
	ID_LoadButton,
	ID_SaveButton,
	ID_SusCheck,
	ID_AppendCheck,
	ID_DataMinCtrl,
	ID_DataMaxCtrl,


};

BEGIN_EVENT_TABLE(SooperLooperGui::MidiBindPanel, wxPanel)
//EVT_LIST_ITEM_ACTIVATED (ID_ListCtrl, SooperLooperGui::MidiBindPanel::list_event)
	EVT_LIST_ITEM_SELECTED (ID_ListCtrl, SooperLooperGui::MidiBindPanel::item_selected)

	EVT_BUTTON (ID_CloseButton, SooperLooperGui::MidiBindPanel::on_button)
	EVT_BUTTON (ID_LearnButton, SooperLooperGui::MidiBindPanel::on_button)
	EVT_BUTTON (ID_AddButton, SooperLooperGui::MidiBindPanel::on_button)
	EVT_BUTTON (ID_RemoveButton, SooperLooperGui::MidiBindPanel::on_button)
	EVT_BUTTON (ID_ModifyButton, SooperLooperGui::MidiBindPanel::on_button)
	EVT_BUTTON (ID_ClearAllButton, SooperLooperGui::MidiBindPanel::on_button)
	EVT_BUTTON (ID_LoadButton, SooperLooperGui::MidiBindPanel::on_button)
	EVT_BUTTON (ID_SaveButton, SooperLooperGui::MidiBindPanel::on_button)
	
	EVT_CHOICE(ID_ControlCombo, SooperLooperGui::MidiBindPanel::on_combo)
	
	EVT_SIZE (SooperLooperGui::MidiBindPanel::onSize)
	EVT_PAINT (SooperLooperGui::MidiBindPanel::onPaint)
	
END_EVENT_TABLE()

static const wxString CcString(wxT("CC"));
static const wxString CcOnString(wxT("CC On"));
static const wxString CcOffString(wxT("CC Off"));
static const wxString NoteString(wxT("Note"));
static const wxString NoteOnString(wxT("Note On"));
static const wxString NoteOffString(wxT("Note Off"));
static const wxString PcString(wxT("PC"));
static const wxString PitchBendString(wxT("Pitch Bend"));
static const wxString KeyPressureString(wxT("Key Pressure"));
static const wxString ChannelPressureString(wxT("Channel Pressure"));
	
static int wxCALLBACK list_sort_callback (long item1, long item2, long sortData)
{

	MidiBindInfo * info1 = (MidiBindInfo*)(item1);
	MidiBindInfo * info2 = (MidiBindInfo*)(item2);

	return info1->control > info2->control;
}

	
// ctor(s)
MidiBindPanel::MidiBindPanel(MainPanel * mainpan, wxWindow * parent, wxWindowID id,
		       const wxPoint& pos,
		       const wxSize& size,
		       long style ,
		       const wxString& name)

	: wxPanel ((wxWindow *)parent, id, pos, size, style, name), _parent (mainpan)
{
	_learning = false;
	_justResized = false;
	_selitem = -1;
	init();
}

MidiBindPanel::~MidiBindPanel()
{

}

void MidiBindPanel::onSize(wxSizeEvent &ev)
{

	_justResized = true;
	ev.Skip();
}

void MidiBindPanel::onPaint(wxPaintEvent &ev)
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


void MidiBindPanel::init()
{
	wxBoxSizer * topsizer = new wxBoxSizer(wxVERTICAL);

	_listctrl = new wxListCtrl(this, ID_ListCtrl, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxLC_SINGLE_SEL|wxSUNKEN_BORDER);
	_listctrl->InsertColumn(0, wxT("Control"));
	_listctrl->InsertColumn(1, wxT("Loop#"));
	_listctrl->InsertColumn(2, wxT("Midi Event"));
	_listctrl->InsertColumn(3, wxT("Range"));

	_listctrl->SetColumnWidth(0, 180);
	_listctrl->SetColumnWidth(1, 60);
	_listctrl->SetColumnWidth(2, 100);
	
	

	wxBoxSizer * buttsizer = new wxBoxSizer(wxHORIZONTAL);

	wxButton * butt = new wxButton (this, ID_SaveButton, wxT("Save..."));
	buttsizer->Add (butt, 0, wxALL|wxALIGN_CENTRE, 3);
	
	butt = new wxButton (this, ID_LoadButton, wxT("Load..."));
	buttsizer->Add (butt, 0, wxALL|wxALIGN_CENTRE, 3);

	_append_check = new wxCheckBox (this, ID_AppendCheck, wxT("load adds to existing"));
	buttsizer->Add (_append_check, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 3);

	buttsizer->Add (1,-1,1, wxALL, 0);
	
	butt = new wxButton (this, ID_ClearAllButton, wxT("Clear All"));
	buttsizer->Add (butt, 0, wxALL, 3);
	
	topsizer->Add (buttsizer, 0, wxLEFT|wxTOP|wxRIGHT|wxEXPAND, 4);

	// add list
	topsizer->Add (_listctrl, 1, wxEXPAND|wxALL, 4);


	
	_edit_panel = new wxPanel(this, -1);
	wxBoxSizer * editsizer = new wxBoxSizer(wxHORIZONTAL);

	wxStaticBox * shotBox = new wxStaticBox(_edit_panel, -1, wxT("Command/Control"), wxDefaultPosition, wxDefaultSize);
        wxStaticBoxSizer * colsizer = new wxStaticBoxSizer(shotBox, wxVERTICAL);
	
	//wxBoxSizer * colsizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer * rowsizer;
	wxStaticText * staticText;
	//colsizer->Add (staticText, 0, wxALL|wxALIGN_CENTRE, 2);

	_control_combo = new wxChoice(_edit_panel, ID_ControlCombo,  wxDefaultPosition, wxDefaultSize, 0, 0);
	_control_combo->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	//_control_combo->SetToolTip(wxT("Choose control or command"));
	populate_controls();
	//_control_combo->SetSelection(0);
	colsizer->Add (_control_combo, 0, wxALL|wxEXPAND, 2);
	_control_combo->SetWindowVariant(wxWINDOW_VARIANT_NORMAL);

	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	staticText = new wxStaticText(_edit_panel, -1, wxT("Loop #"), wxDefaultPosition, wxSize(-1, -1), wxALIGN_LEFT);
	staticText->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	rowsizer->Add (staticText, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	_loopnum_combo =  new wxChoice(_edit_panel, ID_LoopNumCombo, wxDefaultPosition, wxSize(100, -1), 0, 0);
	_loopnum_combo->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	_loopnum_combo->Append (wxT("Selected"), (void *) -2);
	_loopnum_combo->Append (wxT("Global"), (void *) -1);
	_loopnum_combo->Append (wxT("All"), (void *) 0);

	for (int i=1; i <= 16; ++i) {
		_loopnum_combo->Append (wxString::Format(wxT("%d"), i), (void *) i);
	}
	_loopnum_combo->SetSelection(2);
	rowsizer->Add (_loopnum_combo, 1, wxALL|wxALIGN_CENTRE_VERTICAL, 1);

	_sus_check = new wxCheckBox(_edit_panel, ID_SusCheck, wxT("SUS"));
	_sus_check->SetWindowVariant(wxWINDOW_VARIANT_SMALL);

	rowsizer->Add (_sus_check, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 1);
	
	colsizer->Add (rowsizer, 0, wxALL|wxEXPAND, 0);

	editsizer->Add (colsizer, 0, wxALL|wxEXPAND, 4);

	
	
	// midi event stuff
	shotBox = new wxStaticBox(_edit_panel, -1, wxT("MIDI Event"), wxDefaultPosition, wxDefaultSize);
        colsizer = new wxStaticBoxSizer(shotBox, wxVERTICAL);
	
	//staticText = new wxStaticText(_edit_panel, -1, "MIDI Event", wxDefaultPosition, wxSize(-1, -1), wxALIGN_CENTRE);
	//colsizer->Add (staticText, 0, wxALL|wxALIGN_CENTRE, 2);

	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	staticText = new wxStaticText(_edit_panel, -1, wxT("Ch#"), wxDefaultPosition, wxSize(-1, -1), wxALIGN_LEFT);
	staticText->SetWindowVariant(wxWINDOW_VARIANT_SMALL);

	rowsizer->Add (staticText, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	_chan_spin =  new wxSpinCtrl(_edit_panel, ID_ChanSpin, wxT("1"), wxDefaultPosition, wxSize(50,-1), wxSP_ARROW_KEYS, 1, 16, 1, wxT("KeyAware"));
	_chan_spin->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	rowsizer->Add (_chan_spin, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);
	
	_type_combo = new wxChoice(_edit_panel, ID_TypeCombo,  wxDefaultPosition, wxDefaultSize, 0, 0);
	_type_combo->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	//_control_combo->SetToolTip(wxT("Choose control or command"));
	_type_combo->Append (NoteString);
	_type_combo->Append (NoteOnString);
	_type_combo->Append (NoteOffString);
	_type_combo->Append (CcString);
	_type_combo->Append (CcOnString);
	_type_combo->Append (CcOffString);
	_type_combo->Append (PcString);
	_type_combo->Append (KeyPressureString);
	_type_combo->Append (ChannelPressureString);
	_type_combo->Append (PitchBendString);
	_type_combo->SetSelection(0);
	rowsizer->Add (_type_combo, 1, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	_param_spin =  new wxSpinCtrl(_edit_panel, ID_ParamSpin, wxT("0"), wxDefaultPosition, wxSize(50,-1), wxSP_ARROW_KEYS, 0, 127, 0, wxT("KeyAware"));
	_param_spin->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	rowsizer->Add (_param_spin, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	_learn_button = new wxButton (_edit_panel, ID_LearnButton, wxT("Learn"), wxDefaultPosition, wxSize(-1, -1));
	_learn_button->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	rowsizer->Add (_learn_button, 0, wxALL, 3);

	colsizer->Add (rowsizer, 0, wxALL|wxEXPAND, 0);

	_range_panel = new wxPanel(_edit_panel);
	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	staticText = new wxStaticText(_range_panel, -1, wxT("Targ Range"), wxDefaultPosition, wxSize(-1, -1), wxALIGN_LEFT);
	staticText->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	rowsizer->Add (staticText, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	_lbound_ctrl = new wxTextCtrl(_range_panel, ID_LBoundCtrl, wxT(""), wxDefaultPosition, wxSize(40, -1), 0, wxDefaultValidator, wxT("KeyAware"));
	_lbound_ctrl->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	rowsizer->Add (_lbound_ctrl, 1, wxALL|wxALIGN_CENTRE_VERTICAL, 1);

	staticText = new wxStaticText(_range_panel, -1, wxT(" to "), wxDefaultPosition, wxSize(-1, -1), wxALIGN_LEFT);
	staticText->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	rowsizer->Add (staticText, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	_ubound_ctrl = new wxTextCtrl(_range_panel, ID_UBoundCtrl, wxT(""), wxDefaultPosition, wxSize(40, -1), 0, wxDefaultValidator, wxT("KeyAware"));
	_ubound_ctrl->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	rowsizer->Add (_ubound_ctrl, 1, wxALL|wxALIGN_CENTRE_VERTICAL, 1);

	_style_combo =  new wxChoice(_range_panel, ID_StyleCombo,  wxDefaultPosition, wxSize(30, -1), 0, 0);
	_style_combo->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	_style_combo->Append (wxT("Normal"), (void *) MidiBindInfo::NormalStyle);
	_style_combo->Append (wxT("Gain"), (void *) MidiBindInfo::GainStyle);
	_style_combo->Append (wxT("Integer"), (void *) MidiBindInfo::IntegerStyle);
	_style_combo->Append (wxT("Toggle"), (void *) MidiBindInfo::ToggleStyle);
	_style_combo->SetToolTip(wxT("Choose a scaling type"));
	_style_combo->SetSelection(0);
	rowsizer->Add (_style_combo, 1, wxALL|wxALIGN_CENTRE_VERTICAL, 1);

	_range_panel->SetAutoLayout( true );     // tell dialog to use sizer
        _range_panel->SetSizer( rowsizer );      // actually set the sizer
	rowsizer->SetSizeHints( _range_panel );   // set size hints to honour mininum size
	rowsizer->Fit(_range_panel);
	
	colsizer->Add (_range_panel, 0, wxALL|wxEXPAND, 0);


	_data_range_panel = new wxPanel(_edit_panel);
	rowsizer = new wxBoxSizer(wxHORIZONTAL);
	staticText = new wxStaticText(_data_range_panel, -1, wxT("Data Range"), wxDefaultPosition, wxSize(-1, -1), wxALIGN_LEFT);
	staticText->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	rowsizer->Add (staticText, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	_data_min_ctrl = new wxTextCtrl(_data_range_panel, ID_DataMinCtrl, wxT(""), wxDefaultPosition, wxSize(40, -1), 0, wxDefaultValidator, wxT("KeyAware"));
	_data_min_ctrl->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	rowsizer->Add (_data_min_ctrl, 1, wxALL|wxALIGN_CENTRE_VERTICAL, 1);

	staticText = new wxStaticText(_data_range_panel, -1, wxT(" to "), wxDefaultPosition, wxSize(-1, -1), wxALIGN_LEFT);
	staticText->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	rowsizer->Add (staticText, 0, wxALL|wxALIGN_CENTRE_VERTICAL, 2);

	_data_max_ctrl = new wxTextCtrl(_data_range_panel, ID_DataMaxCtrl, wxT(""), wxDefaultPosition, wxSize(40, -1), 0, wxDefaultValidator, wxT("KeyAware"));
	_data_max_ctrl->SetWindowVariant(wxWINDOW_VARIANT_SMALL);
	rowsizer->Add (_data_max_ctrl, 1, wxALL|wxALIGN_CENTRE_VERTICAL, 1);

	_data_range_panel->SetAutoLayout( true );     // tell dialog to use sizer
        _data_range_panel->SetSizer( rowsizer );      // actually set the sizer
	//rowsizer->SetSizeHints( _range_panel );   // set size hints to honour mininum size
	//rowsizer->Fit(_data_range_panel);
	
	colsizer->Add (_data_range_panel, 0, wxALL|wxEXPAND, 0);

	
	editsizer->Add (colsizer, 1, wxALL|wxEXPAND, 4);

	
	
	
	_edit_panel->SetAutoLayout( true );     // tell dialog to use sizer
	_edit_panel->SetSizer( editsizer );      // actually set the sizer
	editsizer->SetSizeHints( _edit_panel );   // set size hints to honour mininum size
	editsizer->Fit(_edit_panel);
	
	topsizer->Add (_edit_panel, 0, wxEXPAND|wxALL, 1);


	buttsizer = new wxBoxSizer(wxHORIZONTAL);
	buttsizer->Add (1,1, 1, wxALL, 0);

	butt = new wxButton (this, ID_AddButton, wxT("Add New"));
	buttsizer->Add (butt, 0, wxALL|wxALIGN_CENTRE, 3);
	
	butt = new wxButton (this, ID_RemoveButton, wxT("Remove"));
	buttsizer->Add (butt, 0, wxALL|wxALIGN_CENTRE, 3);

	buttsizer->Add (10, 1, 0);
	
	butt = new wxButton (this, ID_ModifyButton, wxT("Modify"));
	buttsizer->Add (butt, 0, wxALL|wxALIGN_CENTRE, 3);


	//buttsizer->Add (1,-1,1, wxALL, 0);
	
	
	//butt = new wxButton (this, ID_CloseButton, wxT("Close"));
	//buttsizer->Add (butt, 0, wxALL|wxALIGN_CENTRE, 3);


	topsizer->Add (buttsizer, 0, wxALL|wxEXPAND, 4);


	_parent->get_loop_control().MidiBindingChanged.connect (mem_fun (*this, &MidiBindPanel::got_binding_changed));
	_parent->get_loop_control().ReceivedNextMidi.connect (mem_fun (*this, &MidiBindPanel::recvd_next_midi));
	_parent->get_loop_control().NextMidiCancelled.connect (mem_fun (*this, &MidiBindPanel::cancelled_next_midi));

	refresh_state();
	
	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( topsizer );      // actually set the sizer
	//topsizer->Fit( this );            // set size to minimum size as calculated by the sizer
	//topsizer->SetSizeHints( this );   // set size hints to honour mininum size
}

void MidiBindPanel::populate_controls()
{
	// add all known controls to combo
	CommandMap & cmap = CommandMap::instance();
	
	cmap.get_commands(_cmdlist);
	cmap.get_controls(_ctrlist);

	_cmdlist.sort();
	_ctrlist.sort();

	for (list<string>::iterator iter = _cmdlist.begin(); iter != _cmdlist.end(); ++iter)
	{
		if (cmap.is_command(*iter)) {
			_control_combo->Append (wxString::Format(wxT("[cmd]  %s"), wxString::FromAscii(iter->c_str()).c_str()), (void *) (iter->c_str()));
		}
	}
	for (list<string>::iterator iter = _ctrlist.begin(); iter != _ctrlist.end(); ++iter)
	{
		if (cmap.is_input_control(*iter)) {
			_control_combo->Append (wxString::Format(wxT("[ctrl]  %s"), wxString::FromAscii(iter->c_str()).c_str()), (void *) (iter->c_str()));
		}
	}
	for (list<string>::iterator iter = _ctrlist.begin(); iter != _ctrlist.end(); ++iter)
	{
		if (cmap.is_global_control(*iter)) {
			_control_combo->Append (wxString::Format(wxT("[g. ctrl]  %s"), wxString::FromAscii(iter->c_str()).c_str()), (void *) (iter->c_str()));
		}
	}
	
	
}

void MidiBindPanel::cancelled_next_midi()
{
	_learning = false;
	_learn_button->SetLabel (wxT("Learn"));
	_learn_button->SetForegroundColour (*wxBLACK); // todo default
}

void MidiBindPanel::recvd_next_midi(SooperLooper::MidiBindInfo & info)
{
	if (_learning) {
		//cerr << "got next: " << info.serialize() << endl;
		MidiBindInfo ninfo = _currinfo;
		ninfo.channel = info.channel;
		ninfo.param = info.param;
		ninfo.type = info.type;

		update_entry_area (&ninfo);
		
		_learn_button->SetLabel (wxT("Learn"));
		_learn_button->SetForegroundColour (*wxBLACK); // todo default
		
		_learning = false;
	}

}

void MidiBindPanel::got_binding_changed(SooperLooper::MidiBindInfo & info)
{
	// cancel learning too
	if (_learning) {
		_learn_button->SetLabel (wxT("Learn"));
		_learn_button->SetForegroundColour (*wxBLACK); // todo default
		_learning = false;
	}

	refresh_state();
}

void MidiBindPanel::refresh_state()
{
	_listctrl->DeleteAllItems();

	wxListItem item;
	item.SetMask (wxLIST_MASK_TEXT|wxLIST_MASK_DATA|wxLIST_MASK_STATE);
	item.SetColumn(0);
	item.SetWidth(wxLIST_AUTOSIZE);
	item.SetStateMask(wxLIST_STATE_SELECTED);
	item.SetState(0);

	_bind_list.clear();

	
	_parent->get_loop_control().midi_bindings().get_bindings(_bind_list);
	
	int itemid = 0;
	
	for (MidiBindings::BindingList::iterator biter = _bind_list.begin(); biter != _bind_list.end(); ++biter)
	{
		MidiBindInfo & info = (*biter);
		
		// control
		item.SetId(itemid);
		item.SetColumn(0);
		item.SetData((void *) &info);
		item.SetText (wxString::FromAscii (info.control.c_str()));

		_listctrl->InsertItem (item);

		// loop #
		item.SetColumn(1);
		item.SetText (wxString::Format(wxT("%d"), info.instance + 1));
		if (info.instance == -1) {
			item.SetText (wxT("All"));
		}
		else if (info.instance == -2) {
			item.SetText (wxT("Global"));
		}
		else if (info.instance == -3) {
			item.SetText (wxT("Selected"));
		}
		_listctrl->SetItem (item);

		// midi event
		item.SetColumn(2);
		item.SetText (wxString::Format(wxT("ch%d - %s - %d"), info.channel+1, wxString::FromAscii(info.type.c_str()).c_str(), info.param));
		_listctrl->SetItem (item);

		// range
		item.SetColumn(3);
		if (info.command == "set") {
			item.SetText (wxString::Format(wxT("%g - %g  (%d - %d)  %s"), info.lbound, info.ubound,
						       info.data_min, info.data_max, 
						       info.style == MidiBindInfo::GainStyle ? wxT("gain") : 
						       ( info.style == MidiBindInfo::ToggleStyle ? wxT("togg") : 
							 ( info.style == MidiBindInfo::IntegerStyle ? wxT("int") :  wxT("") ))));
		}
		else {
			if (info.command == "susnote") {
				item.SetText(wxT("SUS"));
			}
			else {
				item.SetText(wxT(""));
			}
		}
		_listctrl->SetItem (item);
		

		itemid++;
	}

	_listctrl->SortItems (list_sort_callback, (unsigned long) _listctrl);
	
	for (long i=0; i < _listctrl->GetItemCount(); ++i) {
		item.SetId(i);
		item.SetColumn(0);
		if (_listctrl->GetItem(item)) {
			MidiBindInfo * info = (MidiBindInfo *) item.GetData();
			if (*info == _currinfo) {
				_listctrl->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
				_listctrl->EnsureVisible(i);
				_selitem  = i;
				// cerr << "matched curringo " << i << endl;
				break;
			}
		}
	}
	
// 	if (selexists) {
// 		_edit_panel->Enable(true);
// 	} else {
// 		_edit_panel->Enable(false);
// 	}
	
}


void MidiBindPanel::update_entry_area(MidiBindInfo * usethis)
{
	if (_selitem < 0 && !usethis) {
		return;
	}

	MidiBindInfo * info = 0;

	if (usethis) {
		info = usethis;
	}
	else if (_selitem >=0) {
		info = static_cast<MidiBindInfo *>((void *)_listctrl->GetItemData(_selitem));
	}

	if (!info) {
		return;
	}

	for (int i=0; i < (int)_control_combo->GetCount(); ++i) {
		if (static_cast<const char *>(_control_combo->GetClientData(i)) == info->control) {
			_control_combo->SetSelection(i);
			break;
		}
	}

	_loopnum_combo->SetSelection(info->instance + 3);
	_chan_spin->SetValue(info->channel + 1);

	if (info->type == "cc") {
		_type_combo->SetStringSelection(CcString);
	}
	else if (info->type == "ccoff") {
		_type_combo->SetStringSelection(CcOffString);
	}
	else if (info->type == "ccon") {
		_type_combo->SetStringSelection(CcOnString);
	}
	else if (info->type == "n") {
		_type_combo->SetStringSelection(NoteString);
	}
	else if (info->type == "on") {
		_type_combo->SetStringSelection(NoteOnString);
	}
	else if (info->type == "off") {
		_type_combo->SetStringSelection(NoteOffString);
	}
	else if (info->type == "pc") {
		_type_combo->SetStringSelection(PcString);
	}
	else if (info->type == "pb") {
		_type_combo->SetStringSelection(PitchBendString);
	}
	else if (info->type == "kp") {
		_type_combo->SetStringSelection(KeyPressureString);
	}
	else if (info->type == "cp") {
		_type_combo->SetStringSelection(ChannelPressureString);
	}

	if (info->command == "set") {
		_range_panel->Enable(true);
		_data_range_panel->Enable(true);

		_lbound_ctrl->SetValue (wxString::Format(wxT("%g"), info->lbound));
		_ubound_ctrl->SetValue (wxString::Format(wxT("%g"), info->ubound));
		
		_data_min_ctrl->SetValue (wxString::Format(wxT("%d"), info->data_min));
		_data_max_ctrl->SetValue (wxString::Format(wxT("%d"), info->data_max));
	}
	else {
		_range_panel->Enable(false); 
		_data_range_panel->Enable(false); 
		if (info->command == "susnote") {
			_sus_check->SetValue(true);
		}
		else {
			_sus_check->SetValue(false);
		}

		_lbound_ctrl->SetValue (wxT(""));
		_ubound_ctrl->SetValue (wxT(""));
		
		_data_min_ctrl->SetValue (wxT(""));
		_data_max_ctrl->SetValue (wxT(""));
	}
	
	_param_spin->SetValue(info->param);


	if (info->style == MidiBindInfo::GainStyle) {
		_style_combo->SetSelection(1);
	}
	else if (info->style == MidiBindInfo::ToggleStyle) {
		_style_combo->SetSelection(3);
	}
	else if (info->style == MidiBindInfo::IntegerStyle) {
		_style_combo->SetSelection(2);
	}
	else {
		_style_combo->SetSelection(0);
	}
	
}

void MidiBindPanel::update_curr_binding()
{
	double fval = 0.0;
	long  lval = 0;
	CommandMap & cmap = CommandMap::instance();

	if (_control_combo->GetSelection() < 0) {
		return;
	}
	
	// take info from editpanel and set the MidiBindInfo
	_currinfo.control = static_cast<const char *>(_control_combo->GetClientData(_control_combo->GetSelection()));
	_currinfo.channel = _chan_spin->GetValue() - 1;

	if (_loopnum_combo->GetSelection() >= 0) {
		_currinfo.instance = (int) (long) _loopnum_combo->GetClientData(_loopnum_combo->GetSelection()) - 1;
	}
	else {
		_currinfo.instance = -1;
	}

	
	wxString tsel = _type_combo->GetStringSelection();
	if (tsel == NoteString) {
		_currinfo.type = "n";
	}
	else if (tsel == NoteOnString) {
		_currinfo.type = "on";
	}
	else if (tsel == NoteOffString) {
		_currinfo.type = "off";
	}
	else if (tsel == CcString) {
		_currinfo.type = "cc";
	}
	else if (tsel == CcOnString) {
		_currinfo.type = "ccon";
	}
	else if (tsel == CcOffString) {
		_currinfo.type = "ccoff";
	}
	else if (tsel == PcString) {
		_currinfo.type = "pc";
	}
	else if (tsel == PitchBendString) {
		_currinfo.type = "pb";
	}
	else if (tsel == KeyPressureString) {
		_currinfo.type = "kp";
	}
	else if (tsel == ChannelPressureString) {
		_currinfo.type = "cp";
	}

	if (cmap.is_command(_currinfo.control)) {
		if (_currinfo.type == "pc" || _currinfo.type == "cc") {
			_currinfo.command = "hit";
		}
		else {
			if (_sus_check->GetValue()) {
				_currinfo.command = "susnote";
			}
			else {
				_currinfo.command = "note";
			}
		}
	}
	else {
		_currinfo.command = "set";
			
		// control
		if (cmap.is_global_control(_currinfo.control)) {
			_currinfo.instance = -2;
		}
	}

	
	_currinfo.param = _param_spin->GetValue();

	if (tsel == PitchBendString || tsel == ChannelPressureString) {
		_currinfo.param = 0;
	}

	if (_lbound_ctrl->GetValue().ToDouble(&fval)) {
		_currinfo.lbound = (float) fval;
	}
	else {
		_currinfo.lbound = 0.0;
	}

	if (_ubound_ctrl->GetValue().ToDouble(&fval)) {
		_currinfo.ubound = (float) fval;
	}
	else {
		_currinfo.ubound = 1.0;
	}

	if (_data_min_ctrl->GetValue().ToLong(&lval)) {
		//cerr << "data minis: " << (const char *) _data_min_ctrl->GetValue().ToAscii() << endl;
		_currinfo.data_min = (int) lval;
	}
	else {
		_currinfo.data_min = 0;
	}

	if (_data_max_ctrl->GetValue().ToLong(&lval)) {
		//cerr << "data max is: " << (const char *) _data_max_ctrl->GetValue().ToAscii() << endl;
		_currinfo.data_max = (int) lval;
	}
	else if (tsel == PitchBendString) {
		_currinfo.data_max = 16383;
	}
	else {
		_currinfo.data_max = 127;
	}

	if (_style_combo->GetSelection() == 1) {
		_currinfo.style = MidiBindInfo::GainStyle;
	}
	else if (_style_combo->GetSelection() == 2) {
		_currinfo.style = MidiBindInfo::IntegerStyle;
	}
	else if (_style_combo->GetSelection() == 3) {
		_currinfo.style = MidiBindInfo::ToggleStyle;
	}
	else {
		_currinfo.style = MidiBindInfo::NormalStyle;
	}
	
}

void MidiBindPanel::on_combo (wxCommandEvent &ev)
{
	if (_control_combo->GetSelection() < 0) {
		return;
	}

	string control = static_cast<const char *>(_control_combo->GetClientData(_control_combo->GetSelection()));

 	if (CommandMap::instance().is_command(control)) {
 		_range_panel->Enable(false);
 		_data_range_panel->Enable(false);
		_lbound_ctrl->SetValue(wxT(""));
		_ubound_ctrl->SetValue(wxT(""));
		_data_min_ctrl->SetValue(wxT(""));
		_data_max_ctrl->SetValue(wxT(""));
 	}
 	else {
 		_range_panel->Enable(true);
 		_data_range_panel->Enable(true);

		// set default ranges
		CommandMap::ControlInfo info;
		CommandMap::instance().get_control_info(control, info);
		_lbound_ctrl->SetValue(wxString::Format(wxT("%g"), info.minValue));
		_ubound_ctrl->SetValue(wxString::Format(wxT("%g"), info.maxValue));
		_data_min_ctrl->SetValue (wxT(""));
		_data_max_ctrl->SetValue (wxT(""));
 	}
}

void MidiBindPanel::on_button (wxCommandEvent &ev)
{
	if (ev.GetId() == ID_CloseButton) {
		Show(false);
	}
	else if (ev.GetId() == ID_AddButton)
	{
		update_curr_binding();
		_parent->get_loop_control().add_midi_binding(_currinfo);
		_parent->get_loop_control().request_all_midi_bindings();
	}
	else if (ev.GetId() == ID_LearnButton) {
		if (!_learning) {
			update_curr_binding();
			
			_learn_button->SetLabel (wxT("C. Learn"));
			_learn_button->SetForegroundColour (*wxRED);
			_learning = true;
			_parent->get_loop_control().request_next_midi_event();
		}
		else {
			//_learn_button->SetLabel (wxT("Learn"));
			//_learn_button->SetForegroundColour (*wxBLACK); // todo default
			//_learning = false;
			
			_parent->get_loop_control().cancel_next_midi_event();
		}
	}
	else if (ev.GetId() == ID_RemoveButton)
	{
		if (_listctrl->GetSelectedItemCount() > 0) {
			update_curr_binding();
			_parent->get_loop_control().remove_midi_binding(_currinfo);
			_parent->get_loop_control().request_all_midi_bindings();
		}
	}
	else if (ev.GetId() == ID_ModifyButton)
	{
		//if (_listctrl->GetSelectedItemCount() > 0) {
			_parent->get_loop_control().remove_midi_binding(_currinfo);
			update_curr_binding();
			_parent->get_loop_control().add_midi_binding(_currinfo);
			_parent->get_loop_control().request_all_midi_bindings();
			//}
	}
	else if (ev.GetId() == ID_ClearAllButton)
	{
		// are you sure?
		wxMessageDialog dial(this, wxT("Are you sure you want to clear all midi bindings?"), wxT("Clear MIDI Bindings"), wxYES_NO);
		if (dial.ShowModal() == wxID_YES) {
			_parent->get_loop_control().clear_midi_bindings();
		}
	}
	else if (ev.GetId() == ID_LoadButton)
	{
		wxString filename = _parent->do_file_selector(wxT("Choose midi binding file to open"), wxT(""), wxT("*.slb"), wxFD_OPEN|wxFD_CHANGE_DIR);
		if ( !filename.empty() )
		{
			_parent->get_loop_control().load_midi_bindings(filename,  _append_check->GetValue());
		}
	}
	else if (ev.GetId() == ID_SaveButton)
	{
		wxString filename = _parent->do_file_selector(wxT("Choose midi binding file to save"), wxT(""), wxT("*.slb"), wxFD_SAVE|wxFD_CHANGE_DIR|wxFD_OVERWRITE_PROMPT);

		if ( !filename.empty() )
		{
			// add .slb if there isn't one already
			if (filename.size() > 4 && filename.substr(filename.size() - 4, 4) != wxT(".slb")) {
				filename += wxT(".slb");
			}
			_parent->get_loop_control().save_midi_bindings(filename);
		}
	}
	else {
		ev.Skip();
	}
}
				   

void MidiBindPanel::item_selected (wxListEvent & ev)
{
	//cerr << "item " << ev.GetText() << " sel" << endl;
	_selitem = ev.GetIndex();

	update_entry_area();
	update_curr_binding();
	
}

void MidiBindPanel::learning_stopped ()
{
	// cerr << "learning stopped " << endl;
	//_learn_button->SetLabel (wxT("Learn Selected"));
	//_learn_button->SetForegroundColour (*wxBLACK);

	//refresh_state();

	//_listctrl->SetFocus();
}
	
	
