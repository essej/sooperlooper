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

#ifndef __sooperlooper_midi_bridge__
#define __sooperlooper_midi_bridge__

#include <stdint.h>
#include <lo/lo.h>

#include <cstdio>

#include <string>
#include <vector>
#include <map>

namespace SooperLooper {

class MidiBridge
{
  public:

	MidiBridge (std::string name, std::string oscurl);
	virtual ~MidiBridge();			

	virtual bool is_ok() {return false;}
	
	virtual void clear_bindings ();
	virtual bool load_bindings (std::string filename);

	
  protected:

	void queue_midi (int chcmd, int param, int val);
	
	
	std::string _name;
	std::string _oscurl;

  private:

	struct EventInfo
	{
		enum Style {
			NormalStyle = 0,
			GainStyle,
		};
		
		EventInfo() {}
		EventInfo (std::string tp, std::string cmd, int instc, float lbnd=0.0f, float ubnd=1.0f, Style styl=NormalStyle)
			: type(tp), command(cmd), instance(instc), lbound(lbnd), ubound(ubnd), style(styl) {}

		std::string type;
		std::string command;
		int         instance;

		float       lbound;
		float       ubound;

		Style       style;
	};

	void send_osc (EventInfo & info, float val);
	
	std::FILE * search_open_file (std::string filename);

	typedef std::vector<EventInfo> EventList;
	typedef std::map<int, EventList> BindingsMap;

	std::map<std::string, int> _typemap;
	
	BindingsMap _bindings;

	lo_address _addr;

};

};


#endif
