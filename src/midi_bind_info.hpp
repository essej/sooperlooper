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

#ifndef __sooperlooper_midi_bind_info__
#define __sooperlooper_midi_bind_info__

#include <string>

namespace SooperLooper {

class MidiBindInfo
{
public:
	
	enum Style {
		NormalStyle = 0,
		GainStyle,
	};
	
	MidiBindInfo() {}
	MidiBindInfo (int chan, std::string tp, std::string cmd, std::string ctrl, int instc, float lbnd=0.0f, float ubnd=1.0f, Style styl=NormalStyle)
		: channel(chan), type(tp), command(cmd), control(ctrl), instance(instc), lbound(lbnd), ubound(ubnd), style(styl) {}

	inline bool operator==(const MidiBindInfo &) const;

	
	std::string serialize() const;
	bool unserialize(std::string);

	int         channel;
	std::string type;
	std::string command;
	std::string control;
	int         param;
	
	int         instance;
	
	float       lbound;
	float       ubound;
	
	Style       style;
};

	inline bool MidiBindInfo::operator==(const MidiBindInfo &other) const
	{
		if (channel == other.channel &&
		    type == other.type &&
		    command == other.command &&
		    control == other.control &&
		    param  == other.param &&
		    instance == other.instance)
		{
			return true;
		}
		    
		return false;
	}
	
};

#endif
