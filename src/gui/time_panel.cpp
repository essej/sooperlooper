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
#include <wx/file.h>
#include <wx/filename.h>

#include <iostream>

#include "time_panel.hpp"
#include "loop_control.hpp"

using namespace SooperLooperGui;
using namespace std;

BEGIN_EVENT_TABLE(TimePanel, wxPanel)
	EVT_PAINT(TimePanel::OnPaint)
	EVT_SIZE(TimePanel::OnSize)
END_EVENT_TABLE()

TimePanel::TimePanel(LoopControl * control, wxWindow * parent, wxWindowID id, const wxPoint& pos, const wxSize& size)
	: wxPanel(parent, id, pos, size), _loop_control(control), _index(0), _width(1), _height(1)
{
	_backing_store = 0;
	update_size();
	init();
}

TimePanel::~TimePanel()
{
	if (_backing_store) {
		delete _backing_store;
	}
}

void
TimePanel::init()
{
	SetThemeEnabled(false);

	_bgcolor.Set(34, 49, 71);
	SetBackgroundColour(_bgcolor);
	_bgbrush.SetColour(_bgcolor);
	
	_pos_font.SetFamily(wxSWISS);
	_pos_font.SetStyle(wxNORMAL);
	_pos_font.SetWeight(wxBOLD);
	_pos_color.Set(244, 255, 158);
	_pos_str = "00:00.00";
	_pos_font.SetPointSize(10);
	normalize_font_size(_pos_font, 110, 40, wxT("00:00.00"));

	_pos_bm = new wxBitmap(110,40); 
	_posdc.SelectObject(*_pos_bm);
	_posdc.SetFont(_pos_font);
 	_posdc.SetTextForeground(_pos_color);
	_posdc.SetBackground (_bgbrush);

	_state_font.SetFamily(wxSWISS);
	_state_font.SetWeight(wxBOLD);
	_state_font.SetStyle(wxNORMAL);
	_state_color.Set(154, 255, 168);
	_state_str = "------";
	_state_font.SetPointSize(10);
	normalize_font_size(_state_font, 110, 30, wxT("ooooooooooo"));

	_state_bm = new wxBitmap(110,20); 
	_statedc.SelectObject(*_state_bm);
	_statedc.SetFont(_state_font);
 	_statedc.SetTextForeground(_state_color);
	_statedc.SetBackground (_bgbrush);
	
	_time_font.SetFamily(wxSWISS);
	_time_font.SetStyle(wxNORMAL);
	_time_font.SetWeight(wxNORMAL);
	_time_color.Set(244, 255, 178);
	_time_font.SetPointSize(7);
	normalize_font_size(_time_font, 45, 20, wxT("00:00.00"));
	_tot_str = "00:00.00";
	_cyc_str = "00:00.00";
	_rem_str = "00:00.00";


	_other_bm = new wxBitmap(90,60); 
	_otherdc.SelectObject(*_other_bm);
	_otherdc.SetFont(_time_font);
	_otherdc.SetBackground (_bgbrush);
	_legend_color.Set(225, 225, 225);

	_wait_bm = new wxBitmap(110,20); 
	_waitdc.SelectObject(*_wait_bm);
	_waitdc.SetFont (_time_font);
	_waitdc.SetBackground (_bgbrush);
	_waitdc.SetTextForeground(_legend_color);

	_waiting = false;
}

void
TimePanel::normalize_font_size(wxFont & fnt, int width, int height, wxString fitstr)
{
	int fontw, fonth, desch, lead, lastw=0, lastsize = fnt.GetPointSize();
	
	GetTextExtent(fitstr, &fontw, &fonth, &desch, &lead, &fnt);
	//printf ("Text extent for %s: %d %d %d %d   sz: %d\n", fitstr.c_str(), fontw, desch, lead, fonth, fnt.GetPointSize());
	while (fonth < height && fontw < width) {
		lastsize = fnt.GetPointSize();
		fnt.SetPointSize(fnt.GetPointSize() + 1);
		GetTextExtent(fitstr, &fontw, &fonth, &desch, &lead, &fnt);
		//printf ("Text extent for buttfont: %d %d %d %d sz: %d\n", fontw, fonth, desch, lead, fnt.GetPointSize());

		//if (lastw == fontw) break; // safety
		lastw = fontw;
	}

	fnt.SetPointSize(lastsize);
	
}


void
TimePanel::format_time(wxString & timestr, float val)
{
	// seconds
	int minutes = (int) (val / 60.0f);
	float secs = val - minutes*60.0f;
	
	timestr.Printf("%02d:%05.2f", minutes, secs);
}

bool
TimePanel::update_time()
{
	float val;
	bool ret = false;
	
	if (_loop_control->is_updated(_index, "loop_pos")) {
		_loop_control->get_value(_index, "loop_pos", val);
		format_time (_pos_str, val);
		ret = true;
	}

	if (_loop_control->is_updated(_index, "loop_len")) {
		_loop_control->get_value(_index, "loop_len", val);
		format_time (_tot_str, val);
		ret = true;
	}

	if (_loop_control->is_updated(_index, "cycle_len")) {
		_loop_control->get_value(_index, "cycle_len", val);
		format_time (_cyc_str, val);
		ret = true;
	}

	if (_loop_control->is_updated(_index, "free_time")) {
		_loop_control->get_value(_index, "free_time", val);
		format_time (_rem_str, val);
		ret = true;
	}

	if (_loop_control->is_updated(_index, "total_time")) {
		_loop_control->get_value(_index, "total_time", val);
		format_time (_mem_str, val);
		ret = true;
	}

	if (_loop_control->is_updated(_index, "waiting")) {
		_loop_control->get_value(_index, "waiting", val);
		_waiting = (val > 0.0f) ? true : false;
		ret = true;
	}
	
	if (_loop_control->is_updated(_index, "state")) {
		SooperLooper::LooperState tmpstate;
		_loop_control->get_state(_index, tmpstate, _state_str);
		ret = true;
	}
	
	
	return ret;
}

void
TimePanel::update_size()
{
	GetClientSize(&_width, &_height);

	if (_width > 0 && _height > 0) {
		_memdc.SelectObject (wxNullBitmap);
		if (_backing_store) {
			delete _backing_store;
		}
		_backing_store = new wxBitmap(_width, _height);
		
		_memdc.SelectObject(*_backing_store);
	}
}

void
TimePanel::OnSize (wxSizeEvent &ev)
{
	update_size();
	ev.Skip();
}

void
TimePanel::OnPaint(wxPaintEvent &ev)
{
 	wxPaintDC pdc(this);

	if (!_backing_store) {
		return;
	}
	
 	draw_area(_memdc);

 	pdc.Blit(0, 0, _width, _height, &_memdc, 0, 0);
}

void
TimePanel::draw_area(wxDC & dc)
{
	wxCoord sw=0, sh=0, tw=0, th=0, w=0, h=0;
	// wxCoord cw=0, ch=0, rw=0, rh=0;

	dc.SetBackground(_bgbrush);
	dc.Clear();

	// main pos

// 	dc.SetFont(_pos_font);
// 	dc.SetTextForeground(_pos_color);
// 	dc.DrawText (_pos_str, 5, 3);
	_posdc.Clear();
	_posdc.GetTextExtent(_pos_str, &w, &h);
 	_posdc.DrawText (_pos_str, 0, 0);

	dc.Blit (5,3, _pos_bm->GetWidth(), _pos_bm->GetHeight(), &_posdc, 0, 0);
	
	// state
// 	dc.SetFont(_state_font);
// 	dc.SetTextForeground(_state_color);
// 	dc.GetTextExtent(_state_str, &sw, &sh);
// 	dc.DrawText (_state_str, 5, _height - sh - 5);
	_statedc.Clear();
 	_statedc.GetTextExtent(_state_str, &sw, &sh);
 	_statedc.DrawText (_state_str, 0, 0);
	dc.Blit (5, _height - sh - 5, _state_bm->GetWidth(), _state_bm->GetHeight(), &_statedc, 0, 0);

	
	// waiting string
	if (_waiting) {
		_waitdc.Clear();
		//dc.SetFont(_legend_font);
		//dc.DrawText (wxT("waiting for sync"), 5, _height - sh - 17);
		_waitdc.GetTextExtent(wxT("waiting for sync"), &tw, &th);
		_waitdc.DrawText (wxT("waiting for sync"), 0, 0);
		dc.Blit (5, h , tw, th, &_waitdc, 0, 0);
	}
	
	// other times
// 	dc.SetFont(_time_font);
// 	dc.SetTextForeground(_time_color);
 	_otherdc.SetTextForeground(_time_color);
	_otherdc.Clear();
	
// 	dc.GetTextExtent(_tot_str, &tw, &th);
// 	dc.DrawText (_tot_str, _width - tw - 5, 5);

 	_otherdc.GetTextExtent(_tot_str, &tw, &th);
	tw += 3;
	
 	_otherdc.DrawText (_tot_str, _other_bm->GetWidth() - tw, 0);
	
	
// 	dc.GetTextExtent(_cyc_str, &cw, &ch);
// 	dc.DrawText (_cyc_str, _width - cw - 5, 5 + th);
 	_otherdc.DrawText (_cyc_str, _other_bm->GetWidth() - tw, 2 + th);

// 	// rem time
// 	dc.GetTextExtent(_rem_str, &rw, &rh);
// 	dc.DrawText (_rem_str, _width - rw - 5, _height - rh - 5);
 	//_otherdc.DrawText (_rem_str, _other_bm->GetWidth() - tw, 10 + th + th);
	_otherdc.DrawText (_mem_str, _other_bm->GetWidth() - tw, 10 + th + th);
	
	// legends
// 	dc.SetFont(_legend_font);
// 	dc.SetTextForeground(_legend_color);
 	_otherdc.SetTextForeground(_legend_color);

// 	dc.GetTextExtent(wxT("tot"), &w, &h);
// 	dc.DrawText (wxT("tot"), _width - tw - w - 10, 5);
 	_otherdc.GetTextExtent(wxT("mem"), &w, &h);
 	_otherdc.DrawText (wxT("tot"), _other_bm->GetWidth() - tw - w - 5, 0);
	
	
// 	dc.GetTextExtent(wxT("cyc"), &w, &h);
// 	dc.DrawText (wxT("cyc"), _width - cw - w - 10, 5 + th);
 	_otherdc.DrawText (wxT("cyc"), _other_bm->GetWidth() - tw - w - 5, 2 + th);

// 	dc.GetTextExtent(wxT("rem"), &w, &h);
// 	dc.DrawText (wxT("rem"), _width - rw - w - 10, _height - rh - 5);
 	_otherdc.DrawText (wxT("mem"), _other_bm->GetWidth() - tw - w - 5, 10 + th + th);

	dc.Blit (120, 5, _other_bm->GetWidth(), _other_bm->GetHeight(), &_otherdc, 0, 0);
	
}
