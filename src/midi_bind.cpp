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


#include "midi_bind.hpp"

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <cmath>

#include <midi++/parser.h>
#include <midi++/factory.h>


using namespace SooperLooper;
using namespace std;
using namespace MIDI;


MidiBindings::MidiBindings()
{
	_typemap["cc"] = MIDI::controller;
	_typemap["n"] = MIDI::on;
	_typemap["pc"] = MIDI::program;
	_typemap["on"] = MIDI::on;
	_typemap["off"] = MIDI::on;
	_typemap["pb"] = MIDI::pitchbend;
	_typemap["kp"] = MIDI::polypress;
	_typemap["cp"] = MIDI::chanpress;
	_typemap["ccon"] = MIDI::controller;
	_typemap["ccoff"] = MIDI::controller;

	// temp bindings
// 	_bindings[0x9000 | 48] = MidiBindInfo("note", "record", -1);
// 	_bindings[0x9000 | 49] = MidiBindInfo("note", "undo", -1);
// 	_bindings[0x9000 | 50] = MidiBindInfo("note", "overdub", -1);
// 	_bindings[0x9000 | 51] = MidiBindInfo("note", "redo", -1);
// 	_bindings[0x9000 | 52] = MidiBindInfo("note", "multiply", -1);

// 	_bindings[0x9000 | 54] = MidiBindInfo("note", "mute", -1);
// 	_bindings[0x9000 | 55] = MidiBindInfo("note", "scratch", -1);
// 	_bindings[0x9000 | 56] = MidiBindInfo("note", "reverse", -1);


// 	_bindings[0xb000 | 1] =  MidiBindInfo("set", "feedback", -1, 0.0, 1.0);
// 	_bindings[0xb000 | 7] =  MidiBindInfo("set", "scratch_pos", -1, 0.0, 1.0);
// 	_bindings[0xb000 | 74] =  MidiBindInfo("set", "dry", -1, 0.0, 1.0);
// 	_bindings[0xb000 | 71] =  MidiBindInfo("set", "wet", -1, 0.0, 1.0);

}

MidiBindings::~MidiBindings()
{

}

void
MidiBindings::get_bindings (BindingList & blist) const
{
	for (BindingsMap::const_iterator biter = _bindings.begin(); biter != _bindings.end(); ++biter) {
		const BindingList & elist = (*biter).second;
		
		for (BindingList::const_iterator eiter = elist.begin(); eiter != elist.end(); ++eiter) {
			const MidiBindInfo & info = (*eiter);
			
			blist.push_back (info);
		}
	}
}

bool
MidiBindings::get_channel_and_type(byte chcmd, int & ch, string & type) const
{
	ch = chcmd & 0x0f;
	byte cmd = chcmd & 0xf0;
	bool found = false;

	
	for (TypeMap::const_iterator titer = _typemap.begin(); titer != _typemap.end(); ++titer)
	{
		if ((*titer).second == cmd) {
			type = titer->first;
			found = true;
			break;
		}
	}

	return found;
}

int
MidiBindings::binding_key (const MidiBindInfo & info) const
{
	int typei;
	int key;

	TypeMap::const_iterator titer = _typemap.find(info.type);
	if (titer == _typemap.end()) {
		//cerr << "invalid midi type str: " << info.type << endl;
		return 0;
	}
	
	typei = titer->second;
	key = ((typei + info.channel) << 8) | info.param;

	return key;
}

bool
MidiBindings::add_binding (const MidiBindInfo & info, bool exclusive)
{
	///  type->typei is { pc = 0xc0 , cc = 0xb0 , on = 0x80 , n = 0x90 , pb = 0xe0 }
	// chcmd = cmd + ch
	// lookup key = (chcmd << 8) | param

	int key;
	
	if ((key = binding_key (info)) == 0) {
		return false;
	}

	BindingsMap::iterator biter = _bindings.find(key);
	if (biter == _bindings.end()) {
		_bindings.insert (BindingsMap::value_type ( key, BindingList()));
	}

	// check for others, and clear them if exclusive
	if (exclusive && _bindings[key].size() > 0) {
		//cerr << "cleared existing" << endl;
		_bindings[key].clear();
	}
	
	_bindings[key].push_back (info);
	// cerr << "added binding: " << info.type << "  "  << info.control << "  " << info.instance << "  " << info.lbound << "  " << info.ubound << endl;
	
	// cerr << "added binding: " << info.serialize() << endl;
	return true;
}

bool
MidiBindings::remove_binding (const MidiBindInfo & info)
{
	int key;
	
	if ((key = binding_key (info)) == 0) {
		return false;
	}

	BindingsMap::iterator biter = _bindings.find(key);
	if (biter == _bindings.end()) {
		return false;
	}

	BindingList & blist = biter->second;

	for (BindingList::iterator iter = blist.begin(); iter != blist.end(); ++iter) {
		MidiBindInfo & binfo = (*iter);
		if (binfo == info) {
			// cerr << "found match to remove" << endl;
			blist.erase(iter);
			break;
		}
	}

	if (blist.empty()) {
		_bindings.erase(biter);
	}
	
	return true;
}

void
MidiBindings::clear_bindings ()
{
	_bindings.clear();
}

// Moved to engine for easier access to initial values for toggle
bool
MidiBindings::load_bindings (string filename, bool append)
{
	//FILE * bindfile = 0;
	ifstream bindfile;

	bindfile.open(filename.c_str(), ios::in);
	
	if (!bindfile.is_open()) {
		cerr << "sooperlooper warning: could not open for reading: " << filename << endl;
		return false;
	}
	// todo: look for is in systemwide and ~/.sooperlooper/bindings/


	return load_bindings (bindfile, append);
}

bool
MidiBindings::load_bindings (std::istream & instream, bool append)
{
	char  line[200];

	
	if (!append) {
		clear_bindings();
	}

	while ( ! instream.eof())
		//while (fgets (line, sizeof(line), bindfile) != NULL)
		
	{
	        instream.getline (line, sizeof(line));
		
		// ignore empty lines and # lines
		if (line[0] == '\n' || line[0] == '#' || line[0] == '\0') {
			continue;
		}

		MidiBindInfo info;

		if (!info.unserialize(line)) {
			continue;
		}

		add_binding (info);
	}

	return true;
}

bool MidiBindings::save_bindings (string filename)
{
	ofstream bindfile (filename.c_str());

	if (!bindfile.is_open()) {
		cerr << "error: could not open for writing: " << filename << endl;
		return false;
	}

	return save_bindings(bindfile);
}

bool MidiBindings::save_bindings (std::ostream & outstream)
{
	
	for (BindingsMap::const_iterator biter = _bindings.begin(); biter != _bindings.end(); ++biter) {
		const BindingList & elist = (*biter).second;
		
		for (BindingList::const_iterator eiter = elist.begin(); eiter != elist.end(); ++eiter) {
			const MidiBindInfo & info = (*eiter);

			outstream << info.serialize() << endl;
		}
	}

	return true;
}



string MidiBindInfo::serialize() const
{
	//  ch type param   cmd  ctrl  instance  [min_val_bound max_val_bound valstyle min_data max_data]
	//
	//    ch = midi channel starting from 0
	//    type is one of:  'pc' = program change  'cc' = control change  'n' = note on/off
	//           'on' = note on  'off' = note off  'pb' = pitch bend
	//    param = # of midi parameter
	//
	//    cmd is one of ( note, down, up, set )
	//    ctrl is an SL control
	//    instance is loop #
	//    min_val_bound is what to treat midi val 0
	//    max_val_bound is what to treat midi val 127
	//    valstyle can be 'gain' or 'toggle'
	//    min_data and max_data can be integer values from 0 to 127

	char buf[100];
	char stylebuf[20];

	if (style == GainStyle) {
		snprintf(stylebuf, sizeof(stylebuf), "gain");
	} else if (style == ToggleStyle) {
		snprintf(stylebuf, sizeof(stylebuf), "toggle");
	} else if (style == IntegerStyle) {
		snprintf(stylebuf, sizeof(stylebuf), "integer");
	} else {
		strncpy(stylebuf, "norm", sizeof(stylebuf));
	}

	// i:ch s:type i:param  s:cmd  s:ctrl i:instance f:min_val_bound f:max_val_bound s:valstyle i:min_data i:max_data
	snprintf(buf, sizeof(buf), "%d %s %d  %s %s %d  %.9g %.9g  %s %d %d",
		 channel, type.c_str(), param, command.c_str(),
		 control.c_str(), instance, lbound, ubound, stylebuf, data_min, data_max);
	
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
	int dmin = 0;
	int dmax = 127;

	int ret;

	stylestr[0] = '\0';

	if (strval.empty()) return false;
	
	if ((ret = sscanf(strval.c_str(), "%d %5s %d  %30s %30s %d  %g %g  %10s %d %d",
			  &chan, tp, &parm, cmd, ctrl, &inst, &lb, &ub, stylestr, &dmin, &dmax)) < 5)
	{
		cerr << "ret: " << ret << " invalid input line: " << strval << endl;
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
	data_min = dmin;
	data_max = dmax;

	if (stylestr[0] == 'g') {
		style = GainStyle;
	}
	else if (stylestr[0] == 't') {
		style = ToggleStyle;
	}
	else if (stylestr[0] == 'i') {
		style = IntegerStyle;
	} else {
		style = NormalStyle;
	}
	
	return true;
}
