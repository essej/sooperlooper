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

#ifndef __sooperlooper_command_map__
#define __sooperlooper_command_map__

#include <string>
#include <map>
#include <list>
#include "event.hpp"

namespace SooperLooper {

class CommandMap
{
public:

	static CommandMap & instance() {
		if (!_instance) {
			_instance = new CommandMap();
		}
		return *_instance;
	}
	
	inline Event::command_t  to_command_t (std::string cmd);
	inline std::string       to_command_str (Event::command_t cmd);
	
	inline Event::control_t  to_control_t (std::string cmd);
	inline std::string       to_control_str (Event::control_t cmd);

	inline Event::type_t  to_type_t (std::string cmd);
	inline std::string       to_type_str (Event::type_t cmd);

	void get_commands (std::list<std::string> & cmdlist);
	void get_controls (std::list<std::string> & ctrllist);

	bool is_command (std::string cmd) { return _str_cmd_map.find(cmd) != _str_cmd_map.end(); }
	bool is_control (std::string ctrl) { return _str_ctrl_map.find(ctrl) != _str_ctrl_map.end(); }

	bool is_input_control (std::string ctrl) { return _input_controls.find(ctrl) != _input_controls.end(); }
	bool is_output_control (std::string ctrl) { return _output_controls.find(ctrl) != _output_controls.end(); }
	bool is_event_control (std::string ctrl) { return _event_controls.find(ctrl) != _event_controls.end(); }
	bool is_global_control (std::string ctrl) { return _global_controls.find(ctrl) != _global_controls.end(); }
	
protected:
	CommandMap();
	virtual ~CommandMap() {}
	
	static CommandMap * _instance;

	typedef std::map<std::string, Event::command_t> StringCommandMap;
	StringCommandMap _str_cmd_map;
	
	typedef std::map<Event::command_t, std::string> CommandStringMap;
	CommandStringMap _cmd_str_map;
	
	typedef std::map<std::string, Event::control_t> StringControlMap;
	StringControlMap _str_ctrl_map;

	typedef std::map<Event::control_t, std::string> ControlStringMap;
	ControlStringMap _ctrl_str_map;

	typedef	std::map<std::string,Event::type_t> StringTypeMap;
	StringTypeMap _str_type_map;
	
	typedef	std::map<Event::type_t,std::string> TypeStringMap;
	TypeStringMap _type_str_map;
	
	
	StringControlMap _input_controls;
	StringControlMap _output_controls;
	StringControlMap _event_controls;
	StringControlMap _global_controls;
};


inline Event::command_t  CommandMap::to_command_t (std::string cmd)
{
	StringCommandMap::iterator result = _str_cmd_map.find(cmd);

	if (result == _str_cmd_map.end()) {
		return Event::UNKNOWN;
	}

	return (*result).second;
}

inline std::string  CommandMap::to_command_str (Event::command_t cmd)
{
	CommandStringMap::iterator result = _cmd_str_map.find(cmd);

	if (result == _cmd_str_map.end()) {
		return "unknown";
	}

	return (*result).second;
}


inline Event::control_t
CommandMap::to_control_t (std::string cmd)
{
	StringControlMap::iterator result = _str_ctrl_map.find(cmd);

	if (result == _str_ctrl_map.end()) {
		return Event::Unknown;
	}

	return (*result).second;
}

inline std::string
CommandMap::to_control_str (Event::control_t cmd)
{
	ControlStringMap::iterator result = _ctrl_str_map.find(cmd);

	if (result == _ctrl_str_map.end()) {
		return "unknown";
	}

	return (*result).second;
}

inline Event::type_t
CommandMap::to_type_t (std::string tp)
{
	StringTypeMap::iterator result = _str_type_map.find(tp);

	if (result == _str_type_map.end()) {
		return Event::type_cmd_hit; // arbitrary
	}

	return (*result).second;
}
	
inline std::string
CommandMap::to_type_str (Event::type_t tp)
{
	TypeStringMap::iterator result = _type_str_map.find(tp);

	if (result == _type_str_map.end()) {
		return "unknown";
	}

	return (*result).second;
}
	

	
};

#endif
