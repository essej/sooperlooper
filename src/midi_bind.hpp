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

#ifndef __sooperlooper_midi_bind__
#define __sooperlooper_midi_bind__

#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <iostream>

#include <midi++/types.h>


namespace SooperLooper {

class MidiBindInfo
{
public:
	
	enum Style {
		NormalStyle = 0,
		GainStyle = 1,
		ToggleStyle = 2,
		IntegerStyle = 3
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

	// internal state for toggle style
	float       last_toggle_val;
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
	


	
class MidiBindings
{
public:

	MidiBindings();
	virtual ~MidiBindings();

	typedef std::vector<MidiBindInfo> BindingList;

	void clear_bindings ();

	bool load_bindings (std::string filename, bool append=false);
	bool load_bindings (std::istream & instream, bool append=false);

	bool save_bindings (std::string filename);
	bool save_bindings (std::ostream & outstream);

	void get_bindings (BindingList & blist) const;
	bool add_binding (const MidiBindInfo & info, bool exclusive=false);
	bool remove_binding (const MidiBindInfo & info);

	int binding_key (const MidiBindInfo & info) const;
	bool get_channel_and_type(MIDI::byte chcmd, int & ch, std::string & type) const;

	// the int key here is  (chcmd << 8) | param
	// or midi byte 1 and 2 in 16 bits
	typedef std::map<int, BindingList> BindingsMap;
	
	// for direct use... be careful
	BindingsMap & bindings_map() { return _bindings; }
	
protected:

	
	typedef std::map<std::string, int> TypeMap;
	TypeMap _typemap;
	
	BindingsMap _bindings;
	

};

};
#endif
