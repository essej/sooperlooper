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
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <algorithm>

#include <sys/poll.h>
#include <unistd.h>
#include <fcntl.h>

#include "control_osc.hpp"
#include "event_nonrt.hpp"
#include "engine.hpp"
#include "ringbuffer.hpp"
#include "midi_bind.hpp"
#include "command_map.hpp"
#include "version.h"

#include <lo/lo.h>
#include <sigc++/sigc++.h>
using namespace sigc;

using namespace SooperLooper;
using namespace std;

//#define DEBUG 1

static void error_callback(int num, const char *m, const char *path)
{
#ifdef DEBUG
	fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, m);
#endif
}



ControlOSC::ControlOSC(Engine * eng, unsigned int port)
	: _engine(eng), _port(port)
{
	char tmpstr[255];

	_ok = false;
	_shutdown = false;
	_osc_server = 0;
	_osc_unix_server = 0;
	_osc_thread = 0;
	_max_instance = 0;
	
	for (int j=0; j < 20; ++j) {
		snprintf(tmpstr, sizeof(tmpstr), "%d", _port);

		if ((_osc_server = lo_server_new (tmpstr, error_callback))) {
			break;
		}
#ifdef DEBUG		
		cerr << "can't get osc at port: " << _port << endl;
#endif
		_port++;
		continue;
	}

	/*** APPEARS sluggish for now
	     
	// attempt to create unix socket server too
	//snprintf(tmpstr, sizeof(tmpstr), "/tmp/sooperlooper_%d", getpid());
	snprintf(tmpstr, sizeof(tmpstr), "/tmp/sooperlooper_XXXXXX");
	int fd = mkstemp(tmpstr);
	if (fd >=0) {
		unlink(tmpstr);
		close(fd);
		_osc_unix_server = lo_server_new (tmpstr, error_callback);
		if (_osc_unix_server) {
			_osc_unix_socket_path = tmpstr;
		}
	}

	*/

	_engine->LoopAdded.connect(mem_fun (*this, &ControlOSC::on_loop_added));
	_engine->LoopRemoved.connect(mem_fun (*this, &ControlOSC::on_loop_removed));

	register_callbacks();

	// for all loops
	on_loop_added(-1);

	// for selected loop
	on_loop_added(-3);
	
	// lo_server_thread_add_method(_sthread, NULL, NULL, ControlOSC::_dummy_handler, this);

	_cmd_map = &CommandMap::instance();


	if (!init_osc_thread()) {
		return;
	}
	

	_ok = true;
}

ControlOSC::~ControlOSC()
{
	if (!_osc_unix_socket_path.empty()) {
		// unlink it
		unlink(_osc_unix_socket_path.c_str());
	}

	// stop server thread
	terminate_osc_thread();
}

void
ControlOSC::register_callbacks()
{
	lo_server srvs[2];
	lo_server serv;

	srvs[0] = _osc_server;
	srvs[1] = _osc_unix_server;
	
	for (size_t i=0; i < 2; ++i) {
		if (!srvs[i]) continue;
		serv = srvs[i];


		/* add method that will match the path /quit with no args */
		lo_server_add_method(serv, "/quit", "", ControlOSC::_quit_handler, this);

		// add ping handler:  s:returl s:retpath [useid]
		lo_server_add_method(serv, "/ping", "ss", ControlOSC::_ping_handler, this);
		lo_server_add_method(serv, "/ping", "ssi", ControlOSC::_ping_handler, this);

		// add loop add handler:  i:channels  i:bytes_per_channel
		lo_server_add_method(serv, "/loop_add", "if", ControlOSC::_loop_add_handler, this);
		lo_server_add_method(serv, "/loop_add", "ifi", ControlOSC::_loop_add_handler, this);

		// load session:  s:filename  s:returl  s:retpath
		lo_server_add_method(serv, "/load_session", "sss", ControlOSC::_load_session_handler, this);

		// save session:  s:filename  s:returl  s:retpath (i:write_audio)
		lo_server_add_method(serv, "/save_session", "sss", ControlOSC::_save_session_handler, this);
		lo_server_add_method(serv, "/save_session", "sssi", ControlOSC::_save_session_handler, this);
		
		// add loop del handler:  i:index 
		lo_server_add_method(serv, "/loop_del", "i", ControlOSC::_loop_del_handler, this);

		// un/register config handler:  s:returl  s:retpath
		lo_server_add_method(serv, "/register", "ss", ControlOSC::_register_config_handler, this);
		lo_server_add_method(serv, "/unregister", "ss", ControlOSC::_unregister_config_handler, this);

		lo_server_add_method(serv, "/set", "sf", ControlOSC::_global_set_handler, this);
		lo_server_add_method(serv, "/get", "sss", ControlOSC::_global_get_handler, this);

		// un/register_update args= s:ctrl s:returl s:retpath
		lo_server_add_method(serv, "/register_update", "sss", ControlOSC::_global_register_update_handler, this);
		lo_server_add_method(serv, "/unregister_update", "sss", ControlOSC::_global_unregister_update_handler, this);
		lo_server_add_method(serv, "/register_auto_update", "siss", ControlOSC::_global_register_auto_update_handler, this);
		lo_server_add_method(serv, "/unregister_auto_update", "sss", ControlOSC::_global_unregister_auto_update_handler, this);


		// certain RT global ctrls
		lo_server_add_method(serv, "/sl/-2/set", "sf", ControlOSC::_set_handler, new CommandInfo(this, -2, Event::type_global_control_change));

		// get all midi bindings:  s:returl s:retpath
		lo_server_add_method(serv, "/get_all_midi_bindings", "ss", ControlOSC::_midi_binding_handler,
				     new MidiBindCommand(this, MidiBindCommand::GetAllBinding));

		// remove a specific midi binding:  s:binding_serialization s:options
		lo_server_add_method(serv, "/remove_midi_binding", "ss", ControlOSC::_midi_binding_handler,
				     new MidiBindCommand(this, MidiBindCommand::RemoveBinding));

		// add a specific midi binding:  s:binding_serialization s:options
		lo_server_add_method(serv, "/add_midi_binding", "ss", ControlOSC::_midi_binding_handler,
				     new MidiBindCommand(this, MidiBindCommand::AddBinding));

		// clear all bindings
		lo_server_add_method(serv, "/clear_midi_bindings", "", ControlOSC::_midi_binding_handler,
				     new MidiBindCommand(this, MidiBindCommand::ClearAllBindings));
		
		// load bindings from file:  s:binding_filename s:options
		lo_server_add_method(serv, "/load_midi_bindings", "ss", ControlOSC::_midi_binding_handler,
				     new MidiBindCommand(this, MidiBindCommand::LoadBindings));

		// save bindings to file:  s:binding_filename s:options
		lo_server_add_method(serv, "/save_midi_bindings", "ss", ControlOSC::_midi_binding_handler,
				     new MidiBindCommand(this, MidiBindCommand::SaveBindings));

		// learn midi binding:  s:binding_serialization s:options  s:returl s:retpath
		lo_server_add_method(serv, "/learn_midi_binding", "ssss", ControlOSC::_midi_binding_handler,
				     new MidiBindCommand(this, MidiBindCommand::LearnBinding));

		// return next midi event in a binding serialization:  s:returl  s:retpath
		lo_server_add_method(serv, "/get_next_midi_event", "ss", ControlOSC::_midi_binding_handler,
				     new MidiBindCommand(this, MidiBindCommand::GetNextMidi));

		// cancel learn or get next midi event in a binding serialization:  s:returl  s:retpath
		lo_server_add_method(serv, "/cancel_midi_learn", "ss", ControlOSC::_midi_binding_handler,
				     new MidiBindCommand(this, MidiBindCommand::CancelLearn));

		// cancel learn or get next midi event in a binding serialization:  s:returl  s:retpath
		lo_server_add_method(serv, "/cancel_get_next_midi", "ss", ControlOSC::_midi_binding_handler,
				     new MidiBindCommand(this, MidiBindCommand::CancelGetNext));
		
		
		// MIDI clock
		lo_server_add_method(serv, "/sl/midi_start", NULL, ControlOSC::_midi_start_handler, this);
		lo_server_add_method(serv, "/sl/midi_stop", NULL, ControlOSC::_midi_stop_handler, this);
		lo_server_add_method(serv, "/sl/midi_tick", NULL, ControlOSC::_midi_tick_handler, this);
	
	}
}

bool
ControlOSC::init_osc_thread ()
{
	// create new thread to run server
	if (pipe (_request_pipe)) {
		cerr << "Cannot create osc request signal pipe" <<  strerror (errno) << endl;
		return false;
	}

	if (fcntl (_request_pipe[0], F_SETFL, O_NONBLOCK)) {
		cerr << "osc: cannot set O_NONBLOCK on signal read pipe " << strerror (errno) << endl;
		return false;
	}

	if (fcntl (_request_pipe[1], F_SETFL, O_NONBLOCK)) {
		cerr << "osc: cannot set O_NONBLOCK on signal write pipe " << strerror (errno) << endl;
		return false;
	}
	
	pthread_create (&_osc_thread, NULL, &ControlOSC::_osc_receiver, this);
	if (!_osc_thread) {
		return false;
	}

	//pthread_detach (_osc_thread);
	return true;
}

void
ControlOSC::terminate_osc_thread ()
{
	void* status;

	_shutdown = true;
	
	poke_osc_thread ();

	pthread_join (_osc_thread, &status);
}

void
ControlOSC::poke_osc_thread ()
{
	char c;

	if (write (_request_pipe[1], &c, 1) != 1) {
		cerr << "cannot send signal to osc thread! " <<  strerror (errno) << endl;
	}
}


void
ControlOSC::on_loop_added (int instance, bool sendupdate)
{
	// will be called from main event loop

	char tmpstr[255];
#ifdef DEBUG
	cerr << "loop added: " << instance << endl;
#endif
	lo_server srvs[2];
	lo_server serv;

	if (instance >= 0 && instance < _max_instance) {
		// already added this method
		if (sendupdate) {
			send_all_config();
		}
		return;
	}
	
	srvs[0] = _osc_server;
	srvs[1] = _osc_unix_server;
	
	for (size_t i=0; i < 2; ++i) {
		if (!srvs[i]) continue;
		serv = srvs[i];
		
		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/down", instance);
		lo_server_add_method(serv, tmpstr, "s", ControlOSC::_updown_handler, new CommandInfo(this, instance, Event::type_cmd_down));
	
		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/up", instance);
		lo_server_add_method(serv, tmpstr, "s", ControlOSC::_updown_handler, new CommandInfo(this, instance, Event::type_cmd_up));

		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/hit", instance);
		lo_server_add_method(serv, tmpstr, "s", ControlOSC::_updown_handler, new CommandInfo(this, instance, Event::type_cmd_hit));
		
		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/upforce", instance);
		lo_server_add_method(serv, tmpstr, "s", ControlOSC::_updown_handler, new CommandInfo(this, instance, Event::type_cmd_upforce));
		
		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/set", instance);
		lo_server_add_method(serv, tmpstr, "sf", ControlOSC::_set_handler, new CommandInfo(this, instance, Event::type_control_change));
	
		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/get", instance);
		lo_server_add_method(serv, tmpstr, "sss", ControlOSC::_get_handler, new CommandInfo(this, instance, Event::type_control_request));

		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/set_prop", instance);
		lo_server_add_method(serv, tmpstr, "ss", ControlOSC::_set_prop_handler, new CommandInfo(this, instance, Event::type_control_change));

		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/get_prop", instance);
		lo_server_add_method(serv, tmpstr, "sss", ControlOSC::_get_prop_handler, new CommandInfo(this, instance, Event::type_control_request));

		// load loop:  s:filename  s:returl  s:retpath
		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/load_loop", instance);
		lo_server_add_method(serv, tmpstr, "sss", ControlOSC::_loadloop_handler, new CommandInfo(this, instance, Event::type_control_request));

		// save loop:  s:filename  s:format s:endian s:returl  s:retpath
		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/save_loop", instance);
		lo_server_add_method(serv, tmpstr, "sssss", ControlOSC::_saveloop_handler, new CommandInfo(this, instance, Event::type_control_request));
	
		// register_update args= s:ctrl s:returl s:retpath
		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/register_update", instance);
		lo_server_add_method(serv, tmpstr, "sss", ControlOSC::_register_update_handler, new CommandInfo(this, instance, Event::type_control_request));

		// unregister_update args= s:ctrl s:returl s:retpath
		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/unregister_update", instance);
		lo_server_add_method(serv, tmpstr, "sss", ControlOSC::_unregister_update_handler, new CommandInfo(this, instance, Event::type_control_request));

		// register_audo_update args= s:ctrl i:millisec s:returl s:retpath
		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/register_auto_update", instance);
		lo_server_add_method(serv, tmpstr, "siss", ControlOSC::_register_auto_update_handler, new CommandInfo(this, instance, Event::type_control_request));

		// unregister_auto_update args= s:ctrl s:returl s:retpath
		snprintf(tmpstr, sizeof(tmpstr), "/sl/%d/unregister_auto_update", instance);
		lo_server_add_method(serv, tmpstr, "sss", ControlOSC::_unregister_auto_update_handler, new CommandInfo(this, instance, Event::type_control_request));

	}

	_max_instance = instance + 1;
	
	if (sendupdate) {
		send_all_config();
	}
}

void
ControlOSC::on_loop_removed ()
{
	// will be called from main event loop
	send_all_config();
}


std::string
ControlOSC::get_server_url()
{
	string url;
	char * urlstr;

	if (_osc_server) {
		urlstr = lo_server_get_url (_osc_server);
		url = urlstr;
		free (urlstr);
	}
	
	return url;
}

std::string
ControlOSC::get_unix_server_url()
{
	string url;
	char * urlstr;

	if (_osc_unix_server) {
		urlstr = lo_server_get_url (_osc_unix_server);
		url = urlstr;
		free (urlstr);
	}
	
	return url;
}


/* server thread */

void *
ControlOSC::_osc_receiver(void * arg)
{
	static_cast<ControlOSC*> (arg)->osc_receiver();
	return 0;
}

void
ControlOSC::osc_receiver()
{
	struct pollfd pfd[3];
	int fds[3];
	lo_server srvs[3];
	int nfds = 0;
	int timeout = -1;
	int ret;
	
	fds[0] = _request_pipe[0];
	nfds++;
	
	if (_osc_server && lo_server_get_socket_fd(_osc_server) >= 0) {
		fds[nfds] = lo_server_get_socket_fd(_osc_server);
		srvs[nfds] = _osc_server;
		nfds++;
	}

	if (_osc_unix_server && lo_server_get_socket_fd(_osc_unix_server) >= 0) {
		fds[nfds] = lo_server_get_socket_fd(_osc_unix_server);
		srvs[nfds] = _osc_unix_server;
		nfds++;
	}
	
	
	while (!_shutdown) {

		for (int i=0; i < nfds; ++i) {
			pfd[i].fd = fds[i];
			pfd[i].events = POLLIN|POLLPRI|POLLHUP|POLLERR;
			pfd[i].revents = 0;
		}
		
	again:
		//cerr << "poll on " << nfds << " for " << timeout << endl;
		if ((ret = poll (pfd, nfds, timeout)) < 0) {
			if (errno == EINTR) {
				/* gdb at work, perhaps */
				cerr << "EINTR hit " << endl;
				goto again;
			}
			
			cerr << "OSC thread poll failed: " <<  strerror (errno) << endl;
			
			break;
		}

		//cerr << "poll returned " << ret << "  pfd[0].revents = " << pfd[0].revents << "  pfd[1].revents = " << pfd[1].revents << endl;
		
		if (_shutdown) {
			break;
		}
		
		if ((pfd[0].revents & ~POLLIN)) {
			cerr << "OSC: error polling extra port" << endl;
			break;
		}
		
		for (int i=1; i < nfds; ++i) {
			if (pfd[i].revents & POLLIN)
			{
				// this invokes callbacks
				//cerr << "invoking recv on " << pfd[i].fd << endl;
				lo_server_recv(srvs[i]);
			}
		}

	}

	//cerr << "SL engine shutdown" << endl;
	
	if (_osc_server) {
		int fd = lo_server_get_socket_fd(_osc_server);
		if (fd >=0) {
				// hack around
			close(fd);
		}
		lo_server_free (_osc_server);
		_osc_server = 0;
	}

	if (_osc_unix_server) {
		cerr << "freeing unix server" << endl;
		lo_server_free (_osc_unix_server);
		_osc_unix_server = 0;
	}
	
	close(_request_pipe[0]);
	close(_request_pipe[1]);
}



/* STATIC callbacks */


int ControlOSC::_dummy_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
#ifdef DEBUG
	cerr << "got path: " << path << endl;
#endif
	return 0;
}


int ControlOSC::_quit_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->quit_handler (path, types, argv, argc, msg);

}

int ControlOSC::_ping_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->ping_handler (path, types, argv, argc, msg);

}

int ControlOSC::_global_set_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->global_set_handler (path, types, argv, argc, msg);

}
int ControlOSC::_global_get_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->global_get_handler (path, types, argv, argc, msg);

}


int ControlOSC::_updown_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->updown_handler (path, types, argv, argc, msg, cp);
}

int ControlOSC::_set_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->set_handler (path, types, argv, argc, msg, cp);
}

int ControlOSC::_get_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->get_handler (path, types, argv, argc, msg, cp);
}

int ControlOSC::_set_prop_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->set_prop_handler (path, types, argv, argc, msg, cp);
}

int ControlOSC::_get_prop_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->get_prop_handler (path, types, argv, argc, msg, cp);
}

int ControlOSC::_register_update_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->register_update_handler (path, types, argv, argc, msg, cp);
}

int ControlOSC::_unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->unregister_update_handler (path, types, argv, argc, msg, cp);
}

int ControlOSC::_register_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->register_auto_update_handler (path, types, argv, argc, msg, cp);
}

int ControlOSC::_unregister_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->unregister_auto_update_handler (path, types, argv, argc, msg, cp);
}

int ControlOSC::_loop_add_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->loop_add_handler (path, types, argv, argc, msg);
}

int ControlOSC::_loop_del_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->loop_del_handler (path, types, argv, argc, msg);
}

int ControlOSC::_load_session_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->load_session_handler (path, types, argv, argc, msg);
}
int ControlOSC::_save_session_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->save_session_handler (path, types, argv, argc, msg);
}

int ControlOSC::_register_config_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->register_config_handler (path, types, argv, argc, msg);
}

int ControlOSC::_unregister_config_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->unregister_config_handler (path, types, argv, argc, msg);
}

int ControlOSC::_loadloop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->loadloop_handler (path, types, argv, argc, msg, cp);
}

int ControlOSC::_saveloop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
	CommandInfo * cp = static_cast<CommandInfo*> (user_data);
	return cp->osc->saveloop_handler (path, types, argv, argc, msg, cp);
}

int ControlOSC::_global_register_update_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->global_register_update_handler (path, types, argv, argc, msg);
}

int ControlOSC::_global_unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->global_unregister_update_handler (path, types, argv, argc, msg);
}

int ControlOSC::_global_register_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->global_register_auto_update_handler (path, types, argv, argc, msg);
}

int ControlOSC::_global_unregister_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->global_unregister_auto_update_handler (path, types, argv, argc, msg);
}


int ControlOSC::_midi_start_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->midi_start_handler (path, types, argv, argc, msg);
}

int ControlOSC::_midi_stop_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->midi_stop_handler (path, types, argv, argc, msg);
}

int ControlOSC::_midi_tick_handler(const char *path, const char *types, lo_arg **argv, int argc,
			 lo_message msg, void *user_data)
{
	ControlOSC * osc = static_cast<ControlOSC*> (user_data);
	return osc->midi_tick_handler (path, types, argv, argc, msg);
}

int ControlOSC::_midi_binding_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
	MidiBindCommand * cp = static_cast<MidiBindCommand*> (user_data);
	return cp->osc->midi_binding_handler (path, types, argv, argc, msg, cp);
}


/* real callbacks */

int ControlOSC::quit_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	_engine->quit();
	return 0;
}


int ControlOSC::ping_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	string returl (&argv[0]->s);
	string retpath (&argv[1]->s);

	validate_returl(returl);

	bool useid = false;
	if (argc > 2) {
		useid = true;
	}

	//lo_address srcaddr = lo_message_get_source (msg);
	//const char * sport = lo_address_get_port(srcaddr);
	//int srcport = atoi(sport);
	//cerr << "got ping from " <<  srcport << endl;

	_engine->push_nonrt_event ( new PingEvent (returl, retpath, useid));
	
	return 0;
}


int ControlOSC::global_get_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg)
{
	// s: param  s: returl  s: retpath
	string param (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	validate_returl(returl);

	_engine->push_nonrt_event ( new GlobalGetEvent (param, returl, retpath));
	return 0;
}

int ControlOSC::global_set_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg)
{
	// s: param  f: val
	string param(&argv[0]->s);
	float val  = argv[1]->f;
	lo_address srcaddr = lo_message_get_source (msg);
	const char * sport = lo_address_get_port(srcaddr);
	int srcport = atoi(sport);

	_engine->push_nonrt_event ( new GlobalSetEvent (param, val));

	// send out updates to registered in main event loop
	_engine->push_nonrt_event ( new ConfigUpdateEvent (ConfigUpdateEvent::Send, -2, _cmd_map->to_control_t(param), "", "", val, srcport));
	
	return 0;
}

int
ControlOSC::global_register_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	// un/register_update args= s:ctrl s:returl s:retpath
	string ctrl (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	validate_returl(returl);

	// push this onto a queue for the main event loop to process
	// -2 means global
	_engine->push_nonrt_event ( new ConfigUpdateEvent (ConfigUpdateEvent::Register, -2, _cmd_map->to_control_t(ctrl), returl, retpath));

	return 0;
}

int
ControlOSC::global_unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	// 1st is return URL string 2nd is retpath
	string ctrl (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	validate_returl(returl);

	// push this onto a queue for the main event loop to process
	// -2 means global
	_engine->push_nonrt_event ( new ConfigUpdateEvent (ConfigUpdateEvent::Unregister, -2, _cmd_map->to_control_t(ctrl), returl, retpath));

	return 0;
}

int
ControlOSC::global_register_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	// first arg is control string, 2nd is every int millisecs, 3rd is return URL string 4th is retpath
	string ctrl (&argv[0]->s);
	short int millisec  = argv[1]->i;
	string returl (&argv[2]->s);
	string retpath (&argv[3]->s);

	validate_returl(returl);

	//round down to the nearest step size
	millisec -= millisec % AUTO_UPDATE_STEP;

	if (millisec < AUTO_UPDATE_MIN)
		millisec = AUTO_UPDATE_MIN;
	else if (millisec > AUTO_UPDATE_MAX)
		millisec = AUTO_UPDATE_MAX;


	// push this onto a queue for the main event loop to process
	// -2 means global
	_engine->push_nonrt_event ( new ConfigUpdateEvent (ConfigUpdateEvent::RegisterAuto, -2, _cmd_map->to_control_t(ctrl), returl, retpath,0.0,-1, millisec));

	return 0;
}

int
ControlOSC::global_unregister_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	// 1st is return URL string 2nd is retpath
	string ctrl (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	validate_returl(returl);

	// push this onto a queue for the main event loop to process
	// -2 means global
	_engine->push_nonrt_event ( new ConfigUpdateEvent (ConfigUpdateEvent::UnregisterAuto, -2, _cmd_map->to_control_t(ctrl), returl, retpath));

	return 0;
}





int
ControlOSC::midi_binding_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, MidiBindCommand * info)
{
	if (info->command == MidiBindCommand::GetAllBinding)
	{
		// get all midi bindings:  s:returl s:retpath
		string returl (&argv[0]->s);
		string retpath (&argv[1]->s);
	
		validate_returl(returl);

		_engine->push_nonrt_event ( new MidiBindingEvent (MidiBindingEvent::GetAll, "", "", returl, retpath));
	}
	else if (info->command == MidiBindCommand::RemoveBinding)
	{
		// remove a specific midi binding:  s:binding_serialization s:options
		string bindstr (&argv[0]->s);
		string options (&argv[1]->s);
	
		_engine->push_nonrt_event ( new MidiBindingEvent (MidiBindingEvent::Remove, bindstr, options));
	}
	else if (info->command == MidiBindCommand::AddBinding)
	{
		// add a specific midi binding:  s:binding_serialization s:options
		string bindstr (&argv[0]->s);
		string options (&argv[1]->s);
	
		_engine->push_nonrt_event ( new MidiBindingEvent (MidiBindingEvent::Add, bindstr, options));
	}
	else if (info->command == MidiBindCommand::LearnBinding)
	{
		// add a specific midi binding:  s:binding_serialization s:options
		string bindstr (&argv[0]->s);
		string options (&argv[1]->s);
		string returl (&argv[2]->s);
		string retpath (&argv[3]->s);

		validate_returl(returl);

		_engine->push_nonrt_event ( new MidiBindingEvent (MidiBindingEvent::Learn, bindstr, options, returl, retpath));
	}
	else if (info->command == MidiBindCommand::GetNextMidi)
	{
		// add a specific midi binding:  s:returl s:retpath
		string returl (&argv[0]->s);
		string retpath (&argv[1]->s);

		validate_returl(returl);

		_engine->push_nonrt_event ( new MidiBindingEvent (MidiBindingEvent::GetNextMidi, "", "", returl, retpath));
	}
	else if (info->command == MidiBindCommand::CancelLearn)
	{
		// add a specific midi binding:  s:returl s:retpath
		string returl (&argv[0]->s);
		string retpath (&argv[1]->s);

		validate_returl(returl);

		_engine->push_nonrt_event ( new MidiBindingEvent (MidiBindingEvent::CancelLearn, "", "", returl, retpath));
	}
	else if (info->command == MidiBindCommand::CancelGetNext)
	{
		// add a specific midi binding:  s:returl s:retpath
		string returl (&argv[0]->s);
		string retpath (&argv[1]->s);

		validate_returl(returl);

		_engine->push_nonrt_event ( new MidiBindingEvent (MidiBindingEvent::CancelGetNext, "", "", returl, retpath));
	}
	else if (info->command == MidiBindCommand::LoadBindings)
	{
		// load from file:  s:binding_filename s:options
		string bindstr (&argv[0]->s);
		string options (&argv[1]->s);
	
		_engine->push_nonrt_event ( new MidiBindingEvent (MidiBindingEvent::Load, bindstr, options));
	}
	else if (info->command == MidiBindCommand::SaveBindings)
	{
		// save to file:  s:binding_filename s:options
		string bindstr (&argv[0]->s);
		string options (&argv[1]->s);

		_engine->push_nonrt_event ( new MidiBindingEvent (MidiBindingEvent::Save, bindstr, options));
	}
	else if (info->command == MidiBindCommand::ClearAllBindings)
	{
		// load from file:  s:binding_filename s:options
	
		_engine->push_nonrt_event ( new MidiBindingEvent (MidiBindingEvent::Clear, "", ""));
	}

	return 0;
}


int
ControlOSC::midi_start_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	_engine->push_sync_event (Event::MidiStart);
	return 0;
}


int
ControlOSC::midi_stop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	_engine->push_sync_event (Event::MidiStop);
	return 0;
}


int
ControlOSC::midi_tick_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	_engine->push_sync_event (Event::MidiTick);
	return 0;
}


int ControlOSC::updown_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, CommandInfo *info)
{
	// first arg is a string
	
	string cmd(&argv[0]->s);

	_engine->push_command_event(info->type, _cmd_map->to_command_t(cmd), info->instance);
	
	return 0;
}


int ControlOSC::set_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, CommandInfo *info)
{

	// first arg is a control string, 2nd is float val

	string ctrl(&argv[0]->s);
	float val  = argv[1]->f;
	lo_address srcaddr = lo_message_get_source (msg);
	const char * sport = lo_address_get_port(srcaddr);
	int srcport = atoi(sport);
	//cerr << "source is " << srcport << endl;

	_engine->push_control_event(info->type, _cmd_map->to_control_t(ctrl), val, info->instance, srcport);
	
	return 0;

}

int ControlOSC::get_prop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, CommandInfo *info)
{

	// first arg is control string, 2nd is return URL string 3rd is retpath
	string ctrl (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	// cerr << "get prop " << ctrl << endl;

	validate_returl(returl);

	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new GetPropertyEvent (info->instance, ctrl, returl, retpath));

	return 0;
}

int ControlOSC::set_prop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, CommandInfo *info)
{
	// first arg is a control string, 2nd is string val

	string ctrl(&argv[0]->s);
	string val (&argv[1]->s);
	lo_address srcaddr = lo_message_get_source (msg);
	const char * sport = lo_address_get_port(srcaddr);
	int srcport = atoi(sport);
	// cerr << "set_prop " << ctrl << "  to val: " << val << endl;

	_engine->push_nonrt_event ( new SetPropertyEvent (info->instance, ctrl, val));

	return 0;
}

int ControlOSC::loop_add_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	// 1st is an int #channels
	// 2nd is a float #bytes per channel (if 0, use engine default) 
	
	int channels = argv[0]->i;
	float secs = argv[1]->f;
	int discrete = 1;

	if (argc > 2) {
		discrete = argv[2]->i;
	}

	_engine->push_nonrt_event ( new ConfigLoopEvent (ConfigLoopEvent::Add, channels, secs, 0, discrete));
	
	return 0;
}

int ControlOSC::loop_del_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	// 1st is index of loop to delete
	
	int index = argv[0]->i;

	_engine->push_nonrt_event ( new ConfigLoopEvent (ConfigLoopEvent::Remove, 0, 0.0f, index));

	return 0;
}


int ControlOSC::load_session_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{

	// first arg is fname, 2nd is return URL string 3rd is retpath
	string fname (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	validate_returl(returl);

	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new SessionEvent (SessionEvent::Load, fname, returl, retpath));
	
	return 0;
}

int ControlOSC::save_session_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{

	// first arg is fname, 2nd is return URL string 3rd is retpath
	string fname (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);
	bool audio = false;

	validate_returl(returl);

	if (argc > 3) {
		audio = (bool) argv[3]->i;
	}

	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new SessionEvent (SessionEvent::Save, fname, returl, retpath, audio));
	
	return 0;
}


int ControlOSC::loadloop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, CommandInfo *info)
{

	// first arg is fname, 2nd is return URL string 3rd is retpath
	string fname (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	validate_returl(returl);

	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new LoopFileEvent (LoopFileEvent::Load, info->instance, fname, returl, retpath));
	
	return 0;
}

int ControlOSC::saveloop_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, CommandInfo *info)
{
	// save loop:  s:filename  s:format s:endian s:returl  s:retpath
	string fname (&argv[0]->s);
	string format (&argv[1]->s);
	string endian (&argv[2]->s);
	string returl (&argv[3]->s);
	string retpath (&argv[4]->s);

	validate_returl(returl);

	LoopFileEvent::FileFormat fmt = LoopFileEvent::FormatFloat;
	LoopFileEvent::Endian end = LoopFileEvent::LittleEndian;

	if (format == "float") {
		fmt = LoopFileEvent::FormatFloat;
	}
	else if (format == "pcm16") {
		fmt = LoopFileEvent::FormatPCM16;
	}
	else if (format == "pcm24") {
		fmt = LoopFileEvent::FormatPCM24;
	}
	else if (format == "pcm32") {
		fmt = LoopFileEvent::FormatPCM32;
	}

	if (endian == "big") {
		end = LoopFileEvent::BigEndian;
	}
	
	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new LoopFileEvent (LoopFileEvent::Save, info->instance, fname, returl, retpath, fmt, end));
	
	return 0;
}


lo_address
ControlOSC::find_or_cache_addr(string returl)
{
	lo_address addr = 0;

	if (returl.empty()) return 0;
	
	if (_retaddr_map.find(returl) == _retaddr_map.end()) {
		addr = lo_address_new_from_url (returl.c_str());
		if (lo_address_errno (addr) < 0) {
			fprintf(stderr, "addr error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
		}
		_retaddr_map[returl] = addr;
	}
	else {
		addr = _retaddr_map[returl];
	}
	
	return addr;
}

int ControlOSC::get_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, CommandInfo *info)
{
	// cerr << "get " << path << endl;

	// first arg is control string, 2nd is return URL string 3rd is retpath
	string ctrl (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	validate_returl(returl);

	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new GetParamEvent (info->instance, _cmd_map->to_control_t(ctrl), returl, retpath));
	
	return 0;
}

int ControlOSC::register_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, CommandInfo *info)
{
	// first arg is control string, 2nd is return URL string 3rd is retpath
	string ctrl (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	validate_returl(returl);

	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new ConfigUpdateEvent (ConfigUpdateEvent::Register, info->instance, _cmd_map->to_control_t(ctrl), returl, retpath));
        //cerr << "register update recvd for " << (int)info->instance << "  ctrl: " << ctrl << endl;
	
	return 0;
}

int ControlOSC::unregister_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, CommandInfo *info)
{

	// first arg is control string, 2nd is return URL string 3rd is retpath
	string ctrl (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	validate_returl(returl);

	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new ConfigUpdateEvent (ConfigUpdateEvent::Unregister, info->instance, _cmd_map->to_control_t(ctrl), returl, retpath));
	
	return 0;
}

int ControlOSC::register_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, CommandInfo *info)
{
	// first arg is control string, 2nd is every int millisecs, 3rd is return URL string 4th is retpath
	string ctrl (&argv[0]->s);
	short int millisec  = argv[1]->i;
	string returl (&argv[2]->s);
	string retpath (&argv[3]->s);

	validate_returl(returl);

	//round down to the nearest step size
	millisec -= millisec % AUTO_UPDATE_STEP;

	if (millisec < AUTO_UPDATE_MIN)
		millisec = AUTO_UPDATE_MIN;
	else if (millisec > AUTO_UPDATE_MAX)
		millisec = AUTO_UPDATE_MAX;

	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new ConfigUpdateEvent (ConfigUpdateEvent::RegisterAuto, info->instance, _cmd_map->to_control_t(ctrl), returl, retpath,0.0,-1, millisec));
	// cerr << "register autoupdate recvd for " << (int)info->instance << "  ctrl: " << ctrl << endl;
	return 0;
}

int ControlOSC::unregister_auto_update_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, CommandInfo *info)
{

	// first arg is control string, 2nd is return URL string 3rd is retpath
	string ctrl (&argv[0]->s);
	string returl (&argv[1]->s);
	string retpath (&argv[2]->s);

	validate_returl(returl);

	// push this onto a queue for the main event loop to process
	_engine->push_nonrt_event ( new ConfigUpdateEvent (ConfigUpdateEvent::UnregisterAuto, info->instance, _cmd_map->to_control_t(ctrl), returl, retpath));
	
	return 0;
}


int
ControlOSC::register_config_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	// 1st is return URL string 2nd is retpath
	string returl (&argv[0]->s);
	string retpath (&argv[1]->s);

	validate_returl(returl);

	_engine->push_nonrt_event ( new RegisterConfigEvent (RegisterConfigEvent::Register, returl, retpath));
	
	return 0;
}

int
ControlOSC::unregister_config_handler(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg)
{
	// 1st is return URL string 2nd is retpath
	string returl (&argv[0]->s);
	string retpath (&argv[1]->s);

	validate_returl(returl);

	_engine->push_nonrt_event ( new RegisterConfigEvent (RegisterConfigEvent::Unregister, returl, retpath));
	return 0;
}


void
ControlOSC::finish_get_event (GetParamEvent & event)
{
	// called from the main event loop (not osc thread)
	string ctrl (_cmd_map->to_control_str(event.control));
	string returl (event.ret_url);
	string retpath (event.ret_path);
	lo_address addr;

	addr = find_or_cache_addr (returl);
	if (!addr) {
		return;
	}
	
//	 cerr << "sending to " << returl << "  path: " << retpath << "  ctrl: " << ctrl << "  val: " <<  event.ret_value << endl;

	if (lo_send(addr, retpath.c_str(), "isf", event.instance, ctrl.c_str(), event.ret_value) == -1) {
		fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
	}
	
}

void 
ControlOSC::finish_get_property_event (GetPropertyEvent & event)
{
	// called from the main event loop (not osc thread)
	string ctrl (event.property);
	string returl (event.ret_url);
	string retpath (event.ret_path);
	lo_address addr;

	addr = find_or_cache_addr (returl);
	if (!addr) {
		return;
	}

	// cerr << "sending prop to " << returl << "  path: " << retpath << "  ctrl: " << ctrl << "  val: " <<  event.ret_value << endl;

	if (lo_send(addr, retpath.c_str(), "iss", event.instance, ctrl.c_str(), event.ret_value.c_str()) == -1) {
		fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
	}
}


void
ControlOSC::finish_global_get_event (GlobalGetEvent & event)
{
	// called from the main event loop (not osc thread)
	string param (event.param);
	string returl (event.ret_url);
	string retpath (event.ret_path);
	lo_address addr;

	addr = find_or_cache_addr (returl);
	if (!addr) {
		return;
	}
	
	// cerr << "sending to " << returl << "  path: " << retpath << "  ctrl: " << param << "  val: " <<  event.ret_value << endl;

	if (lo_send(addr, retpath.c_str(), "isf", -2, param.c_str(), event.ret_value) == -1) {
		fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
	}
	
}

void
ControlOSC::finish_update_event (ConfigUpdateEvent & event)
{
	// called from the main event loop (not osc thread)

	lo_address addr;
	string retpath = event.ret_path;
	string returl  = event.ret_url;
	string ctrl    = _cmd_map->to_control_str (event.control);
	int source  = event.source;

	if (event.type == ConfigUpdateEvent::Send)
	{
		if (event.instance == -1) {
			for (unsigned int i = 0; i < _engine->loop_count(); ++i) {
				send_registered_updates (ctrl, event.value, (int) i, source);
			}
		} else {
			send_registered_updates (ctrl, event.value, event.instance, source);
		}

	}
	else if (event.type == ConfigUpdateEvent::Register ||
		 event.type == ConfigUpdateEvent::RegisterAuto)
	{
		if ((addr = find_or_cache_addr (returl)) == 0) {
			return;
		}

		// add this to register_ctrl map
		InstancePair ipair(event.instance, ctrl);
		
		if (event.type == ConfigUpdateEvent::Register) {
			ControlRegistrationMap::iterator iter;
			ControlRegistrationMap * regmap;
			regmap = &_registration_map;
			iter = regmap->find (ipair);
			if (iter == regmap->end()) {
				(*regmap)[ipair] = UrlList();
				iter = regmap->find (ipair);
			}

			UrlList & ulist = (*iter).second;
			UrlPair upair(addr, retpath);

			if (find(ulist.begin(), ulist.end(), upair) == ulist.end()) {
#ifdef DEBUG
				cerr << "registered " << (int)event.instance << "  ctrl: " << ctrl << "  " << returl << endl;
#endif
				ulist.push_back (upair);
			}
		}
		else {
			ControlRegistrationMapAuto::iterator iter;
			InstancePair ipair(event.instance, ctrl);
			ControlRegistrationMapAuto * regmap;
			regmap = &_auto_registration_map;
			iter = regmap->find (ipair);
			if (iter == regmap->end()) {
				(*regmap)[ipair] = UrlListAuto();
				iter = regmap->find (ipair);
			}

			UrlListAuto & ulist_auto = (*iter).second;
			UrlListAuto::iterator list_it;
			UrlPair upair(addr, retpath);
			UrlPairAuto upair_auto = {upair,event.update_time_ms};
			UrlPairAuto upair_dud = {upair,0};

			for(list_it = ulist_auto.begin(); list_it != ulist_auto.end(); ++list_it) {
				if ((*list_it).upair == upair) {
					if((*list_it).timeout == event.update_time_ms) {
						break;
					} else {
#ifdef DEBUG
						cerr << "updated " << (int)event.instance << "  ctrl: " << ctrl << "  " << returl << "  timeout: " << event.update_time_ms << endl;
#endif
						(*list_it).timeout = event.update_time_ms;
						break;
					}
				} 
			}
			if (list_it == ulist_auto.end()) {
#ifdef DEBUG
				cerr << "registered " << (int)event.instance << "  ctrl: " << ctrl << "  " << returl << "  timeout: " << event.update_time_ms << endl;
#endif
				ulist_auto.push_back(upair_auto);
			}
		}

		
	}
	else if (event.type == ConfigUpdateEvent::Unregister ||
		 event.type == ConfigUpdateEvent::UnregisterAuto)
	{

		if ((addr = find_or_cache_addr (returl)) == 0) {
			return;
		}
		
		// remove this from register_ctrl map
		InstancePair ipair(event.instance, ctrl);

		if (event.type == ConfigUpdateEvent::Unregister) {
			ControlRegistrationMap * regmap;
			ControlRegistrationMap::iterator iter;
			regmap = &_registration_map;
			iter = regmap->find (ipair);

			if (iter != regmap->end()) {
				UrlList & ulist = (*iter).second;
				UrlPair upair(addr, retpath);
				UrlList::iterator uiter = find(ulist.begin(), ulist.end(), upair);

				if (uiter != ulist.end()) {
#ifdef DEBUG
					cerr << "unregistered " << ctrl << "  " << returl << endl;
#endif
					ulist.erase (uiter);
				}
			}
		}
		else { //UnRegisterAuto 
			ControlRegistrationMapAuto * regmap;
			ControlRegistrationMapAuto::iterator iter;
			regmap = &_auto_registration_map;
			iter = regmap->find (ipair);

			if (iter != regmap->end()) {
				UrlListAuto & ulist_auto = (*iter).second;
				UrlPair upair(addr, retpath);
				UrlPairAuto upair_auto = {upair, 0};
				UrlListAuto::iterator list_it;
				for(list_it = ulist_auto.begin(); list_it != ulist_auto.end(); ++list_it) {
					if ((*list_it).upair == upair) 
					break;
				}
				if (list_it != ulist_auto.end()) {
#ifdef DEBUG
					cerr << "unregistered " << ctrl << "  " << returl << endl;
#endif
					ulist_auto.erase (list_it);
				}
			}
		}
	}
}

			
void ControlOSC::finish_loop_config_event (ConfigLoopEvent &event)
{
	if (event.type == ConfigLoopEvent::Remove) {
		// unregister everything for this instance
		ControlRegistrationMap::iterator citer = _registration_map.begin();
		ControlRegistrationMap::iterator tmp;
		
		while ( citer != _registration_map.end()) {
			if ((*citer).first.first == event.index) {
				tmp = citer;
				++tmp;
				_registration_map.erase(citer);
				citer = tmp;
			}
			else {
				++citer;
			}
		}
	}
}

void ControlOSC::finish_midi_binding_event (MidiBindingEvent & event)
{
	lo_address addr = find_or_cache_addr (event.ret_url);
	if (!addr) {
		return;
	}

	if (event.type == MidiBindingEvent::Learn) {
		// send back the event, then done
		if (lo_send(addr, event.ret_path.c_str(), "ss", "add", event.bind_str.c_str()) == -1) {
			fprintf(stderr, "OSC error sending binding %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
			return;
		}
		
		lo_send(addr, event.ret_path.c_str(), "ss", "done", event.bind_str.c_str());
	}
	else if (event.type == MidiBindingEvent::GetNextMidi) {
		lo_send(addr, event.ret_path.c_str(), "ss", "recv", event.bind_str.c_str());
	}
	else if (event.type == MidiBindingEvent::CancelLearn)
	{
		lo_send(addr, event.ret_path.c_str(), "ss", "learn_cancel", "");
	}
	else if (event.type == MidiBindingEvent::CancelGetNext)
	{
		lo_send(addr, event.ret_path.c_str(), "ss", "next_cancel", "");
	}
}

void
ControlOSC::send_registered_updates(string ctrl, float val, int instance, int source)
{
	InstancePair ipair(instance, ctrl);
	ControlRegistrationMap::iterator iter = _registration_map.find (ipair);
	//LastValueMap::iterator lastval;

	if (iter != _registration_map.end())
	{
		if ( ! send_registered_updates (iter, ctrl, val, instance, source)) {
			// remove ipair if false is returned.. no more good registrations
			_registration_map.erase(ipair);
		}
	}
	else {
#ifdef DEBUG
		//cerr << "not in map: " << instance << " ctrL: " << ctrl << endl;
#endif
	}
}


void ControlOSC::send_auto_updates (const std::list<short int> timeout_list)
{
	ControlRegistrationMapAuto::iterator iter = _auto_registration_map.begin();
	ControlRegistrationMapAuto::iterator tmpiter;

	while (iter != _auto_registration_map.end())
	{
		std::list<short int> timeout_after_opt = timeout_list;
		const InstancePair & ipair = (*iter).first;
		float val = _engine->get_control_value (_cmd_map->to_control_t(ipair.second), ipair.first);
		
		//_engine->ParamChanged(_cmd_map->to_control_t(ipair.second), ipair.first);

		if ( ! send_registered_auto_updates (iter, ipair, val, timeout_list)) {
			// remove ipair if false is returned.. no more good registrations
			tmpiter = iter;
			++iter;
			_auto_registration_map.erase(tmpiter);
		}
		else {
		++iter;
		}

	}
}

bool
ControlOSC::send_registered_auto_updates(ControlRegistrationMapAuto::iterator & iter,
				    const InstancePair & ipair, float val, const std::list<short int> timeout_list)
{
	UrlListAuto::iterator tmpurl;
	UrlListAuto & ulist_auto = (*iter).second;
	string ctrl = ipair.second;
	int instance = ipair.first;
	
	for (UrlListAuto::iterator url = ulist_auto.begin(); url != ulist_auto.end();)
	{
		bool unregister = false;
		lo_address addr = (*url).upair.first;
		for (std::list<short int>::const_iterator timeout = timeout_list.begin(); timeout != timeout_list.end(); timeout++) {
			if ((*url).timeout == (*timeout)) {
				LastValueMap *last_value_map = &_last_value_map[(*url).upair];
				//optimize out unecessary updates
				LastValueMap::iterator lastval = (*last_value_map).find (ipair);
				if (val != (*lastval).second) {

					if (lo_send(addr, (*url).upair.second.c_str(), "isf", instance, ctrl.c_str(), val) == -1) 
						unregister = true;
					else
						(*last_value_map)[ipair] = val;
				}

				break;
			}
		}
		if (unregister) {
			tmpurl = url;
			++url;
			ulist_auto.erase(tmpurl);
		} else {
			++url;
		}
	}
	
	if (ulist_auto.empty()) {
		return false;
	}

	return true;
}

bool
ControlOSC::send_registered_updates(ControlRegistrationMap::iterator & iter,
				    string ctrl, float val, int instance, int source)
{
	UrlList::iterator tmpurl;
	
	UrlList & ulist = (*iter).second;
	
	for (UrlList::iterator url = ulist.begin(); url != ulist.end();)
	{
		lo_address addr = (*url).first;
		const char * port = lo_address_get_port(addr);
		int aport = atoi (port);
		
		if (aport == source) {
			// ignore if this was caused by a set from this addr
			//cerr << "ignoreing address to send update for " << ctrl << "  port: " << aport << "  " << port << endl;
		}
		
		else if (lo_send(addr, (*url).second.c_str(), "isf", instance, ctrl.c_str(), val) == -1) {
#ifdef DEBUG
			fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
#endif
			// auto-unregister
			tmpurl = url;
			++url;
			
			ulist.erase(tmpurl);
			continue;
		}
		
		++url;
	}
	
	if (ulist.empty()) {
		return false;
	}

	return true;
}


void
ControlOSC::finish_register_event (RegisterConfigEvent &event)
{
	AddrPathPair apair(event.ret_url, event.ret_path);
	AddressList::iterator iter = find (_config_registrations.begin(), _config_registrations.end(), apair);

	if (iter == _config_registrations.end()) {
		_config_registrations.push_back (apair);
	}
}

void ControlOSC::send_all_config ()
{
	// for now just send pingacks to all registered addresses
	for (AddressList::iterator iter = _config_registrations.begin(); iter != _config_registrations.end(); ++iter)
	{
		send_pingack (true, false, (*iter).first, (*iter).second);
	}
}

void ControlOSC::send_error (std::string returl, std::string retpath, std::string mesg)
{
	lo_address addr;

	addr = find_or_cache_addr (returl);
	if (!addr) {
		return;
	}

	string oururl = get_server_url();
	
	if (lo_send(addr, retpath.c_str(), "ss", oururl.c_str(), mesg.c_str()) < 0) {
		fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
	}
}

void ControlOSC::send_pingack (bool useudp, bool use_id, string returl, string retpath)
{
	lo_address addr;

	addr = find_or_cache_addr (returl);
	if (!addr) {
		return;
	}

	string oururl;

	if (!useudp) {
		oururl = get_unix_server_url();
	}

	// default to udp
	if (oururl.empty()) {
		oururl = get_server_url();
	}
	
	//cerr << "sooperlooper: sending ping response to " << returl << endl;
	// sends our server URL, the SL version, the loop count, and a unique id for continuity checking
	if (use_id)
	{
		if (lo_send(addr, retpath.c_str(), "ssii", oururl.c_str(), sooperlooper_version, _engine->loop_count(), _engine->get_id()) < 0) {
			fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
		}
	}
	else {
		if (lo_send(addr, retpath.c_str(), "ssi", oururl.c_str(), sooperlooper_version, _engine->loop_count()) < 0) {
			fprintf(stderr, "OSC error %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
		}
	}
}


	
void ControlOSC::send_all_midi_bindings (MidiBindings * bindings, string returl, string retpath)
{
	lo_address addr = find_or_cache_addr (returl);
	if (!addr) {
		return;
	}

	MidiBindings::BindingList blist;
	bindings->get_bindings (blist);

	for (MidiBindings::BindingList::iterator biter = blist.begin(); biter != blist.end(); ++biter) {
		MidiBindInfo & info = (*biter);

		if (lo_send(addr, retpath.c_str(), "ss", "add", info.serialize().c_str()) == -1) {
			fprintf(stderr, "OSC error sending binding %d: %s\n", lo_address_errno(addr), lo_address_errstr(addr));
			break;
		}
	}

	lo_send(addr, retpath.c_str(), "ss", "done", "");
			
}

void ControlOSC::validate_returl(std::string & returl) 
{
	if (returl.substr(0,10) != "osc.udp://") 
		returl = "osc.udp://" + returl;
}
