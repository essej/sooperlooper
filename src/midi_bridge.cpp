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


#include "midi_bridge.hpp"

#include <iostream>
#include <cstdio>

using namespace SooperLooper;
using namespace std;


MidiBridge::MidiBridge (string name, string oscurl)
	: _name (name), _oscurl(oscurl)
{

	_addr = lo_address_new_from_url (_oscurl.c_str());
	if (lo_address_errno (_addr) < 0) {
		fprintf(stderr, "MidiBridge:: addr error %d: %s\n", lo_address_errno(_addr), lo_address_errstr(_addr));
	}


	_typemap["cc"] = 0xb0;
	_typemap["n"] = 0x90;
	_typemap["pc"] = 0xc0;

	// temp bindings
// 	_bindings[0x9000 | 48] = EventInfo("note", "record", -1);
// 	_bindings[0x9000 | 49] = EventInfo("note", "undo", -1);
// 	_bindings[0x9000 | 50] = EventInfo("note", "overdub", -1);
// 	_bindings[0x9000 | 51] = EventInfo("note", "redo", -1);
// 	_bindings[0x9000 | 52] = EventInfo("note", "multiply", -1);

// 	_bindings[0x9000 | 54] = EventInfo("note", "mute", -1);
// 	_bindings[0x9000 | 55] = EventInfo("note", "scratch", -1);
// 	_bindings[0x9000 | 56] = EventInfo("note", "reverse", -1);


// 	_bindings[0xb000 | 1] =  EventInfo("set", "feedback", -1, 0.0, 1.0);
// 	_bindings[0xb000 | 7] =  EventInfo("set", "scratch_pos", -1, 0.0, 1.0);
// 	_bindings[0xb000 | 74] =  EventInfo("set", "dry", -1, 0.0, 1.0);
// 	_bindings[0xb000 | 71] =  EventInfo("set", "wet", -1, 0.0, 1.0);
	
}

MidiBridge::~MidiBridge()
{
	lo_address_free (_addr);
}


void
MidiBridge::clear_bindings ()
{
	_bindings.clear();
}

FILE *
MidiBridge::search_open_file (std::string filename)
{
	FILE *bindfile = 0;
	
	if ((bindfile = fopen(filename.c_str(), "r")) == NULL) {
		cerr << "error: could not open " << filename << endl;
	}

	// todo: look for it in systemwide and ~/.sooperlooper/bindings/

	return bindfile;
}


bool
MidiBridge::load_bindings (std::string filename)
{
	//  ch:cmds:param   type  ctrl  instance  min_val_bound max_val_bound
	// cmds is one of:  'pc' = program change  'cc' = control change  'n' = note on/off

	// cmd is { pc = 0xc0 , cc = 0xb0 , on = 0x80 , n = 0x90  }
	// chcmd = cmd + ch
	// lookup key = (chcmd << 8) | param
	
	FILE * bindfile = 0;
	char  line[200];

	char cmds[10];
	int chan;
	int param;
	char type[32];
	int typei;
	char ctrl[32];
	int  instance = -1;
	float lbound = 0.0f;
	float ubound = 1.0f;
	int key;
	int ret;

	if ((bindfile = search_open_file(filename)) == NULL) {
		cerr << "error: could not open " << filename << endl;
		return false;
	}

	while (fgets (line, sizeof(line), bindfile) != NULL)
	{
		instance = -1;
		lbound = 0.0f;
		ubound = 1.0f;

		// ignore empty lines and # lines
		if (line[0] == '\n' || line[0] == '#') {
			continue;
		}
		
		if ((ret=sscanf (line, "%d %3s %d  %30s  %30s  %d  %f  %f",
			    &chan, cmds, &param, type, ctrl, &instance, &lbound, &ubound) < 5)) {
			cerr << "ret: " << ret << " invalid input line: " << line;
			continue;
		}

		if (_typemap.find(cmds) == _typemap.end()) {
			cerr << "invalid midi type str: " << cmds;
			continue;
		}

		typei = _typemap[cmds];
		key = ((typei + chan) << 8) | param;
		
		_bindings[key] =  EventInfo(type, ctrl, instance, lbound, ubound);
		cerr << "added binding: " << type << "  "  << ctrl << "  " << instance << "  " << lbound << "  " << ubound << endl;
	}

	fclose(bindfile);

	return true;
}

	
void
MidiBridge::queue_midi (int chcmd, int param, int val)
{
	// convert midi to lookup key
	// lookup key = (chcmd << 8) | param

	int key = (chcmd << 8) | param;

	BindingsMap::iterator iter = _bindings.find(key);
	
	if (iter != _bindings.end())
	{
		EventInfo & info = (*iter).second;

		float scaled_val = (float) ((val/127.0) *  ( info.ubound - info.lbound)) + info.lbound;

		cerr << "found binding: val is " << val << "  scaled: " << scaled_val << endl;
		
		send_osc (info, scaled_val);
		
	}
	else {
		fprintf(stderr, "binding %x not found\n", key);
	}
}


void
MidiBridge::send_osc (EventInfo & info, float val)
{
	static char tmpbuf[100];


	string type = info.type;
	
	if (info.type == "set") {
		snprintf (tmpbuf, sizeof(tmpbuf)-1, "/sl/%d/%s", info.instance, info.type.c_str());

		if (lo_send(_addr, tmpbuf, "sf", info.command.c_str(), val) < 0) {
			fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(_addr), lo_address_errstr(_addr));
		}
	}
	else {
		if (type == "note") {
			if (val > 0.0f) {
				cerr << "val is " << val << endl;
				type = "down";
			}
			else {
				type = "up";
			}
		}

		snprintf (tmpbuf, sizeof(tmpbuf)-1, "/sl/%d/%s", info.instance, type.c_str());
		
		if (lo_send(_addr, tmpbuf, "s", info.command.c_str()) < 0) {
			fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(_addr), lo_address_errstr(_addr));
		}
	}
}
