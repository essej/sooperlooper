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

#include <cstdio>
#include <iostream>

#include "midi_bind_info.hpp"

using namespace SooperLooper;
using namespace std;

string MidiBindInfo::serialize() const
{
	//  ch type param   cmd  ctrl  instance  [min_val_bound max_val_bound valstyle]
	//
	//    ch = midi channel starting from 0
	//    type is one of:  'pc' = program change  'cc' = control change  'n' = note on/off
	//           'on' = note on  'off' = note off
	//    param = # of midi parameter
	//
	//    cmd is one of ( note, down, up, set )
	//    ctrl is an SL control
	//    instance is loop #
	//    min_val_bound is what to treat midi val 0
	//    max_val_bound is what to treat midi val 127
	//    valstyle can be 'gain'

	char buf[100];

	// i:ch s:type i:param  s:cmd  s:ctrl i:instance f:min_val_bound f:max_val_bound s:valstyle
	snprintf(buf, sizeof(buf), "%d %s %d  %s %s %d  %.9g %.9g  %s",
		 channel, type.c_str(), param, command.c_str(),
		 control.c_str(), instance, lbound, ubound, style == GainStyle ? "gain": "");
	
	return string(buf);
}

bool MidiBindInfo::unserialize(string strval)
{
	int chan;
	int parm;
	char tp[32];
	char ctrl[32];
	char cmd[32];
	char stylestr[32];
	int  inst = -1;
	float lb = 0.0f;
	float ub = 1.0f;
	int ret;

	stylestr[0] = '\0';
	
	if ((ret = sscanf(strval.c_str(), "%d %3s %d  %30s %30s %d  %g %g  %10s",
			  &chan, tp, &parm, cmd, ctrl, &inst, &lb, &ub, stylestr)) < 5)
	{
		cerr << "ret: " << ret << " invalid input line: " << strval;
		return false;
	}

	channel = chan;
	type = tp;
	param = parm;
	command = cmd;
	control = ctrl;
	instance = inst;
	lbound = lb;
	ubound = ub;

	if (stylestr[0] == 'g') {
		style = GainStyle;
	} else {
		style = NormalStyle;
	}
	
	return true;
}
