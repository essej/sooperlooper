/*
 *  Copyright (C) 2004 Steve Harris, Uwe Koloska
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  $Id$
 */

#include <cstdio>
#include <cstdlib>

#include <iostream>

#include <unistd.h>
#include <pthread.h>

#include <curses.h>

#include <map>
#include <string>

#include <lo/lo.h>

using namespace std;


static lo_address addr;

static string our_url;
static lo_server osc_server = 0;
static pthread_t osc_thread = 0;

map<string, float> params_val_map;
map<int, string> state_map;
volatile bool updated = false;
volatile bool do_shutdown = false;

static int do_control_change(char cmd)
{
	

	float val = 0.0;
	int ret = 0;
	char * control = NULL;
	int row = 17;
	char fname[200];
	
	noraw();
	echo();
	timeout(-1);
	curs_set(2);
	
	switch (cmd)
	{
	case '1':
		control = "dry";
		mvprintw (row, 0, "Enter Dry Level (-90..0) dB : ");
		clrtoeol();
		refresh();
		ret = scanw ("%f", &val);
		break;
	case '2':
		control = "wet";
		mvprintw (row, 0, "Enter Wet Level (-90..0) dB : ");
		clrtoeol();
		refresh();
		ret = scanw ("%f", &val);
		break;
	case '3':
		control = "feedback";
		mvprintw (row, 0, "Enter Feedback (0..1) : ");
		clrtoeol();
		refresh();
		ret = scanw ("%f", &val);
		break;
	case '4':
		control = "rate";
		mvprintw (row, 0, "Enter Rate (-4..4) : ");
		clrtoeol();
		refresh();
		ret = scanw ("%f", &val);
		break;
	case '5':
		control = "quantize";
		mvprintw (row, 0, "Enter Quantize flag (0 or 1) : ");
		clrtoeol();
		refresh();
		ret = scanw ("%f", &val);
		break;
	case '6':
		control = "round";
		mvprintw (row, 0, "Enter Round flag (0 or 1) : ");
		clrtoeol();
		refresh();
		ret = scanw ("%f", &val);
		break;
	case '7':
		control = "rec_thresh";
		mvprintw (row, 0, "Enter Rec Thresh (0..1) : ");
		clrtoeol();
		refresh();
		ret = scanw ("%f", &val);
		break;

	case 'l':
		mvprintw (row, 0, "Filename to load : ");
		clrtoeol();
		refresh();
		ret = scanw ("%s", fname);
		
		break;
	case 'a':
		mvprintw (row, 0, "Filename to save loop to : ");
		clrtoeol();
		refresh();
		ret = scanw ("%s", fname);

		break;
		
	default:
		break;

	}

	if (ret > 0) {
		if (cmd == 'l') {
			lo_send(addr, "/sl/-1/load_loop", "sss", fname, our_url.c_str(), "/blah");
		}
		else if (cmd == 'a') {
			lo_send(addr, "/sl/-1/save_loop", "sssss", fname, "float", "little", our_url.c_str(), "/blah");
		}
		else {
			lo_send(addr, "/sl/-1/set", "sf", control, val);
		}
	}
	
	mvprintw (row, 0, "");
	clrtoeol ();
	refresh();
	
	raw();
	noecho();
	curs_set(0);

	return 0;
}

static int post_event(char cmd)
{
	int ret = 1;
	
	switch (cmd)
	{
	case 'q':
		lo_send (addr, "/quit", NULL);
		ret = -1;
		break;
	case 'b':
		ret = -1;
		break;
	case 'r':
		lo_send(addr, "/sl/-1/down", "s", "record");
		break;
	case 'o':
		lo_send(addr, "/sl/-1/down", "s", "overdub");
		break;
	case 'x':
		lo_send(addr, "/sl/-1/down", "s", "multiply");
		break;
	case 'i':
		lo_send(addr, "/sl/-1/down", "s", "insert");
		break;
	case 'p':
		lo_send(addr, "/sl/-1/down", "s", "replace");
		break;
	case 'm':
		lo_send(addr, "/sl/-1/down", "s", "mute");
		break;
	case 'u':
		lo_send(addr, "/sl/-1/down", "s", "undo");
		break;
	case 'd':
		lo_send(addr, "/sl/-1/down", "s", "redo");
		break;
	case 'v':
		lo_send(addr, "/sl/-1/down", "s", "reverse");
		break;
		
	default:
		ret = 0;
		break;
	}

	return ret;
}

static int ctrl_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 void *data, void *user_data)
{
	// 1st arg is instance, 2nd ctrl string, 3nd is float value
	//int index = argv[0]->i;
	string ctrl(&argv[1]->s);
	float val  = argv[2]->f;

	params_val_map[ctrl] = val;

	updated = true;

	return 0;
}


static void setup_param_map()
{
	params_val_map["state"] = 0.0f;
	params_val_map["loop_len"] = 0.0f;
	params_val_map["loop_pos"] = 0.0f;
	params_val_map["cycle_len"] = 0.0f;
	params_val_map["free_time"] = 0.0f;
	params_val_map["total_time"] = 0.0f;


	state_map[0] = "Off";
	state_map[1] = "Threshold Start";
	state_map[2] = "RECORDING";
	state_map[3] = "Threshold Stop";
	state_map[4] = "PLAYING";
	state_map[5] = "OVERDUBBING";
	state_map[6] = "MULTIPLYING";
	state_map[7] = "INSERTING";
	state_map[8] = "REPLACING";
	state_map[9] = "TAP DELAY";
	state_map[10] = "MUTED";
	state_map[11] = "SCRATCHING";
	state_map[12] = "ONE SHOT";
	
}

static void request_values()
{
	lo_send(addr, "/sl/0/get", "sss", "state", our_url.c_str(), "/ctrl");
	lo_send(addr, "/sl/0/get", "sss", "loop_pos", our_url.c_str(), "/ctrl");
	lo_send(addr, "/sl/0/get", "sss", "loop_len", our_url.c_str(), "/ctrl");
	lo_send(addr, "/sl/0/get", "sss", "cycle_len", our_url.c_str(), "/ctrl");
}

static void update_values()
{
	if (updated) {
		updated = false;

		mvprintw(13, 0, "State: %10s\n", state_map[(int)params_val_map["state"]].c_str());

		printw ( "Pos: %7.1f   CycleLen: %7.1f   LoopLen: %7.1f\n",
			 params_val_map["loop_pos"], params_val_map["cycle_len"], params_val_map["loop_len"]);
		
	}
}

static void * osc_receiver(void * arg)
{
	while (!do_shutdown)
	{
		lo_server_recv (osc_server);
	}

	return 0;
}

static void cleanup()
{
	do_shutdown = true;
	
	// send an event to self
	lo_address addr = lo_address_new_from_url (our_url.c_str());
	lo_send(addr, "/ping", "");
	lo_address_free (addr);

	pthread_join (osc_thread, NULL);
	lo_server_free (osc_server);
	
	endwin();
}

int main(int argc, char *argv[])
{
    int done = 0;
    char ch;
    int ret;

    
    /* an address to send messages to. sometimes it is better to let the server
     * pick a port number for you by passing NULL as the last argument */
    addr = lo_address_new(NULL, "9951");

      /* send a message to /foo/bar with two float arguments, report any
       * errors */
/*       if (lo_send(t, "/sl/0/up", "ff", 0.12345678f, 23.0f) == -1) { */
/* 	printf("OSC error %d: %s\n", lo_address_errno(t), lo_address_errstr(t)); */
/*       } */


    if (argc > 1) {
	    post_event (argv[1][0]);
	    exit (0);
    }

    
    setup_param_map();
    
    /* start a new server on a free port to recieve param callbacks */
    osc_server = lo_server_new(NULL, NULL);

    our_url = lo_server_get_url (osc_server);
    
    /* add handler for control param callbacks, first arg ctrl string, 2nd arg value */
    lo_server_add_method(osc_server, "/ctrl", "isf", ctrl_handler, NULL);

    // start up our thread
    pthread_create (&osc_thread, NULL, osc_receiver, NULL);
    
    /* now do some basic curses stuff */

    initscr();

    atexit(cleanup);
    
    raw();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);
	
    printw("SooperLooper OSC test client\n");
    printw("  r - record    o - overdub  x - multiply\n");
    printw("  i - insert    p - replace  v - reverse    s - scratch\n");
    printw("  m - mute      u - undo     d - redo\n");
    printw("  l - load loop     a - save loop\n");
    printw("  q - shutdown sooperlooper and quit\n");
    printw("  b - just quit\n");
    printw("\n");
    printw("  1 - set dry level (0..1)       2 - set wet level (0..1)\n");
    printw("  3 - set feedback level (0..1)  4 - set rate (-4..4)\n");
    printw("  5 - set quantize (0 or 1)      6 - set round (0 or 1)\n");
    printw("  7 - set rec thresh (0..1) \n");
    
    refresh();
    
    /* status on line 5 */

    
    while (!done)
    {
	    timeout (100);
	    ch = getch();
	    ret = 0;
	    
	    if (ch == 0 || ch == ERR) {
		    /* timeout */
		    request_values();
		    update_values();

		    continue;
	    }

	    if ((ch >= '1' && ch <= '9') || ch == 'l' || ch == 'a') {

		    do_control_change (ch);
	    }
	    else {
		    ret = post_event (ch);
	    }
	    
	    if (ret < 0) {
		    done = 1;
	    }

	    request_values();
	    update_values();
    }
    
    
    endwin();
    
    return 0;
}

/* vi:set ts=8 sts=4 sw=4: */
