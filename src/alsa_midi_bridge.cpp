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

#include "alsa_midi_bridge.hpp"
#include <pthread.h>

using namespace SooperLooper;
using namespace std;

AlsaMidiBridge::AlsaMidiBridge (string name, string oscurl)
	: MidiBridge (name, oscurl), _seq(0)
{
	_done = false;
	
	if ((_seq = create_sequencer (name, true)) == 0) {
		return;
	}
	
	
	pthread_create (&_midi_thread, NULL, &AlsaMidiBridge::_midi_receiver, this);
}

AlsaMidiBridge::~AlsaMidiBridge()
{
	stop_midireceiver();
}

snd_seq_t *
AlsaMidiBridge::create_sequencer (string client_name, bool isinput)
{
	snd_seq_t * seq;
	int err;
	
	if ((err = snd_seq_open (&seq, "default", SND_SEQ_OPEN_DUPLEX, 0)) != 0) {
		fprintf (stderr, "Could not open ALSA sequencer, aborting\n\n%s\n\n"
			   "Make sure you have configure ALSA properly and that\n"
			   "/proc/asound/seq/clients exists and contains relevant\n"
			   "devices (%s).", 
			   snd_strerror (err));
		return 0;
	}
	
	snd_seq_set_client_name (seq, client_name.c_str());
	
	if ((err = snd_seq_create_simple_port (seq, isinput? "Input" : "Output",
					       (isinput? SND_SEQ_PORT_CAP_WRITE: SND_SEQ_PORT_CAP_READ)| SND_SEQ_PORT_CAP_DUPLEX |
					       SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_SUBS_WRITE,
					       SND_SEQ_PORT_TYPE_APPLICATION|SND_SEQ_PORT_TYPE_SPECIFIC)) != 0) {
		fprintf (stderr, "Could not create ALSA port: %s", snd_strerror (err));
		snd_seq_close(seq);
		return 0;
	}
	
	return seq;
}
	

void * AlsaMidiBridge::_midi_receiver(void *arg)
{
	AlsaMidiBridge * bridge = static_cast<AlsaMidiBridge*> (arg);

	bridge->midi_receiver();
	return 0;
}

void  AlsaMidiBridge::midi_receiver()
{
	snd_seq_event_t *event;
	int val;

	while (!_done) {

		snd_seq_event_input (_seq, &event);

		if (_done) {
			break;
		}

		switch(event->type){
		case SND_SEQ_EVENT_NOTEON:
			queue_midi(0x90+event->data.note.channel,event->data.note.note,event->data.note.velocity);
			printf("Noteon, channel: %d note: %d vol: %d\n",event->data.note.channel,event->data.note.note,event->data.note.velocity);
			break;
		case SND_SEQ_EVENT_NOTEOFF:
			queue_midi(0x90+event->data.note.channel,event->data.note.note,0);
			printf("Noteoff, channel: %d note: %d vol: %d\n",event->data.note.channel,event->data.note.note,event->data.note.velocity);
			break;
		case SND_SEQ_EVENT_KEYPRESS:
			//printf("Keypress, channel: %d note: %d vol: %d\n",event->data.note.channel,event->data.note.note,event->data.note.velocity);
			queue_midi(0xa0+event->data.note.channel,event->data.note.note,event->data.note.velocity);
			break;
		case SND_SEQ_EVENT_CONTROLLER:
			queue_midi(0xb0+event->data.control.channel,event->data.control.param,event->data.control.value);
			printf("Control: %d %d %d\n",event->data.control.channel,event->data.control.param,event->data.control.value);
			break;
		case SND_SEQ_EVENT_PITCHBEND:
			val=event->data.control.value + 0x2000;
			queue_midi(0xe0+event->data.control.channel,val&127,val>>7);
			//printf("Pitch: %d %d %d\n",event->data.control.channel,event->data.control.param,event->data.control.value);
			break;
		case SND_SEQ_EVENT_CHANPRESS:
			//printf("chanpress: %d %d %d\n",event->data.control.channel,event->data.control.param,event->data.control.value);
			queue_midi(0xc0+event->data.control.channel,event->data.control.value,0);
			break;
		case SND_SEQ_EVENT_PGMCHANGE:
			printf("pgmchange: %d %d %d\n",event->data.control.channel,event->data.control.param,event->data.control.value);
			queue_midi(0xc0+event->data.control.channel,event->data.control.value,0);
			break;
		default:
			//printf("Unknown type: %d\n",event->type);
			break;
		}
	}
	
}


void AlsaMidiBridge::stop_midireceiver ()
{
	int err; 
	snd_seq_event_t event;
	snd_seq_t *seq2 = create_sequencer ("slquit", true);

	
	_done = true;

	if (seq2 == 0) {
		// oh well
		return;
	}
	
	snd_seq_connect_to (seq2, 0, snd_seq_client_id (_seq),0);
	snd_seq_ev_clear      (&event);
	snd_seq_ev_set_direct (&event);
	snd_seq_ev_set_subs   (&event);
	snd_seq_ev_set_source (&event, 0);
	snd_seq_ev_set_controller (&event,1,0x80,50);
	
	if ((err = snd_seq_event_output (seq2, &event)) < 0) {
		fprintf (stderr, "cannot send stop event to midi thread: %s\n",
			   snd_strerror (err));
	}

	snd_seq_drain_output (seq2);
	snd_seq_close (seq2);
	pthread_join (_midi_thread,NULL);
	snd_seq_close (_seq);

	_seq = 0;
}
