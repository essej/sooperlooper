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

#include <wx/wx.h>

#include <iostream>
#include <string>
#include <vector>
#include <sigc++/sigc++.h>

#include "main_panel.hpp"
#include "latency_panel.hpp"
#include "loop_control.hpp"
#include "spin_box.hpp"

using namespace SooperLooperGui;
using namespace std;

enum {
	ID_AutoCheck = 8000,
	ID_InputLatency,
	ID_OutputLatency,
	ID_UpdateTimer,
	ID_AutoDisableCheck

};

BEGIN_EVENT_TABLE(SooperLooperGui::LatencyPanel, wxPanel)
	EVT_CHECKBOX (ID_AutoCheck, SooperLooperGui::LatencyPanel::on_check)
	EVT_CHECKBOX (ID_AutoDisableCheck, SooperLooperGui::LatencyPanel::on_check)
	EVT_TIMER(ID_UpdateTimer, SooperLooperGui::LatencyPanel::OnUpdateTimer)

	EVT_SIZE (SooperLooperGui::LatencyPanel::onSize)
	EVT_PAINT (SooperLooperGui::LatencyPanel::onPaint)
	
END_EVENT_TABLE()

	
// ctor(s)
LatencyPanel::LatencyPanel(MainPanel * mainpan, wxWindow * parent, wxWindowID id,
		       const wxPoint& pos,
		       const wxSize& size,
		       long style ,
		       const wxString& name)

	: wxPanel (parent, id, pos, size, style, name), _parent (mainpan)
{
	_justResized = false;
	_do_request = false;
	
	init();
}

LatencyPanel::~LatencyPanel()
{

}

void LatencyPanel::onSize(wxSizeEvent &ev)
{

	_justResized = true;
	ev.Skip();
}

void LatencyPanel::onPaint(wxPaintEvent &ev)
{
	if (_justResized) {

		_justResized = false;
		
	}

	ev.Skip();
}

void
LatencyPanel::OnUpdateTimer(wxTimerEvent &ev)
{
	// refresh state if visible

	if (_do_request) {
		LoopControl & lcontrol = _parent->get_loop_control();
		lcontrol.request_control_value(0, wxT("input_latency"));
		lcontrol.request_control_value(0, wxT("output_latency"));
		lcontrol.request_control_value(0, wxT("autoset_latency"));
		lcontrol.request_global_control_value(wxT("auto_disable_latency"));
		_do_request = false;
		_update_timer->Start(200, true);
		return;
	}

	if (IsShown()) {
		// this test doesn't work
		//cerr << "refreshing" << endl;

		refresh_state();
	}
	
	_update_timer->Start(5000, true);

}

void LatencyPanel::init()
{
	wxBoxSizer * topsizer = new wxBoxSizer(wxVERTICAL);

	_auto_check = new wxCheckBox(this, ID_AutoCheck, wxT("Automatically set Latency Compensation Values"));
	topsizer->Add (_auto_check, 0, wxEXPAND|wxALL, 12);

	wxBoxSizer * rowsizer = new wxBoxSizer(wxHORIZONTAL);
	
	_input_spin = new SpinBox(this, ID_InputLatency, 0.0f, 100000.0f, 512.0f, false, wxDefaultPosition, wxSize(200, 35));
	_input_spin->set_units(wxT("samples"));
	_input_spin->set_label(wxT("  Input Latency"));
	_input_spin->set_snap_mode (SpinBox::IntegerSnap);
	_input_spin->set_allow_outside_bounds(false);
	//_input_spin->SetFont (sliderFont);
	_input_spin->set_decimal_digits(0);
	_input_spin->value_changed.connect (bind (slot (*this,  &LatencyPanel::on_spin_change), (int) ID_InputLatency));
	rowsizer->Add (_input_spin, 1, wxLEFT|wxEXPAND, 10);

	_output_spin = new SpinBox(this, ID_OutputLatency, 0.0f, 100000.0f, 512.0f, false, wxDefaultPosition, wxSize(200, 35));
	_output_spin->set_units(wxT("samples"));
	_output_spin->set_label(wxT("  Output Latency"));
	_output_spin->set_snap_mode (SpinBox::IntegerSnap);
	_output_spin->set_allow_outside_bounds(false);
	_output_spin->set_decimal_digits(0);
	//_output_spin->SetFont (sliderFont);
	_output_spin->value_changed.connect (bind (slot (*this,  &LatencyPanel::on_spin_change), (int) ID_OutputLatency));
	rowsizer->Add (_output_spin, 1, wxLEFT|wxRIGHT|wxEXPAND, 10);
	
	
	topsizer->Add (rowsizer, 0, wxEXPAND|wxALL, 6);

	_auto_disable_check = new wxCheckBox(this, ID_AutoDisableCheck, wxT("Automatically Disable Compensation when Monitoring Input"));
	topsizer->Add (_auto_disable_check, 0, wxEXPAND|wxALL, 12);

	_update_timer = new wxTimer(this, ID_UpdateTimer);
	_update_timer->Start(5000, true);

	refresh_state();
	
	this->SetAutoLayout( true );     // tell dialog to use sizer
	this->SetSizer( topsizer );      // actually set the sizer
	//topsizer->Fit( this );            // set size to minimum size as calculated by the sizer
	//topsizer->SetSizeHints( this );   // set size hints to honour mininum size
}

void LatencyPanel::refresh_state()
{
	float retval;
	LoopControl & lcontrol = _parent->get_loop_control();

	if (lcontrol.get_value (0, wxT("autoset_latency"), retval)) {
		_auto_check->SetValue(retval > 0.0f);
	}
	if (lcontrol.get_global_value (wxT("auto_disable_latency"), retval)) {
		_auto_disable_check->SetValue(retval > 0.0f);
	}
	
	if (lcontrol.get_value (0, wxT("input_latency"), retval)) {
		_input_spin->set_value (retval);

		if (_auto_check->GetValue()) {
			_input_spin->set_default_value(retval);
		}
	}

	if (lcontrol.get_value (0, wxT("output_latency"), retval)) {
		_output_spin->set_value (retval);
		if (_auto_check->GetValue()) {
			_output_spin->set_default_value(retval);
		}
	}


	if (_auto_check->GetValue()) {
		_input_spin->Enable(false);
		_output_spin->Enable(false);
	}
	else {
		_input_spin->Enable(true);
		_output_spin->Enable(true);
	}

}

void LatencyPanel::on_check (wxCommandEvent &ev)
{
	// set state of autoset for all loops
	LoopControl & lcontrol = _parent->get_loop_control();

	if (ev.GetId() == ID_AutoCheck) {
		lcontrol.post_ctrl_change (-1, wxT("autoset_latency"), _auto_check->GetValue() ? 1.0f : 0.0f);
	}
	else {
		lcontrol.post_ctrl_change (-2, wxT("auto_disable_latency"), _auto_disable_check->GetValue() ? 1.0f : 0.0f);
	}
	_do_request = true;
	_update_timer->Start(200, true);

}

void LatencyPanel::on_spin_change (float value, int id)
{
	LoopControl & lcontrol = _parent->get_loop_control();

	// set for all loops
	
	if (id == ID_InputLatency) {
		lcontrol.post_ctrl_change (-1, wxT("input_latency"), value);
	}
	else {
		lcontrol.post_ctrl_change (-1, wxT("output_latency"), value);
	}
}



	
	
