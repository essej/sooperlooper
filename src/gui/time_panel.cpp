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
	init();
}

TimePanel::~TimePanel()
{

}

void
TimePanel::init()
{
	SetThemeEnabled(false);

	_pos_font.SetFamily(wxSWISS);
	_pos_font.SetStyle(wxNORMAL);
	_pos_font.SetWeight(wxBOLD);
	_pos_color.Set(244, 255, 158);
	_pos_str = "--:--.--";
	_pos_font.SetPointSize(10);
	normalize_font_size(_pos_font, 110, 40, wxT("00:00.00"));
	
	_state_font.SetFamily(wxSWISS);
	_state_font.SetWeight(wxBOLD);
	_state_font.SetStyle(wxNORMAL);
	_state_color.Set(154, 255, 168);
	_state_str = "------";
	_state_font.SetPointSize(10);
	normalize_font_size(_state_font, 110, 30, wxT("ooooooooooo"));

	_time_font.SetFamily(wxSWISS);
	_time_font.SetPointSize(7);
	_time_font.SetStyle(wxNORMAL);
	_time_font.SetWeight(wxNORMAL);
	_time_color.Set(244, 255, 178);

	_legend_font.SetFamily(wxSWISS);
	_legend_font.SetPointSize(7);
	_legend_font.SetStyle(wxNORMAL);
	_legend_font.SetWeight(wxNORMAL);
	_legend_color.Set(225, 225, 225);

	_bgcolor.Set(34, 49, 71);
	SetBackgroundColour(_bgcolor);
	_bgbrush.SetColour(_bgcolor);
	
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
		format_time (_tot_str, val);
		ret = true;
	}

	if (_loop_control->is_updated(_index, "state")) {
		_loop_control->get_state(_index, _state_str);
		ret = true;
	}
	
	
	return ret;
}


void
TimePanel::OnSize (wxSizeEvent &ev)
{
	GetClientSize(&_width, &_height);

	if (_backing_store) {
		delete _backing_store;
	}
	_backing_store = new wxBitmap(_width, _height);

	ev.Skip();
}

void
TimePanel::OnPaint(wxPaintEvent &ev)
{
	wxPaintDC pdc(this);

	wxMemoryDC dc;

	if (!_backing_store) {
		return;
	}
   
	dc.SelectObject(*_backing_store);
	
	draw_area(dc);

	pdc.Blit(0, 0, _width, _height, &dc, 0, 0);
}

void
TimePanel::draw_area(wxDC & dc)
{
	wxCoord sw=0, sh=0, tw=0, th=0, w=0, h=0;
	wxCoord cw=0, ch=0, rw=0, rh=0;

	dc.SetBackground(_bgbrush);
	dc.Clear();
	
	// main pos
	dc.SetFont(_pos_font);
	dc.SetTextForeground(_pos_color);
	dc.DrawText (_pos_str, 5, 5);
	
	// state
	dc.SetFont(_state_font);
	dc.SetTextForeground(_state_color);
	dc.GetTextExtent(_state_str, &sw, &sh);
	dc.DrawText (_state_str, 5, _height - sh - 5);

	// other times
	dc.SetFont(_time_font);
	dc.SetTextForeground(_time_color);

	dc.GetTextExtent(_tot_str, &tw, &th);
	dc.DrawText (_tot_str, _width - tw - 5, 5);

	dc.GetTextExtent(_cyc_str, &cw, &ch);
	dc.DrawText (_cyc_str, _width - cw - 5, 5 + th);

	// rem time
	dc.GetTextExtent(_rem_str, &rw, &rh);
	dc.DrawText (_rem_str, _width - rw - 5, _height - rh - 5);
	
	// legends
	dc.SetFont(_legend_font);
	dc.SetTextForeground(_legend_color);

	dc.GetTextExtent(wxT("tot"), &w, &h);
	dc.DrawText (wxT("tot"), _width - tw - w - 10, 5);

	dc.GetTextExtent(wxT("cyc"), &w, &h);
	dc.DrawText (wxT("cyc"), _width - cw - w - 10, 5 + th);

	dc.GetTextExtent(wxT("rem"), &w, &h);
	dc.DrawText (wxT("rem"), _width - rw - w - 10, _height - rh - 5);
	
}
