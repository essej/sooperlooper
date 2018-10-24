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
#include <cmath>

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
	draw_area();
}

TimePanel::~TimePanel()
{
	_memdc.SelectObject(wxNullBitmap);
	if (_backing_store) {
		delete _backing_store;
	}

	delete _pos_bm;
	delete _cyc_bm;
	delete _state_bm;
	delete _other_bm;
	delete _wait_bm;
}

void
TimePanel::init()
{
	SetThemeEnabled(false);

	_curr_ccnt = -1;
	_curr_currc = -1;
	
	_transbrush.SetStyle(wxTRANSPARENT);

	_bgcolor.Set(34, 49, 71);
	SetBackgroundColour(_bgcolor);
	_bgbrush.SetColour(_bgcolor);
	
	_pos_font.SetFamily(wxSWISS);
	_pos_font.SetStyle(wxNORMAL);
	_pos_font.SetWeight(wxBOLD);
	_pos_color.Set(244, 255, 158);
	_pos_str = wxT("00:00.00");
	_pos_font.SetPointSize(10);
	normalize_font_size(_pos_font, 110, 30, wxT("00:00.00"));

       	_pos_bm = new wxBitmap(110,25); 
	_posdc.SelectObject(*_pos_bm);
	_posdc.SetFont(_pos_font);
 	_posdc.SetTextForeground(_pos_color);
	_posdc.SetBackground (_bgbrush);

	_cyc_font.SetFamily(wxSWISS);
	_cyc_font.SetStyle(wxNORMAL);
	_cyc_font.SetWeight(wxNORMAL);
	_cyc_color.Set(154, 255, 168);
	_cyc_font.SetPointSize(10);
	normalize_font_size(_cyc_font, 20, 20, wxT("00"));
	_cyc_cnt_str = wxT("-");
	_cyc_curr_str = wxT("-");

       	_cyc_bm = new wxBitmap(20,42); 
	_cycdc.SelectObject(*_cyc_bm);
	_cycdc.SetFont(_cyc_font);
 	_cycdc.SetTextForeground(_cyc_color);
 	_cycdc.SetPen(*wxWHITE_PEN);
	_cycdc.SetBackground (_bgbrush);

	_state_font.SetFamily(wxSWISS);
	_state_font.SetWeight(wxBOLD);
	_state_font.SetStyle(wxNORMAL);
	_state_color.Set(154, 255, 168);
	_state_str = wxT("not connected");
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
	_tot_str = wxT("00:00.00");
	_cyc_str = wxT("00:00.00");
	_rem_str = wxT("00:00.00");

	_sw = _sh = _tw = _th = _pw = _ph = _mh = _mw = _ww = _wh = 0;
	
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

	calc_text_extents();
	
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
	float secs = fabs(val - minutes*60.0f);
	
	timestr.Printf(wxT("%02d:%05.2f"), minutes, secs);
}

void
TimePanel::update_cyc()
{
	float llen, lpos, clen;
	
	_loop_control->get_value(_index, wxT("loop_len"), llen);
	_loop_control->get_value(_index, wxT("loop_pos"), lpos);
	_loop_control->get_value(_index, wxT("cycle_len"), clen);
	
	if (clen > 0.0f) {
		int cntc =  (int) roundf (llen / clen);
		int currc = (int) ceilf (lpos / (clen));
		if (currc > cntc) currc = cntc;

		if (cntc != _curr_ccnt || currc != _curr_currc) {
			_curr_ccnt = cntc;
			_curr_currc = currc;
			
			_cyc_cnt_str.Printf(wxT("%2d"), cntc);
			_cyc_curr_str.Printf(wxT("%2d"), currc);

			draw_cycle();
		}
	}
	else {
		_cyc_curr_str.Printf(wxT("-"));
		_cyc_cnt_str.Printf(wxT("-"));

		if (_curr_currc != 0 || _curr_ccnt != 0) {
			_curr_currc = 0;
			_curr_ccnt = 0;
			draw_cycle();
		}

	}
}

bool
TimePanel::update_time()
{
	float val;
	bool ret = false;
	bool up_cyc = false;
	bool up_other = false;
	bool up_pos = false;
	bool up_state = false;
	
	if (_loop_control->is_updated(_index, wxT("loop_pos"))) {
		_loop_control->get_value(_index, wxT("loop_pos"), val);
		format_time (_pos_str, val);
		ret = true;
		up_cyc = true;
		up_pos = true;
	}

	if (_loop_control->is_updated(_index, wxT("loop_len"))) {
		_loop_control->get_value(_index, wxT("loop_len"), val);
		format_time (_tot_str, val);
		up_cyc = true;
		up_other = true;
		ret = true;
	}

	if (_loop_control->is_updated(_index, wxT("cycle_len"))) {
		_loop_control->get_value(_index, wxT("cycle_len"), val);
		format_time (_cyc_str, val);
		up_cyc = true;
		ret = true;
	}

	if (_loop_control->is_updated(_index, wxT("free_time"))) {
		_loop_control->get_value(_index, wxT("free_time"), val);
		format_time (_rem_str, val);
		ret = true;
	}

	if (_loop_control->is_updated(_index, wxT("total_time"))) {
		_loop_control->get_value(_index, wxT("total_time"), val);
		format_time (_mem_str, val);
		ret = true;
		up_other = true;
	}

	if (_loop_control->is_updated(_index, wxT("waiting"))) {
		_loop_control->get_value(_index, wxT("waiting"), val);
		_waiting = (val > 0.0f) ? true : false;
		ret = true;
		up_state = true;
	}
	
	if (_loop_control->is_updated(_index, wxT("state"))) {
		SooperLooper::LooperState tmpstate;
		_loop_control->get_state(_index, tmpstate, _state_str);
		calc_text_extents();
		ret = true;
		up_state = true;
	}

	if (up_cyc) {
		update_cyc();
	}

	if (up_other) {
		draw_other();
	}

	if (up_state) {
		draw_state();
	}

	if (up_pos) {
		draw_pos();
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
	do_redraw();
	ev.Skip();
}

void
TimePanel::do_redraw ()
{
	if (!_backing_store) {
		return;
	}

	draw_area();
	Refresh(false);
}


void
TimePanel::OnPaint(wxPaintEvent &ev)
{
 	wxPaintDC pdc(this);

// 	if (!_backing_store) {
// 		return;
// 	}
	
//  	draw_area(_memdc);

 	pdc.Blit(0, 0, _width, _height, &_memdc, 0, 0);
}

void
TimePanel::calc_text_extents()
{
	_posdc.GetTextExtent(_pos_str, &_pw, &_ph);
 	_statedc.GetTextExtent(_state_str, &_sw, &_sh);
 	_otherdc.GetTextExtent(_tot_str, &_tw, &_th);
	_tw += 3;
 	_otherdc.GetTextExtent(wxT("mem"), &_mw, &_mh);
	_waitdc.GetTextExtent(wxT("waiting for sync"), &_ww, &_wh);
}

void
TimePanel::draw_other()
{
	// other times
 	_otherdc.SetTextForeground(_time_color);
	_otherdc.Clear();
	
 	_otherdc.DrawText (_tot_str, _other_bm->GetWidth() - _tw, 0);
	
	
 	_otherdc.DrawText (_cyc_str, _other_bm->GetWidth() - _tw, 2 + _th);

// 	// rem time
 	//_otherdc.DrawText (_rem_str, _other_bm->GetWidth() - tw, 10 + th + th);
	_otherdc.DrawText (_mem_str, _other_bm->GetWidth() - _tw, 10 + _th + _th);
	
	// legends
 	_otherdc.SetTextForeground(_legend_color);

 	_otherdc.DrawText (wxT("tot"), _other_bm->GetWidth() - _tw - _mw + 6, 0);
	
	
 	_otherdc.DrawText (wxT("cyc"), _other_bm->GetWidth() - _tw - _mw + 6, 2 + _th);

 	_otherdc.DrawText (wxT("mem"), _other_bm->GetWidth() - _tw - _mw - 5, 10 + _th + _th);

	_memdc.Blit (120, 5, _other_bm->GetWidth(), _other_bm->GetHeight(), &_otherdc, 0, 0);


	// sadly we have to draw cycle too, because it can overlap
	draw_cycle();
}


void
TimePanel::draw_pos()
{
	_posdc.Clear();
 	_posdc.DrawText (_pos_str, 0, 0);

	_memdc.Blit (5,3, _pos_bm->GetWidth(), _pos_bm->GetHeight(), &_posdc, 0, 0);
}

void
TimePanel::draw_state()
{
	if (!_waiting) {
		_waitdc.Clear();
		_memdc.Blit (5, _ph , _ww, _wh, &_waitdc, 0, 0);
	}

	_statedc.Clear();
 	_statedc.DrawText (_state_str, 0, 0);
	_memdc.Blit (5, _height - _sh - 5, _state_bm->GetWidth(), _state_bm->GetHeight(), &_statedc, 0, 0);

	// waiting string
	if (_waiting) {
		_waitdc.Clear();
		//dc.SetFont(_legend_font);
		//dc.DrawText (wxT("waiting for sync"), 5, _height - sh - 17);
		_waitdc.DrawText (wxT("waiting for sync"), 0, 0);
		_memdc.Blit (5, _ph , _ww, _wh, &_waitdc, 0, 0);
	}
}

void
TimePanel::draw_cycle()
{
	// draw it
	_cycdc.Clear();
 	_cycdc.DrawText (_cyc_curr_str, 0, 0);
 	_cycdc.DrawText (_cyc_cnt_str,  0, 18);

	_cycdc.DrawLine (5, 17, 13, 17);

	_memdc.Blit (_pos_bm->GetWidth() + 3, 2, _cyc_bm->GetWidth(), _cyc_bm->GetHeight(),  &_cycdc, 0, 0);
}

void
TimePanel::draw_area()
{
	//wxCoord sw=0, sh=0, tw=0, th=0, w=0, h=0;
	// wxCoord cw=0, ch=0, rw=0, rh=0;

	_memdc.SetBackground(_bgbrush);
	_memdc.Clear();

	// main pos

	draw_pos();
	
	// state

	draw_state();
	
	// other times

	draw_other();

	draw_cycle();
	
}
