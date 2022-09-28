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

#include <cstdio>
#include <iostream>
#include <cerrno>
#include "loop_control.hpp"

#include <wx/filename.h>

#include <sys/poll.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#include <midi_bind.hpp>
#include "plugin.hpp"

using namespace std;
using namespace SooperLooperGui;
using namespace SooperLooper;

#define DEFAULT_OSC_PORT 9951

LoopControl::SpawnConfig::SpawnConfig(const wxString & nm)
	: name(nm), host(""), port(DEFAULT_OSC_PORT), num_loops(1), num_channels(2),
	  mem_secs(40.0f), discrete_io(true), exec_name("sooperlooper"), midi_bind_path(""), force_spawn(false), never_spawn(false),
	  never_timeout(false),
	  jack_name(""),
          window_x_pos(100), window_y_pos(100)
{
}


XMLNode& LoopControl::SpawnConfig::get_state () const
{
	char buf[40];
	XMLNode *node = new XMLNode ("context");

	node->add_property ("name", (const char *) name.ToAscii());

	node->add_property ("host", host);

	snprintf(buf, sizeof(buf), "%ld", port);
	node->add_property ("port", buf);

	snprintf(buf, sizeof(buf), "%ld", num_loops);
	node->add_property ("num_loops", buf);

	snprintf(buf, sizeof(buf), "%ld", num_channels);
	node->add_property ("num_channels", buf);

	snprintf(buf, sizeof(buf), "%f", mem_secs);
	node->add_property ("mem_secs", buf);

	node->add_property ("discrete_io", discrete_io ? "yes": "no");

	node->add_property ("exec_name", exec_name);
	node->add_property ("jack_name", jack_name);
	node->add_property ("jack_serv_name", jack_serv_name);
	node->add_property ("midi_bind_path", midi_bind_path);
	node->add_property ("session_path", session_path);

	node->add_property ("force_spawn", force_spawn ? "yes": "no");
	node->add_property ("never_timeout", never_timeout ? "yes": "no");


	return *node;
}

int LoopControl::SpawnConfig::set_state (const XMLNode& node)
{
	XMLNodeList nlist = node.children();
	XMLNodeConstIterator niter;
	XMLNode *child_node;

	wxString tmpstr;
	
	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		child_node = *niter;

		if (child_node->name() == "context") {
			XMLProperty *prop;
			
			if ((prop = child_node->property ("name")) != 0) {
				name =  wxString::FromAscii(prop->value().c_str());
			}
			if ((prop = child_node->property ("host")) != 0) {
				host = prop->value();
			}
			if ((prop = child_node->property ("port")) != 0) {
				port = atol (prop->value().c_str());
			}
			if ((prop = child_node->property ("num_loops")) != 0) {
				num_loops = atol (prop->value().c_str());
			}
			if ((prop = child_node->property ("num_channels")) != 0) {
				num_channels = atol (prop->value().c_str());
			}
			if ((prop = child_node->property ("mem_secs")) != 0) {
				mem_secs = atof (prop->value().c_str());
			}
			if ((prop = child_node->property ("discrete_io")) != 0) {
				if (prop->value() == "yes") {
				        discrete_io = true;
				}
				else {
					discrete_io = false;
				}
			}
			if ((prop = child_node->property ("exec_name")) != 0) {
				exec_name = prop->value();
			}
			if ((prop = child_node->property ("jack_name")) != 0) {
				jack_name = prop->value();
			}
			if ((prop = child_node->property ("jack_serv_name")) != 0) {
				jack_serv_name =  prop->value();
			}
			if ((prop = child_node->property ("midi_bind_path")) != 0) {
			        midi_bind_path = prop->value();
			}
			if ((prop = child_node->property ("session_path")) != 0) {
			        session_path = prop->value();
			}
			if ((prop = child_node->property ("force_spawn")) != 0) {
				if (prop->value() == "yes") {
					force_spawn = true;
				}
				else {
					force_spawn = false;
				}
			}
                        
			/*
			if ((prop = child_node->property ("never_timeout")) != 0) {
				if (prop->value() == "yes") {
					never_timeout = true;
				}
				else {
					never_timeout = false;
				}
			}
			*/
			break;
		}
	}

	return 0;
}


LoopControl::LoopControl (const wxString & rcdir)
	: _spawn_config(wxT("current")), _default_spawn_config(wxT("default"))
{
	_osc_traffic_thread = 0;
	_osc_addr = 0;
	_midi_bindings = new MidiBindings();
	_rcdir = rcdir;
	_sentinel = true;
	_lastchance = false;
	_we_spawned = false;
	_engine_id = 0;

	setup_param_map();
	
	_osc_server = lo_server_new(NULL, NULL);
	if (!_osc_server) return;
	
	char * tmpstr;
	tmpstr = lo_server_get_url (_osc_server);
	_our_url = tmpstr;
	free (tmpstr);
	
	_our_port = lo_server_get_port(_osc_server);

	cerr << "slgui: our URL is " << _our_url << endl;
	
	/* add handler for control param callbacks, first is loop index , 2nd arg ctrl string, 3nd arg value */
	lo_server_add_method(_osc_server, "/ctrl", "isf", LoopControl::_control_handler, this);

	// pingack expects: s:engine_url s:version i:loopcount
	lo_server_add_method(_osc_server, "/pingack", "ssi", LoopControl::_pingack_handler, this);
	lo_server_add_method(_osc_server, "/pingack", "ssii", LoopControl::_pingack_handler, this);

	lo_server_add_method(_osc_server, "/alive_resp", "ssi", LoopControl::_alive_handler, this);
	lo_server_add_method(_osc_server, "/alive_resp", "ssii", LoopControl::_alive_handler, this);

	lo_server_add_method(_osc_server, "/error", "ss", LoopControl::_error_handler, this);
	
	/* add handler for recving midi bindings, s:status s:serialized binding */
	lo_server_add_method(_osc_server, "/recv_midi_bindings", "ss", LoopControl::_midi_binding_handler, this);
	
	_pingack = false;
	_waiting = 0;
	_failed = false;
	_engine_pid = 0;

	_updatetimer = new LoopUpdateTimer(this);

	init_traffic_thread();
}


LoopControl::~LoopControl()
{
	terminate_traffic_thread();

	disconnect(false);

	if (_osc_server) {
		lo_server_free (_osc_server);
	}
	
	delete _updatetimer;
	delete _midi_bindings;
}

bool
LoopControl::init_traffic_thread()
{
	// thread for watching for new osc traffic

	_traffic_done = false;
	
	if (pipe (_traffic_request_pipe)) {
		cerr << "Cannot create midi request signal pipe" <<  strerror (errno) << endl;
		return false;
	}

	if (fcntl (_traffic_request_pipe[0], F_SETFL, O_NONBLOCK)) {
		cerr << "UI: cannot set O_NONBLOCK on signal read pipe " << strerror (errno) << endl;
		return false;
	}

	if (fcntl (_traffic_request_pipe[1], F_SETFL, O_NONBLOCK)) {
		cerr << "UI: cannot set O_NONBLOCK on signal write pipe " << strerror (errno) << endl;
		return false;
	}
	
	pthread_create (&_osc_traffic_thread, NULL, &LoopControl::_osc_traffic, this);
	if (!_osc_traffic_thread) {
		return false;
	}

	return true;
}

void
LoopControl::terminate_traffic_thread ()
{
	void* status;
	char c = 0;

	_traffic_done = true;

	if (write (_traffic_request_pipe[1], &c, 1) != 1) {
		cerr << "cannot send signal to traffic thread! " <<  strerror (errno) << endl;
	}

	pthread_join (_osc_traffic_thread, &status);

	close(_traffic_request_pipe[0]);
	close(_traffic_request_pipe[1]);
}

void
LoopControl::setup_param_map()
{
	state_map[LooperStateOff] = wxT("off");
	state_map[LooperStateOffMuted] = wxT("off");
	state_map[LooperStateWaitStart] = wxT("waiting start");
	state_map[LooperStateRecording] = wxT("recording");
	state_map[LooperStateWaitStop] = wxT("waiting stop");
	state_map[LooperStatePlaying] = wxT("playing");
	state_map[LooperStateOverdubbing] = wxT("overdubbing");
	state_map[LooperStateMultiplying] = wxT("multiplying");
	state_map[LooperStateInserting] = wxT("inserting");
	state_map[LooperStateReplacing] = wxT("replacing");
	state_map[LooperStateDelay] = wxT("delay");
	state_map[LooperStateMuted] = wxT("muted");
	state_map[LooperStatePaused] = wxT("paused");
	state_map[LooperStateScratching] = wxT("scratching");
	state_map[LooperStateOneShot] = wxT("one shot");
	state_map[LooperStateSubstitute] = wxT("substituting");
}


bool
LoopControl::connect()
{

	if (_osc_addr) {
		lo_address_free (_osc_addr);
	}

	if (_spawn_config.host.empty()) {
		_osc_addr = lo_address_new(NULL, (const char *) wxString::Format(wxT("%ld"), _spawn_config.port).ToAscii());
	}
	else {
		_osc_addr = lo_address_new((const char *)_spawn_config.host.c_str(), (const char *) wxString::Format(wxT("%ld"), _spawn_config.port).ToAscii());
	}
	//cerr << "osc errstr: " << lo_address_errstr(_osc_addr) << endl;

	// if the spawn_config host string is 127.0.0.1 or localhost, make our_url match it
	if (_spawn_config.host == "127.0.0.1" || _spawn_config.host == "localhost") {
		char tmpbuf[100];
		//snprintf (tmpbuf, sizeof(tmpbuf), "osc.udp://localhost:%d/", _our_port);
                snprintf (tmpbuf, sizeof(tmpbuf), "osc.udp://127.0.0.1:%d/", _our_port);
		_our_url = tmpbuf;
		cerr << "Changing our url to be : " << _our_url << endl;
	}

	_pingack = false;

	_registeredauto_loop_map.clear();
	_registeredin_loop_map.clear();
	
	if (!_spawn_config.force_spawn) {
		// send off a ping.  set a timer, if we don't have a response, we'll start our own locally
		_waiting = 0;
		lo_send(_osc_addr, "/ping", "ssi", _our_url.c_str(), "/pingack", 1);
		//cerr << "sending initial ping" << endl;
		_updatetimer->Start(700, true);
	}
	// spawn now
	else if (!_spawn_config.never_spawn && spawn_looper()) {
            //cerr << "immediate spawn" << endl;
		_waiting = 1;
		_updatetimer->Start(100, true);
	}
	else {
		return false;
	}
	
	return true;
}

void
LoopControl::register_global_updates(bool unreg)
{
	char buf[50];

	if (!_osc_addr) return;
	
	if (unreg) {
		snprintf(buf, sizeof(buf), "/unregister_update");

	} else {
		snprintf(buf, sizeof(buf), "/register_update");
	}

	// results will come back with instance = -2
	lo_send(_osc_addr, buf, "sss", "tempo", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "sync_source", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "eighth_per_cycle", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "tap_tempo", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "wet", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "dry", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "input_gain", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "auto_disable_latency", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "output_midi_clock", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_midi_stop", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_midi_start", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "send_midi_start_on_trigger", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "smart_eighths", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "selected_loop_num", _our_url.c_str(), "/ctrl");
	
	if (unreg) {
		lo_send(_osc_addr, "/unregister_auto_update", "siss", "in_peak_meter", 100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, "/unregister_auto_update", "siss", "out_peak_meter", 100, _our_url.c_str(), "/ctrl");
	}
	else {
		lo_send(_osc_addr, "/register_auto_update", "siss", "in_peak_meter", 100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, "/register_auto_update", "siss", "out_peak_meter", 100, _our_url.c_str(), "/ctrl");
	}
}

bool
LoopControl::disconnect (bool killit)
{

	if (_osc_addr) {
		register_global_updates(true); // unregister
		
		if (killit) {
			send_quit();
		}
		
		lo_address_free (_osc_addr);
		_osc_addr = 0;
	}
	_osc_url = "";
	_we_spawned = false;
	
	Disconnected(); // emit
	
	return true;
}


void *
LoopControl::_osc_traffic (void *arg)
{
	LoopControl * lc = static_cast<LoopControl *>(arg);
	lc->osc_traffic();
	return 0;
}

void
LoopControl::osc_traffic()
{
	// our only job here in this thread is
	// to send updates when new data comes in on the osc port
	int oscfd = lo_server_get_socket_fd(_osc_server);
	struct pollfd pfd[2];
	int timeout = -1; 
	int nfds = 2;
	struct timespec nsleep = { 0, 20000000 };

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	pfd[0].fd = _traffic_request_pipe[0];
	pfd[0].events = POLLIN|POLLHUP|POLLERR;
	pfd[1].fd = oscfd;
	pfd[1].events = POLLIN|POLLHUP|POLLERR;
	
	while (!_traffic_done)
	{
		pfd[0].fd = _traffic_request_pipe[0];
		pfd[0].events = POLLIN|POLLHUP|POLLERR;
		pfd[1].fd = oscfd;
		pfd[1].events = POLLIN|POLLHUP|POLLPRI|POLLERR;

		pthread_testcancel();
		
		if (poll (pfd, nfds, timeout) < 0) {
			if (errno == EINTR) {
				/* gdb at work, perhaps */
				// goto again;
			}
			
			cerr << "OSC thread poll failed: " <<  strerror (errno) << endl;
			
			continue;
		}

		if (_traffic_done) {
			break;
		}

		if (nfds > 1 && (pfd[1].revents & POLLIN)) 
		{
			
			// emit signal
			// cerr << "got new data" << endl;
			NewDataReady(); // emit
		}

		// sleep for a bit to give someone the chance to read it
		nanosleep (&nsleep, NULL);

	}
}

bool
LoopControl::is_engine_local()
{
	// compare host parts of osc_url and our_url
	bool ret = false;
	char * remhost = lo_url_get_hostname(_osc_url.c_str());
	char * lochost = lo_url_get_hostname(_our_url.c_str());

	// cerr << "osc: " << _osc_url << "  remhost: " << remhost << "  lochost: " << lochost << endl;
	
	if (_host == wxT("0.0.0.0") || // special case ridiculous mac-ness
	    (remhost && lochost && (strncmp(remhost, lochost, 30) == 0))) {
		ret = true;
	}

	if (remhost) {
		free(remhost);
	}
	if (lochost) {
		free(lochost);
	}

	return ret;
}


void
LoopControl::pingtimer_expired()
{ 
	update_values();

	// check state of pingack

	if (_pingack) {
		 cerr << "got ping response" << endl;
	}
	else if (_waiting > 0)
	{
		if (_waiting > 80) {
			if (_lastchance) {
				// give up
				cerr << "slgui: gave up on spawned engine" << endl;
				if (_osc_addr) {
					lo_address_free(_osc_addr);
				}
				_osc_addr = 0;
				_failed = true;
				ConnectFailed("No response from SooperLooper engine process.\nPlease make sure the JACK server is running.\nAlso check that the system's hostname resolves properly."); // emit
			}
			else {
				// last ditch effort with 127.0.0.1 all around
				_osc_addr = lo_address_new("127.0.0.1", (const char *) wxString::Format(wxT("%ld"), _spawn_config.port).ToAscii());

				char tmpbuf[100];
				int sport = lo_server_get_port (_osc_server);
				snprintf (tmpbuf, sizeof(tmpbuf), "osc.udp://127.0.0.1:%d/", sport);
				_our_url = tmpbuf;

				//cerr << "last chance effort: with oururl " << _our_url << endl;
				
				// send off a ping.  
				_pingack = false;
				_waiting = 60;
				lo_send(_osc_addr, "/ping", "ssi", _our_url.c_str(), "/pingack", 1);
				_lastchance = 1;
				_updatetimer->Start(100, true);
			}
		}
		else {
                    //cerr << "waiting " << _waiting << endl;
			_waiting++;
                        if (_waiting == 1 || (_waiting % 2) == 0) {
                            lo_send(_osc_addr, "/ping", "ssi", _our_url.c_str(), "/pingack", 1);
                        }
			_updatetimer->Start(100, true);
		}
	}
	else
	{
		// lets try to spawn our own
		if (!_spawn_config.never_spawn && spawn_looper()) {
			cerr << "slgui: spawned new engine" << endl;
			_updatetimer->Start(100, true);
			_waiting = 1;
		}
		else if (!_spawn_config.never_timeout)
		{
			//cerr << "execute failed" << endl;
			if (_osc_addr) {
				lo_address_free(_osc_addr);
			}
			_osc_addr = 0;
			_failed = true;
			if (!_spawn_config.never_spawn) {
				ConnectFailed("Execution of SooperLooper engine process failed."); // emit
			}
		}
			
	}
}

bool LoopControl::spawn_looper()
{
	// use wxExecute
	char tmpbuf[300];
	string cmdstr = _spawn_config.exec_name;
	
	if (cmdstr.empty()) {
		cmdstr = "sooperlooper"; // always force something
	}

	snprintf(tmpbuf, sizeof(tmpbuf), " -q -U %s -p %ld -l %ld -c %ld -t %d",
		 _our_url.c_str(),
		 _spawn_config.port,
		 _spawn_config.num_loops,
		 _spawn_config.num_channels,
		 (int) _spawn_config.mem_secs
		);
	
	cmdstr += tmpbuf;
	
	if (!_spawn_config.midi_bind_path.empty()) {
		snprintf(tmpbuf, sizeof(tmpbuf), " -m \"%s\"", _spawn_config.midi_bind_path.c_str());	
		cmdstr += tmpbuf;
	}
	else {
		// try default
		//wxString defpath = (_rcdir + wxFileName::GetPathSeparator() + wxT("default_midi.slb"));
		//cmdstr += wxString::Format(wxT(" -m \"%s\""), defpath.c_str());

		string defpath = string(_rcdir.fn_str()) + "/" + "default_midi.slb";
		snprintf(tmpbuf, sizeof(tmpbuf), " -m \"%s\"", defpath.c_str());	
		cmdstr += tmpbuf;
	}
	
	if (!_spawn_config.jack_name.empty()) {
		//cmdstr += wxString::Format(wxT(" -j \"%s\""), _spawn_config.jack_name.c_str());
		snprintf(tmpbuf, sizeof(tmpbuf), " -j \"%s\"", _spawn_config.jack_name.c_str());	
		cmdstr += tmpbuf;
	}

	if (!_spawn_config.jack_serv_name.empty()) {
		//cmdstr += wxString::Format(wxT(" -S \"%s\""), _spawn_config.jack_serv_name.c_str());
		snprintf(tmpbuf, sizeof(tmpbuf), " -S \"%s\"", _spawn_config.jack_serv_name.c_str());	
		cmdstr += tmpbuf;
	}

	if (!_spawn_config.discrete_io) {
		//cmdstr += wxString::Format(wxT(" -D \"no\""));
		snprintf(tmpbuf, sizeof(tmpbuf), " -D no");	
		cmdstr += tmpbuf;
	}
	
	if (!_spawn_config.session_path.empty()) {
		//cmdstr += wxString::Format(wxT(" -L \"%s\""), _spawn_config.session_path.c_str());
		cerr << "session path not empty when spawning: " << _spawn_config.session_path << endl;
		snprintf(tmpbuf, sizeof(tmpbuf), " -L \"%s\"", _spawn_config.session_path.c_str());	
		cmdstr += tmpbuf;
	}


//#ifdef DEBUG
	cerr << "execing: '" << cmdstr.c_str() << "'" << endl << endl;
//#endif
	
	_engine_pid = wxExecute(wxString::FromAscii(cmdstr.c_str()), wxEXEC_ASYNC|wxEXEC_MAKE_GROUP_LEADER);

#ifdef DEBUG
	cerr << "pid is " << _engine_pid << endl;
#endif
	if (_engine_pid > 0) {
		_we_spawned = true;
	}
	
	return _engine_pid > 0;
}

int
LoopControl::_pingack_handler(const char *path, const char *types, lo_arg **argv, int argc,
			      void *data, void *user_data)
{
	LoopControl * lc = static_cast<LoopControl*> (user_data);
	return lc->pingack_handler (path, types, argv, argc, data);
}

int
LoopControl::pingack_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	// s:hosturl  s:version  i:loopcount

	string hosturl(&argv[0]->s);
	string version(&argv[1]->s);
	int loopcount = argv[2]->i;
	int uid = 0;
	char tmpbuf[100];
	
	if (argc > 3) {
		uid = argv[3]->i;
	}

	cerr << "slgui: remote looper is at " << hosturl << " version=" << version << "   loopcount=" << loopcount << "  id=" << uid << endl;

        if (loopcount < _registeredin_loop_map.size()) {
                _registeredin_loop_map.erase(_registeredin_loop_map.find(loopcount), _registeredin_loop_map.end());
                _registeredauto_loop_map.erase(_registeredauto_loop_map.find(loopcount), _registeredauto_loop_map.end());
        }

	char * remport = lo_url_get_port(hosturl.c_str());
	wxString tmpstr = wxString::FromAscii(remport);
	tmpstr.ToLong(&_spawn_config.port);
	free(remport);
	
	if (_lastchance) {
		// force a 127.0.0.1 hostname if this was on our last chance
		//cerr << "forced a 127" << endl;
		snprintf(tmpbuf, sizeof(tmpbuf), "osc.udp://127.0.0.1:%ld/", _spawn_config.port);
		_osc_url = tmpbuf;
	}
	else {
		_osc_url = hosturl;
	}
	
	char * remhost = lo_url_get_hostname(_osc_url.c_str());
	if (_spawn_config.host.empty() && !_lastchance) {
		// only if the spawn config host was empty do we use what is returned
		_host = wxString::FromAscii(remhost);
		_spawn_config.host = remhost;
	}
	else {
		// use the one given
		_host = wxString::FromAscii(_spawn_config.host.c_str());
		snprintf(tmpbuf, sizeof(tmpbuf), "osc.udp://%s:%ld/", _spawn_config.host.c_str(), _spawn_config.port);
		_osc_url = tmpbuf;
		cerr << "  but treating the engine URL as " << _osc_url << endl;
	}

	free(remhost);



	_port = (int) _spawn_config.port;

	if (_osc_addr) {
		lo_address_free(_osc_addr);
	}
	_osc_addr = lo_address_new_from_url (hosturl.c_str());

	if (_engine_id != 0 && _engine_id != uid && uid != 0) {
		cerr << "new engine ID pingacked us!, re-registering" << endl;
		_pingack = false;
	}
        if (uid != 0) {
                _engine_id = uid;
        }

	if (!_pingack) {
		// register future configs with it once
		lo_send(_osc_addr, "/register", "ss", _our_url.c_str(), "/pingack");

		register_global_updates();		

		request_all_midi_bindings();

		_pingack = true;
	}

	
	LooperConnected (loopcount); // emit

	return 0;
}

int
LoopControl::_alive_handler(const char *path, const char *types, lo_arg **argv, int argc,
			      void *data, void *user_data)
{
	LoopControl * lc = static_cast<LoopControl*> (user_data);
	return lc->alive_handler (path, types, argv, argc, data);
}

int
LoopControl::alive_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	// s:hosturl  s:version  i:loopcount [i:id]
	if (argc > 3) {
		int uid = argv[3]->i;
		if (uid != _engine_id && uid != 0) {
			cerr << "engine changed on us, redoing connections" << endl;
			return pingack_handler(path, types, argv, argc, data);
		}
	}

	IsAlive(); // emit
	return 0;
}

int
LoopControl::_error_handler(const char *path, const char *types, lo_arg **argv, int argc,
			      void *data, void *user_data)
{
	LoopControl * lc = static_cast<LoopControl*> (user_data);
	return lc->error_handler (path, types, argv, argc, data);
}

int
LoopControl::error_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	// s:errstr
	string errsrc(&argv[0]->s);
	string errstr(&argv[1]->s);

	ErrorReceived(errstr); // emit
	return 0;
}



int
LoopControl::_control_handler(const char *path, const char *types, lo_arg **argv, int argc,
			      void *data, void *user_data)
{
	LoopControl * lc = static_cast<LoopControl*> (user_data);
	return lc->control_handler (path, types, argv, argc, data);
}


int
LoopControl::control_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	// loop instance is 1st arg, 2nd is ctrl string, 3rd is float value

	int  index = argv[0]->i;
	wxString ctrl = wxString::FromAscii(&argv[1]->s);
	float val  = argv[2]->f;


	// cerr << "got " << ctrl.ToAscii() << " = " << val << "  index=" << index << endl;
	
	if (index == -2)
	{
		// global ctrls
		if (_global_val_map.find(ctrl) == _global_val_map.end()
		    || _global_val_map[ctrl] != val)
		{
			_global_updated[ctrl] = true;

		}
	
		_global_val_map[ctrl] = val;
		
		return 0;
	}
	else if (index < 0) {
		return 0;
	}
	else if (index >= (int) _params_val_map.size()) {
		_params_val_map.resize(index + 1);
		_updated.resize(index + 1);
	}

	if (_params_val_map[index].find(ctrl) == _params_val_map[index].end()
	    || _params_val_map[index][ctrl] != val)
	{
		_updated[index][ctrl] = true;
	}
	
	_params_val_map[index][ctrl] = val;

	
	return 0;
}

int
LoopControl::_midi_binding_handler(const char *path, const char *types, lo_arg **argv, int argc,
			      void *data, void *user_data)
{
	LoopControl * lc = static_cast<LoopControl*> (user_data);
	return lc->midi_binding_handler (path, types, argv, argc, data);
}

int
LoopControl::midi_binding_handler(const char *path, const char *types, lo_arg **argv, int argc, void *data)
{
	// s:status s:serialized binding
	string status(&argv[0]->s);
	string bindstr(&argv[1]->s);
	
	MidiBindInfo info;
	
	if (status == "add") {
		if (info.unserialize (bindstr)) {
			_midi_bindings->add_binding(info);
		}
	}
	else if (status == "done") {
		info.unserialize (bindstr);
		MidiBindingChanged(info); // emit
	}
	else if (status == "recv") {
		info.unserialize (bindstr);
		ReceivedNextMidi(info); // emit
	}
	else if (status == "learn_cancel") {
		MidiLearnCancelled(); // emit
	}
	else if (status == "next_cancel") {
		NextMidiCancelled(); // emit
	}
	
	return 0;
}

void
LoopControl::request_control_value (int index,  wxString ctrl)
{
	if (!_osc_addr) return;
	char buf[20];

	snprintf(buf, sizeof(buf), "/sl/%d/get", index);
	
	lo_send(_osc_addr, buf, "sss", (const char *) ctrl.ToAscii(), _our_url.c_str(), "/ctrl");
}

void
LoopControl::request_global_control_value (wxString ctrl)
{
	if (!_osc_addr) return;
	char buf[20];

	snprintf(buf, sizeof(buf), "/get");
	
	lo_send(_osc_addr, buf, "sss", (const char *) ctrl.ToAscii(), _our_url.c_str(), "/ctrl");
}

void
LoopControl::request_global_values()
{
	if (!_osc_addr) return;
	char buf[20];

	snprintf(buf, sizeof(buf), "/get");
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", "tempo", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "sync_source", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "eighth_per_cycle", _our_url.c_str(), "/ctrl");
	//lo_send(_osc_addr, buf, "sss", "tap_tempo", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "dry", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "wet", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "input_gain", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "auto_disable_latency", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "smart_eighths", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "output_midi_clock", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_midi_stop", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_midi_start", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "send_midi_start_on_trigger", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "selected_loop_num", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "in_peak_meter", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "out_peak_meter", _our_url.c_str(), "/ctrl");

}

void
LoopControl::request_values(int index)
{
	if (!_osc_addr) return;
	char buf[20];

	snprintf(buf, sizeof(buf), "/sl/%d/get", index);
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", "state", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "loop_pos", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "loop_len", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "cycle_len", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "free_time", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "total_time", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "waiting", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "next_state", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "rate_output", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "channel_count", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "in_peak_meter", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "out_peak_meter", _our_url.c_str(), "/ctrl");

}

void
LoopControl::request_all_values(int index)
{
	if (!_osc_addr) return;
	char buf[20];

	snprintf(buf, sizeof(buf), "/sl/%d/get", index);

	request_values(index);
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", "rec_thresh", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "feedback", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_feedback_play", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "dry", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "wet", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "input_gain", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "rate", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "scratch_pos", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "delay_trigger", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "quantize", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "fade_samples", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "round", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "sync", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "playback_sync", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "redo_is_tap", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_rate", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_common_outs", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_common_ins", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "has_discrete_io", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "relative_sync", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "input_latency", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "output_latency", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "trigger_latency", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "autoset_latency", _our_url.c_str(), "/ctrl");
    lo_send(_osc_addr, buf, "sss", "discrete_prefader", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "mute_quantized", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "overdub_quantized", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "replace_quantized", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "round_integer_tempo", _our_url.c_str(), "/ctrl");
	//lo_send(_osc_addr, buf, "sss", "pitch_shift", _our_url.c_str(), "/ctrl");
	//lo_send(_osc_addr, buf, "sss", "stretch_ratio", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "tempo_stretch", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "pan_1", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "pan_2", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "pan_3", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "pan_4", _our_url.c_str(), "/ctrl");

}

void
LoopControl::request_all_midi_bindings()
{
	if (!_osc_addr) return;

	_midi_bindings->clear_bindings();
	lo_send(_osc_addr, "/get_all_midi_bindings", "ss", _our_url.c_str(), "/recv_midi_bindings");
}

void
LoopControl::learn_midi_binding(const MidiBindInfo & info, bool exclusive)
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/learn_midi_binding", "ssss", info.serialize().c_str(), exclusive?"exclusive":"",_our_url.c_str(), "/recv_midi_bindings" );
}

void
LoopControl::request_next_midi_event ()
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/get_next_midi_event", "ss", _our_url.c_str(), "/recv_midi_bindings" );
}


void
LoopControl::add_midi_binding(const MidiBindInfo & info, bool exclusive)
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/add_midi_binding", "ss", info.serialize().c_str(), exclusive?"exclusive":"");
}

void
LoopControl::remove_midi_binding(const MidiBindInfo & info)
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/remove_midi_binding", "ss", info.serialize().c_str(),"");
}

void
LoopControl::clear_midi_bindings()
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/clear_midi_bindings", "");
	request_all_midi_bindings();
}

void
LoopControl::cancel_next_midi_event()
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/cancel_get_next_midi", "ss", _our_url.c_str(), "/recv_midi_bindings" );
}

void
LoopControl::cancel_midi_learn()
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/cancel_midi_learn", "ss", _our_url.c_str(), "/recv_midi_bindings" );
}

void
LoopControl::load_midi_bindings(const wxString & filename, bool append)
{
	lo_send(_osc_addr, "/load_midi_bindings", "ss", (const char *) filename.ToAscii(), append ? "add": "");

	request_all_midi_bindings();
}

void
LoopControl::save_midi_bindings(const wxString & filename)
{
	if (!_osc_addr) return;

	lo_send(_osc_addr, "/save_midi_bindings", "ss", (const char *) filename.ToAscii(), "");
}


int
LoopControl::register_all_in_new_thread(int number_of_loops)
{

	pthread_t register_thread;
	get_spawn_config().num_loops = number_of_loops; //XXX why don't we have the right num_loops already?
	return pthread_create (&register_thread, NULL, &LoopControl::_register_all, this);

}

void *
LoopControl::_register_all (void *arg)
{
	//send all registrations pausing inbetween toavoid losing osc messages

	LoopControl * lc = static_cast<LoopControl *>(arg);

	lc->request_global_values ();

	int num_of_loops = lc->get_spawn_config().num_loops;

	for (int i = 0; i < num_of_loops; i++) {
			lc->register_auto_updates(i);
			lc->register_input_controls(i);
			lc->request_all_values (i);
#if wxCHECK_VERSION(2,5,3)
		::wxMilliSleep(100);
#else
		::wxUsleep(100);
#endif
	}
	return 0;
}

void
LoopControl::register_auto_updates(int index, bool unreg)
{
	if (!_osc_addr) return;
	char buf[30];


	if (unreg) {
		snprintf(buf, sizeof(buf), "/sl/%d/unregister_auto_update", index);
		lo_send(_osc_addr, buf, "sss", "state", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "next_state", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "loop_pos", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "loop_len", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "cycle_len", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "free_time", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "total_time", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "waiting", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "rate_output", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "in_peak_meter", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "out_peak_meter", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "is_soloed", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "stretch_ratio", _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "sss", "pitch_shift", _our_url.c_str(), "/ctrl");
	} else {
                if (_registeredauto_loop_map.find(index) != _registeredauto_loop_map.end()) {
                        // already registered
                        return;
                }
                

		snprintf(buf, sizeof(buf), "/sl/%d/register_auto_update", index);
		// send request for auto updates
		lo_send(_osc_addr, buf, "siss", "state",     100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss", "next_state",     100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss", "loop_pos",  100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss", "loop_len",  100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss", "cycle_len", 100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss", "free_time", 100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss", "total_time",100,  _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss", "waiting",   100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss", "rate_output",100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss", "in_peak_meter", 100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss", "out_peak_meter", 100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss",  "is_soloed", 100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss", "stretch_ratio", 100, _our_url.c_str(), "/ctrl");
		lo_send(_osc_addr, buf, "siss", "pitch_shift", 100, _our_url.c_str(), "/ctrl");

                _registeredauto_loop_map[index] = true;
	}

	

}

void
LoopControl::register_input_controls(int index, bool unreg)
{
	if (!_osc_addr) return;
	char buf[50];

        if (_registeredin_loop_map.find(index) != _registeredin_loop_map.end()) {
                // already registered
                return;
        }

        if ((int)_params_val_map.size() <= index) {
		_params_val_map.resize(index + 1);
		_updated.resize(index + 1);
        }

	if ((int)_params_val_map.size() > index) {
		_params_val_map[index].clear();
		_updated[index].clear();
	}
	
	if (unreg) {
		snprintf(buf, sizeof(buf), "/sl/%d/unregister_update", index);

	} else {
		snprintf(buf, sizeof(buf), "/sl/%d/register_update", index);
	}
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", "rec_thresh", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "feedback", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_feedback_play", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "dry", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "wet", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "input_gain", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "rate", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "scratch_pos", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "delay_trigger", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "quantize", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "fade_samples", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "round", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "sync", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "playback_sync", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "redo_is_tap", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_rate", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_common_ins", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "use_common_outs", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "relative_sync", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "input_latency", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "output_latency", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "trigger_latency", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "autoset_latency", _our_url.c_str(), "/ctrl");
    lo_send(_osc_addr, buf, "sss", "discrete_prefader", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "mute_quantized", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "overdub_quantized", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "replace_quantized", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "round_integer_tempo", _our_url.c_str(), "/ctrl");
	//lo_send(_osc_addr, buf, "sss", "pitch_shift", _our_url.c_str(), "/ctrl");
	//lo_send(_osc_addr, buf, "sss", "stretch_ratio", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "tempo_stretch", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "pan_1", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "pan_2", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "pan_3", _our_url.c_str(), "/ctrl");
	lo_send(_osc_addr, buf, "sss", "pan_4", _our_url.c_str(), "/ctrl");

        // cerr << "SENT REGISTERS FOR ALL index: " << index << endl;

        _registeredin_loop_map[index] = true;
}

void
LoopControl::register_control (int index, wxString ctrl, bool unreg)
{
	if (!_osc_addr) return;

	char buf[30];

	if ((int)_params_val_map.size() > index) {
		_params_val_map[index].clear();
		_updated[index].clear();
	}
	
	if (unreg) {
		snprintf(buf, sizeof(buf), "/sl/%d/unregister_update", index);

	} else {
		snprintf(buf, sizeof(buf), "/sl/%d/register_update", index);
	}
	
	// send request for updates
	lo_send(_osc_addr, buf, "sss", (const char *) ctrl.ToAscii(), _our_url.c_str(), "/ctrl");

}

void
LoopControl::register_auto_update(int index, wxString ctrl, bool unreg)
{
	if (!_osc_addr) return;

	char buf[50];

	if (unreg) {
		snprintf(buf, sizeof(buf), "/sl/%d/unregister_auto_update", index);

		lo_send(_osc_addr, buf, "sss", (const char *) ctrl.ToAscii(), _our_url.c_str(), "/ctrl");

	} else {
		snprintf(buf, sizeof(buf), "/sl/%d/register_auto_update", index);

		lo_send(_osc_addr, buf, "siss", (const char *) ctrl.ToAscii(), 100, _our_url.c_str(), "/ctrl");
	}
	
}


bool
LoopControl::post_down_event(int index, wxString cmd)
{
	if (!_osc_addr) return false;
	char buf[50];

	snprintf(buf, sizeof(buf), "/sl/%d/down", index);
	
	if (lo_send(_osc_addr, buf, "s", (const char *)cmd.ToAscii()) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_up_event(int index, wxString cmd, bool force)
{
	if (!_osc_addr) return false;
	char buf[50];

	if (force) {
		snprintf(buf, sizeof(buf), "/sl/%d/upforce", index);
	}
	else {
		snprintf(buf, sizeof(buf), "/sl/%d/up", index);
	}
	
	if (lo_send(_osc_addr, buf, "s", (const char *) cmd.ToAscii()) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_ctrl_change (int index, wxString ctrl, float val)
{
	if (!_osc_addr) return false;
	char buf[50];

	// go ahead and update our local copy
	if (index >= 0 && index < (int) _params_val_map.size()) {
		_params_val_map[index][ctrl] = val;
	}
	
	snprintf(buf, sizeof(buf), "/sl/%d/set", index);

	if (lo_send(_osc_addr, buf, "sf", (const char *) ctrl.ToAscii(), val) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_global_ctrl_change (wxString ctrl, float val)
{
	if (!_osc_addr) return false;

	// go ahead and update our local copy
	_global_val_map[ctrl] = val;
	
	if (lo_send(_osc_addr, "/set", "sf", (const char *) ctrl.ToAscii(), val) == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_save_loop(int index, wxString fname, wxString format, wxString endian)
{
	if (!_osc_addr) return false;
	char buf[50];

	snprintf(buf, sizeof(buf), "/sl/%d/save_loop", index);

	// send request for updates
	if (lo_send(_osc_addr, buf, "sssss", (const char *) fname.ToAscii(),
		    (const char *) format.ToAscii(), (const char *) endian.ToAscii(),
		    (const char *) _our_url.c_str(), "/error") == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_load_loop(int index, wxString fname)
{
	if (!_osc_addr) return false;
	char buf[30];

	snprintf(buf, sizeof(buf), "/sl/%d/load_loop", index);

	// send request for updates
	if (lo_send(_osc_addr, buf, "sss", (const char *) fname.ToAscii(), _our_url.c_str(), "/error") == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_load_session(wxString fname)
{
	if (!_osc_addr) return false;
	char buf[30];

	snprintf(buf, sizeof(buf), "/load_session");

	// send request for updates
	if (lo_send(_osc_addr, buf, "sss", (const char *) fname.ToAscii(), _our_url.c_str(), "/error") == -1) {
		return false;
	}

	return true;
}

bool
LoopControl::post_save_session(wxString fname, bool write_audio)
{
	if (!_osc_addr) return false;
	char buf[30];

	snprintf(buf, sizeof(buf), "/save_session");


	if (write_audio) {
		if (lo_send(_osc_addr, buf, "sssi", (const char *) fname.ToAscii(), _our_url.c_str(), "/error", write_audio) == -1) {
			return false;
		}
	}
	else {
		if (lo_send(_osc_addr, buf, "sss", (const char *) fname.ToAscii(), _our_url.c_str(), "/error") == -1) {
			return false;
		}
	}

	return true;
}


void
LoopControl::send_quit()
{
	if (!_osc_addr) return;
	lo_send(_osc_addr, "/quit", NULL);
}

void
LoopControl::send_alive_ping()
{
	if (!_osc_addr) return;
	lo_send(_osc_addr, "/ping", "ssi", _our_url.c_str(), "/alive_resp", 1);
}

void
LoopControl::update_values()
{
	if (!_osc_server) return;
	// recv commands nonblocking, until none left

	while (lo_server_recv_noblock (_osc_server, 0) > 0)
	{
		// do nothing
	}

}


bool
LoopControl::is_updated (int index, wxString ctrl)
{
	if (index >= 0 && index < (int) _updated.size())
	{
		ControlValMap::iterator iter = _updated[index].find (ctrl);

		if (iter != _updated[index].end()) {
			return (*iter).second;
		}
	}

	return false;
}
	
bool
LoopControl::get_value (int index, wxString ctrl, float & retval)
{
	bool ret = false;
	
	if (index >= 0 && index < (int) _params_val_map.size())
	{
		ControlValMap::iterator iter = _params_val_map[index].find (ctrl);

		if (iter != _params_val_map[index].end()) {
			retval = (*iter).second;
			// set updated to false
			_updated[index][ctrl] = false;
			ret = true;
		}
	}

	return ret;
}


bool
LoopControl::is_global_updated (wxString ctrl)
{
        UpdatedCtrlMap::iterator iter = _global_updated.find (ctrl);
	
	if (iter != _global_updated.end()) {
		return (*iter).second;
	}

	return false;
}

bool
LoopControl::get_global_value (wxString ctrl, float & retval)
{
	bool ret = false;
	
	ControlValMap::iterator iter = _global_val_map.find (ctrl);
	
	if (iter != _global_val_map.end()) {
		retval = (*iter).second;
		// set updated to false
		_global_updated[ctrl] = false;
		ret = true;
	}

	return ret;
}


bool
LoopControl::get_state (int index, LooperState & state, wxString & statestr)
{
	bool ret = false;

	if (index >= 0 && index < (int) _params_val_map.size())
	{
		ControlValMap::iterator iter = _params_val_map[index].find (wxT("state"));

		if (iter != _params_val_map[index].end()) {
			state = (LooperState) (int) (*iter).second;
			statestr = state_map[state];
			// set updated to false
			_updated[index][wxT("state")] = false;
			ret = true;
		}
	}

	return ret;
}

bool
LoopControl::get_next_state (int index, LooperState & state, wxString & statestr)
{
	bool ret = false;

	if (index >= 0 && index < (int) _params_val_map.size())
	{
		ControlValMap::iterator iter = _params_val_map[index].find (wxT("next_state"));

		if (iter != _params_val_map[index].end()) {
			state = (LooperState) (int) (*iter).second;
			statestr = state_map[state];
			// set updated to false
			_updated[index][wxT("next_state")] = false;
			ret = true;
		}
	}

	return ret;
}

bool
LoopControl::post_add_loop(int channels, float secs, bool discrete)
{
	if (!_osc_addr) return false;

	if (discrete) {
		if (lo_send(_osc_addr, "/loop_add", "if", channels, secs) == -1) {
			return false;
		}
	}
	else {
		if (lo_send(_osc_addr, "/loop_add", "ifi", channels, secs, 0) == -1) {
			return false;
		}
	}

	return true;
}

bool
LoopControl::post_remove_loop()
{
	if (!_osc_addr) return false;
	// todo specify loop channels etc
	int index = -1;
	
	if (lo_send(_osc_addr, "/loop_del", "i", index) == -1) {
		return false;
	}

	return true;
}

