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

#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <getopt.h>

#include <cstdlib>

#include <pbd/transmitter.h>

#include "version.h"

#include "control_osc.hpp"
#include "engine.hpp"
#include "event_nonrt.hpp"

#include "midi_bridge.hpp"
#include "command_map.hpp"
#include <midi++/port_request.h>

// #if WITH_ALSA
// #include "alsa_midi_bridge.hpp"
// #elif WITH_COREMIDI
// #include "coremidi_bridge.hpp"
// #endif

#include "jack_audio_driver.hpp"

using namespace SooperLooper;
using namespace std;

//extern void sl_init ();
//extern	void sl_fini ();

Engine * engine = 0;
int do_shutdown = 0;

Transmitter  warning (Transmitter::Warning);
Transmitter  error (Transmitter::Error);


#define DEFAULT_OSC_PORT 9951
#define DEFAULT_LOOP_TIME 40.0f


char *optstring = "c:l:j:p:m:t:U:S:D:L:qVh";

struct option long_options[] = {
	{ "help", 0, 0, 'h' },
	{ "quiet", 0, 0, 'q' },
	{ "channels", 1, 0, 'c' },
	{ "loopcount", 1, 0, 'l' },
	{ "looptime", 1, 0, 't' },
	{ "load-session", 1, 0, 'L' },
	{ "discrete-io", 1, 0, 'D' },
	{ "osc-port", 1, 0, 'p' },
	{ "jack-name", 1, 0, 'j' },
	{ "jack-server-name", 1, 0, 'S' },
	{ "load-midi-binding", 1, 0, 'm' },
	{ "ping-url", 1, 0, 'U' },
	{ "version", 0, 0, 'V' },
	{ 0, 0, 0, 0 }
};


struct OptionInfo
{
	OptionInfo() :
		loop_count(1), channels(2), quiet(false), jack_name(""),
		oscport(DEFAULT_OSC_PORT), loopsecs(DEFAULT_LOOP_TIME), discrete_io(true),
		show_usage(0), show_version(0), pingurl() {} 
		
	int loop_count;
	int channels;
	bool quiet;
	string jack_name;
	string jack_server_name;
	int oscport;
	string bindfile;
	float loopsecs;
	bool  discrete_io;
	
	int show_usage;
	int show_version;
	string pingurl;
	string loadsession;
};


static void usage(char *argv0)
{
	fprintf(stderr, "SooperLooper %s\nCopyright 2004-2023 Jesse Chappell\nSooperLooper comes with ABSOLUTELY NO WARRANTY\n", sooperlooper_version);
	fprintf(stderr, "This is free software, and you are welcome to redistribute it\n");
	fprintf(stderr, "under certain conditions; see the file COPYING for details\n\n");

	fprintf(stderr, "Usage: %s [options...]\n", argv0);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -l <num> , --loopcount=<num> number of loopers to create (default is 1)\n");
	fprintf(stderr, "  -c <num> , --channels=<num>  channel count for each looper (default is 2)\n");
	fprintf(stderr, "  -t <numsecs> , --looptime=<num>  number of seconds of loop memory per channel (default is %g), at least\n", DEFAULT_LOOP_TIME);
	fprintf(stderr, "  -L <pathname> , --load-session=<pathname> load initial session from pathname\n");
	fprintf(stderr, "  -D <yes/no>, --discrete-io=[yes]  initial loops should have discrete input and output ports (default yes)\n");
	fprintf(stderr, "  -p <num> , --osc-port=<num>  udp port number for OSC server (default is %d)\n", DEFAULT_OSC_PORT);
	fprintf(stderr, "  -j <str> , --jack-name=<str> jack client name, default is sooperlooper\n");
	fprintf(stderr, "  -S <str> , --jack-server-name=<str> specify jack server name\n");
	fprintf(stderr, "  -m <str> , --load-midi-binding=<str> loads midi binding from file or preset\n");
	fprintf(stderr, "  -q , --quiet                 do not output status to stderr\n");
	fprintf(stderr, "  -h , --help                  this usage output\n");
	fprintf(stderr, "  -V , --version               show version only\n");

}

static void parse_options (int argc, char **argv, OptionInfo & option_info)
{
	int longopt_index = 0;
	int c;
	
	while ((c = getopt_long (argc, argv, optstring, long_options, &longopt_index)) >= 0) {
		if (c >= 255) {
			break;
		}
		
		switch (c) {
		case 1:
			/* getopt signals end of '-' options */
			break;

		case 'h':
			option_info.show_usage++;
			break;
		case 'V':
			option_info.show_version++;
			break;
		case 'q':
			option_info.quiet = true;
			break;
		case 'j':
			option_info.jack_name = optarg;
			break;
		case 'S':
			option_info.jack_server_name = optarg;
			break;
		case 'm':
			option_info.bindfile = optarg;
			break;
		case 'c':
			option_info.channels = (unsigned int) atoi(optarg);
			break;
		case 'l':
			option_info.loop_count = atoi(optarg);
			break;
		case 't':
			sscanf(optarg, "%f", &option_info.loopsecs);
			break;
		case 'p':
			option_info.oscport = atoi(optarg);
			break;
		case 'D':
			option_info.discrete_io = (string("no").compare(optarg) != 0);
			break;
		case 'U':
			option_info.pingurl = optarg;
			break;
		case 'L':
			option_info.loadsession = optarg;
			break;
		default:
			fprintf (stderr, "argument error: %d\n", c);
			option_info.show_usage++;
			break;
		}

		if (option_info.show_usage > 0) {
			break;
		}
	}
}


static void* watchdog_thread(void* arg)
{
  sigset_t signalset;
  int signalno;
  int exiting = 0;
  
  while (!exiting)
  {
	  sigemptyset(&signalset);
	  
	  /* handle the following signals explicitly */
	  sigaddset(&signalset, SIGTERM);
	  sigaddset(&signalset, SIGINT);
	  sigaddset(&signalset, SIGHUP);
	  sigaddset(&signalset, SIGPIPE);
	  sigaddset(&signalset, SIGABRT);
	  
	  /* block until a signal received */
	  sigwait(&signalset, &signalno);
	  
	  fprintf (stderr, "recieved signal %d\n", signalno);
	  
	  if (signalno == SIGHUP) {
		  exiting = 1;
	  }
	  else {
		  exiting = 1;
	  }
  }


  if (engine) {
	  engine->quit();
  }
  
  /* to keep the compilers happy; never actually executed */
  return(0);
}

/**
 * Sets up a signal mask with sigaction() that blocks 
 * all common signals, and then launces an watchdog
 * thread that waits on the blocked signals using
 * sigwait().
 */
static void setup_signals()
{
	pthread_t watchdog;
	int res;
	
	/* man pthread_sigmask:
	 *  "...signal actions and signal handlers, as set with
	 *   sigaction(2), are shared between all threads"
	 */
	
	struct sigaction blockaction;
	blockaction.sa_handler = SIG_IGN;
	sigemptyset(&blockaction.sa_mask);
	blockaction.sa_flags = 0;
	
	/* ignore the following signals */
	sigaction(SIGTERM, &blockaction, 0);
	sigaction(SIGINT, &blockaction, 0);
	sigaction(SIGHUP, &blockaction, 0);
	sigaction(SIGPIPE, &blockaction, 0);
	sigaction(SIGABRT, &blockaction, 0);
  
	res = pthread_create(&watchdog, 
			     NULL, 
			     watchdog_thread, 
			     NULL);
	if (res != 0) {
		fprintf(stderr, "sooperlooper: Warning! Unable to create watchdog thread.\n");
	}
}



int main(int argc, char** argv)
{
	OptionInfo option_info;
	CommandMap & cmdmap = CommandMap::instance();
	//setup_signals();
	
	
	parse_options (argc, argv, option_info);

	if (option_info.channels <= 0) {
		option_info.channels = 1;
	}
	if (option_info.loop_count < 0) {
		option_info.loop_count = 0;
	}
	if (option_info.loopsecs <= 0.0f) {
		option_info.loopsecs = DEFAULT_LOOP_TIME;
	}
	
	
	if (option_info.show_usage) {
		usage(argv[0]);
		exit(1);
	}

	
	if (!option_info.quiet || option_info.show_version) {
		cerr << "SooperLooper " << sooperlooper_version << endl << "Copyright 2004-2023 Jesse Chappell" << endl;
	}

	if (option_info.show_version) {
		exit(0);
	}


	//sl_init ();

	// create audio driver
	// todo: a factory
	AudioDriver * driver = new JackAudioDriver(option_info.jack_name, option_info.jack_server_name);
	
	
	engine = new Engine();

	engine->set_default_loop_secs (option_info.loopsecs);
	engine->set_default_channels (option_info.channels);
	
	if (!engine->initialize(driver, 2, option_info.oscport, option_info.pingurl)) {
		cerr << "cannot initialize sooperlooper\n";
		exit (1);
	}

	if (!option_info.quiet) {

		cerr << "OSC server URI (network) is: " << engine->get_osc_url() << endl;
		//cerr << "OSC server URI (local unix socket) is: " << engine->get_osc_url(false) << endl;
	}
	
	if (option_info.loadsession.empty()) {
		for (int i=0; i < option_info.loop_count; ++i)
		{
			engine->add_loop ((unsigned int) option_info.channels, option_info.loopsecs, option_info.discrete_io);
		}

		// set default sync source
		if (option_info.loop_count > 0) {
			engine->push_nonrt_event ( new GlobalSetEvent ("sync_source", 1.0f));
		}
	}
	else {
		// load session instead
		engine->load_session (option_info.loadsession);
	}

	
	if (!driver->activate()) {
		exit(1);
	}

	if (!option_info.pingurl.empty()) {
            // notify whoever asked as to which will be the spawner
            engine->get_control_osc()->send_pingack (true, true, option_info.pingurl);
	}

	MidiBridge * midibridge = 0;

#if WITH_ALSA
	// start up alsamidi bridge
	MIDI::PortRequest portreq (driver->get_name(), "sooperlooper", "duplex", "alsa/sequencer");
	midibridge = new MidiBridge(driver->get_name(), portreq);

#elif WITH_COREMIDI
	MIDI::PortRequest portreq (driver->get_name(), driver->get_name(),  "duplex", "coremidi");
	midibridge = new MidiBridge(driver->get_name(), portreq);
#endif

	if (midibridge && midibridge->is_ok()) {
		engine->set_midi_bridge(midibridge);
		if (!option_info.bindfile.empty()) {
			engine->load_midi_bindings(option_info.bindfile, false, cmdmap);
			//midibridge->bindings().load_bindings (option_info.bindfile);
		}
	}
	
	// go into engine's non-rt event loop
	// this returns when we quit or signal handler causes it to
	engine->mainloop();

	if (midibridge) {
		delete midibridge;
	}

	driver->deactivate();
	usleep(200000);
	driver->set_engine(0);
	
	delete engine;
	
	delete driver;
	
	//sl_fini ();
	
	return 0;
}
