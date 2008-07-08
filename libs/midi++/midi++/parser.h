/*
    Copyright (C) 1998 Paul Barton-Davis
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#ifndef  __midi_parse_h__
#define  __midi_parse_h__

#include <string>
#include <iostream>

#include <sigc++/sigc++.h>
#include <midi++/types.h>

namespace MIDI {

class Port;
class Parser;

typedef SigC::Signal2<void, Parser &, byte>                 OneByteSignal;
typedef SigC::Signal2<void, Parser &, EventTwoBytes *>      TwoByteSignal;
typedef SigC::Signal2<void, Parser &, pitchbend_t>          PitchBendSignal;
typedef SigC::Signal3<void, Parser &, byte *, size_t> Signal;
typedef SigC::Signal4<void, Parser &, byte *, size_t, timestamp_t> TimestampedSignal;

class Parser : public SigC::Object {
 public:
	Parser (Port &p);
	~Parser ();
	
	/* signals that anyone can connect to */
	
	OneByteSignal         bank_change;
	TwoByteSignal         note_on;
	TwoByteSignal         note_off;
	TwoByteSignal         poly_pressure;
	OneByteSignal         pressure;
	OneByteSignal         program_change;
	PitchBendSignal       pitchbend;
	TwoByteSignal         controller;

	OneByteSignal         channel_bank_change[16];
	TwoByteSignal         channel_note_on[16];
	TwoByteSignal         channel_note_off[16];
	TwoByteSignal         channel_poly_pressure[16];
	OneByteSignal         channel_pressure[16];
	OneByteSignal         channel_program_change[16];
	PitchBendSignal       channel_pitchbend[16];
	TwoByteSignal         channel_controller[16];
	SigC::Signal1<void, Parser &>          channel_active_preparse[16];
	SigC::Signal1<void, Parser &>          channel_active_postparse[16];

	OneByteSignal         mtc_quarter_frame;

	TimestampedSignal     raw_preparse;
	Signal                raw_postparse;
	TimestampedSignal     any;
	Signal                sysex;
	Signal                mmc;
	Signal                mtc;
	Signal                position;
	Signal                song;
	SigC::Signal1<void, Parser &>          all_notes_off;
	SigC::Signal1<void, Parser &>          tune;
	SigC::Signal1<void, Parser &>          timing;
	SigC::Signal1<void, Parser &>          start;
	SigC::Signal1<void, Parser &>          stop;
	SigC::Signal1<void, Parser &>          contineu;  /* note spelling */
	SigC::Signal1<void, Parser &>          active_sense;
	SigC::Signal1<void, Parser &>          reset;
	SigC::Signal1<void, Parser &>          eox;

	/* This should really be protected, but then derivatives of Port
	   can't access it.
	*/

	void scanner (byte c);

	size_t *message_counts() { return message_counter; }
	const char *midi_event_type_name (MIDI::eventType);
	void trace (bool onoff, std::ostream *o, const std::string &prefix = "");
	bool tracing() { return trace_stream != 0; }
	Port &port() { return _port; }

	SigC::Signal2<int, byte *, size_t> edit;

	void set_mmc_forwarding (bool yn) {
		_mmc_forward = yn;
	}

	/* MTC */

	enum MTC_Status {
		MTC_Stopped = 0,
		MTC_Forward,
		MTC_Backward
	};

	MTC_FPS mtc_fps() const { return _mtc_fps; }
	MTC_Status  mtc_running() const { return _mtc_running; }
	const byte *mtc_current() const { return _mtc_time; }
	bool        mtc_locked() const  { return _mtc_locked; }

	SigC::Signal1<void,MTC_Status> mtc_status;
	SigC::Signal0<bool>            mtc_skipped;
	SigC::Signal2<void,const byte*,bool> mtc_time;

	void set_mtc_forwarding (bool yn) {
		_mtc_forward = yn;
	}

	void reset_mtc_state ();
	
  private:
	Port &_port;
	/* tracing */

	std::ostream *trace_stream;
	std::string trace_prefix;
	void trace_event (Parser &p, byte *msg, size_t len, timestamp_t ts);
	SigC::Connection trace_connection;

	size_t message_counter[256];

	enum ParseState { 
		NEEDSTATUS,
		NEEDONEBYTE,
		NEEDTWOBYTES,
		VARIABLELENGTH
	};
	ParseState state;
	unsigned char *msgbuf;
	int msglen;
	int msgindex;
	MIDI::eventType msgtype;
	channel_t channel;
	bool runnable;
	bool _mmc_forward;
	bool _mtc_forward;
	int   expected_mtc_quarter_frame_code;
	byte _mtc_time[4];
	byte _qtr_mtc_time[4];
	unsigned long consecutive_qtr_frame_cnt;
	MTC_FPS _mtc_fps;
	MTC_Status _mtc_running;
	bool       _mtc_locked;
	byte last_qtr_frame;

	void channel_msg (byte);
	void realtime_msg (byte);
	void system_msg (byte);
	void signal (byte *msg, size_t msglen);
	bool possible_mmc (byte *msg, size_t msglen);
	bool possible_mtc (byte *msg, size_t msglen);
	void process_mtc_quarter_frame (byte *msg);

	void handle_preparse(Parser &, byte *, size_t, timestamp_t);
	timestamp_t _timestamp;
};

}; /* namespace MIDI */

#endif   // __midi_parse_h__

