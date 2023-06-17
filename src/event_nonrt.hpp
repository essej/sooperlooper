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
	public:
		virtual ~EventNonRT() {}
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

		ConfigLoopEvent(Type tp, int chans=1, float sec=0.0f, int ind=0, int dis=1)
			: type(tp), channels(chans), secs(sec), index(ind), discrete(dis) {}

		virtual ~ConfigLoopEvent() {}

		int channels;
		float secs;
		int index;
		int discrete;
	};

	class SessionEvent : public EventNonRT
	{
	public:
		enum Type {
			Load,
			Save
		} type;

		SessionEvent(Type tp, std::string fname, std::string returl, std::string retpath, bool audio=false) 
			: type(tp), filename(fname), write_audio(audio), ret_url(returl), ret_path(retpath) {}

		virtual ~SessionEvent() {}

		std::string      filename;
		bool             write_audio;
		std::string      ret_url;
		std::string      ret_path;
	};

	
	class LoopFileEvent : public EventNonRT
	{
	public:
		enum Type {
			Load,
			Save
		} type;

		enum FileFormat
		{
			FormatFloat = 0,
			FormatPCM16,
			FormatPCM24,
			FormatPCM32
		};

		enum Endian
		{
			LittleEndian = 0,
			BigEndian
		};
	
		
		LoopFileEvent(Type tp, int inst, std::string fname, std::string returl, std::string retpath,
			      FileFormat fmt=FormatFloat, Endian end=LittleEndian)
			: type(tp), instance(inst), filename(fname), format(fmt), endian(end), ret_url(returl), ret_path(retpath) {}

		virtual ~LoopFileEvent() {}

		int instance;
		std::string      filename;
		FileFormat       format;
		Endian           endian;
		std::string      ret_url;
		std::string      ret_path;
	};
	
	class GetParamEvent : public EventNonRT
	{
	public:
		GetParamEvent( int8_t inst, Event::control_t ctrl, std::string returl, std::string retpath)
			: control(ctrl), instance(inst), ret_url(returl), ret_path(retpath), ret_value(0.0f) {}
		virtual ~GetParamEvent() {}
		
		Event::control_t       control;
		int8_t           instance;
		std::string      ret_url;
		std::string      ret_path;

		float            ret_value;
	};

	class GetPropertyEvent : public EventNonRT
	{
	public:
		GetPropertyEvent( int8_t inst, std::string prop, std::string returl, std::string retpath)
			: property(prop), instance(inst), ret_url(returl), ret_path(retpath) {}
		virtual ~GetPropertyEvent() {}

		std::string      property;
		int8_t           instance;
		std::string      ret_url;
		std::string      ret_path;

		std::string      ret_value;
	};

	class SetPropertyEvent : public EventNonRT
	{
	public:
		SetPropertyEvent(int8_t inst, std::string par, std::string val)
			: property(par), instance(inst), value(val) {}
		virtual ~SetPropertyEvent() {}

		std::string      property;
		int8_t           instance;
		std::string      value;
	};

	class ConfigUpdateEvent : public EventNonRT
	{
	public:

		enum Type
		{
			Register,
			Unregister,
			Send,
			RegisterAuto,
			UnregisterAuto,
			RegisterCmd,
			UnregisterCmd,
			SendCmd,
		} type;

		ConfigUpdateEvent(Type tp, int8_t inst,  Event::control_t ctrl, std::string returl="", std::string retpath="",float val=0.0, int src=-1, short int ms=0)
			: type(tp), control(ctrl), instance(inst), ret_url(returl), ret_path(retpath), value(val), source(src),update_time_ms(ms) {}
		ConfigUpdateEvent(Type tp, int8_t inst,  Event::command_t cmd, std::string returl="", std::string retpath="", int src=-1)
			: type(tp), command(cmd), instance(inst), ret_url(returl), ret_path(retpath), value(0.0f), source(src) {}

		virtual ~ConfigUpdateEvent() {}

		
		Event::control_t       control;
		Event::command_t       command;
		int8_t                 instance;
		std::string            ret_url;
		std::string            ret_path;
		float                  value;
		int                    source;
		short int              update_time_ms;
	};

	class PingEvent : public EventNonRT
	{
	public:
		PingEvent(std::string returl, std::string retpath, bool useid)
			: ret_url(returl), ret_path(retpath), use_id(useid) {}

		virtual ~PingEvent() {}

		std::string  ret_url;
		std::string  ret_path;
		bool         use_id;
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


	class GlobalGetEvent : public EventNonRT
	{
	public:
		GlobalGetEvent(std::string par, std::string returl, std::string retpath)
			: param(par), ret_url(returl), ret_path(retpath), ret_value(0.0f) {}
		virtual ~GlobalGetEvent() {}
		
		std::string      param;
		std::string      ret_url;
		std::string      ret_path;

		float            ret_value;
	};

	class GlobalSetEvent : public EventNonRT
	{
	public:
		GlobalSetEvent(std::string par, float val)
			: param(par), value(val) {}
		virtual ~GlobalSetEvent() {}
		
		std::string      param;
		float            value;
	};

	class MidiBindingEvent : public EventNonRT
	{
	public:
		enum Type
		{
			GetAll,
			Remove,
			Add,
			Clear,
			Load,
			Save,
			Learn,
			GetNextMidi,
			CancelLearn,
			CancelGetNext
		} type;

		MidiBindingEvent(Type tp, std::string bindstr, std::string opt, std::string returl="", std::string retpath="")
			: type(tp), bind_str(bindstr), options(opt), ret_url(returl), ret_path(retpath) {}
		MidiBindingEvent() {}
		virtual ~MidiBindingEvent() {}
		
		std::string      bind_str;
		std::string      options;
		std::string      ret_url;
		std::string      ret_path;
	};
	
	
} // namespace SooperLooper

#endif 
