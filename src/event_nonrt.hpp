/*
** Copyright (C) 2004 Jesse Chappell <jesse@essej.net>
**              and Benno Senoner and Christian Schoenebeck
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

#ifndef __sooperlooper_event_nonrt__
#define __sooperlooper_event_nonrt__

#include <stdint.h>
#include <string>

#include "event.hpp"

namespace SooperLooper {

	class EventNonRT {
	protected:
		EventNonRT() {};

		virtual void dummy(){};
	};


	class ConfigLoopEvent : public EventNonRT
	{
	public:
		enum Type {
			Add,
			Remove
		} type;

		ConfigLoopEvent(Type tp, int chans=1, float sec=0.0f, int ind=0)
			: type(tp), channels(chans), secs(sec), index(ind) {}

		virtual ~ConfigLoopEvent() {}

		int channels;
		float secs;
		int index;
	};

	class LoopFileEvent : public EventNonRT
	{
	public:
		enum Type {
			Load,
			Save
		} type;

		LoopFileEvent(Type tp, int inst, std::string fname,  std::string returl, std::string retpath)
			: type(tp), instance(inst), filename(fname), ret_url(returl), ret_path(retpath) {}

		virtual ~LoopFileEvent() {}

		int instance;
		std::string      filename;
		std::string      ret_url;
		std::string      ret_path;
	};
	
	class GetParamEvent : public EventNonRT
	{
	public:
		GetParamEvent( int8_t inst, Event::control_t ctrl, std::string returl, std::string retpath)
			: control(ctrl), instance(inst), ret_url(returl), ret_path(retpath) {}
		virtual ~GetParamEvent() {}
		
		Event::control_t       control;
		int8_t           instance;
		std::string      ret_url;
		std::string      ret_path;

		float            ret_value;
	};

	class ConfigUpdateEvent : public EventNonRT
	{
	public:

		enum Type
		{
			Register,
			Unregister,
			Send
		} type;

		ConfigUpdateEvent(Type tp, int8_t inst,  Event::control_t ctrl, std::string returl="", std::string retpath="", float val=0.0)
			: type(tp), control(ctrl), instance(inst), ret_url(returl), ret_path(retpath), value(val) {}
		virtual ~ConfigUpdateEvent() {}

		
		Event::control_t       control;
		int8_t           instance;
		std::string      ret_url;
		std::string      ret_path;
		float            value;
	};

	class PingEvent : public EventNonRT
	{
	public:
		PingEvent(std::string returl, std::string retpath)
			: ret_url(returl), ret_path(retpath) {}

		virtual ~PingEvent() {}

		std::string  ret_url;
		std::string  ret_path;
	};

	class RegisterConfigEvent : public EventNonRT
	{
	public:
		enum Type
		{
			Register,
			Unregister
		} type;

		RegisterConfigEvent(Type tp, std::string returl, std::string retpath)
			: type(tp), ret_url(returl), ret_path(retpath) {}

		virtual ~RegisterConfigEvent() {}

		std::string  ret_url;
		std::string  ret_path;
	};
	
	
} // namespace SooperLooper

#endif 
