#include "testbed.hpp"
#include <iostream>
using namespace std;

extern void sl_init ();
extern	void sl_fini ();

int
TestBed::connect_to_jack ()
{
	if ((jack = jack_client_new ("SL_testbed")) == 0) {
		return -1;
	}

	if (jack_set_process_callback (jack, _process_callback, this) != 0) {
		return -1;
	}

	return 0;
}


TestBed::TestBed()
{
    sl_init();
	if (connect_to_jack ()) {
		return;
	}
    looper = new SooperLooper::TestLooper(jack, 0, 1);

    while (!looper->ok);//wait 

	if (jack_activate (jack)) {
		cerr << "cannot activate JACK\n";
		return;
	}
}

TestBed::~TestBed()
{
    sl_fini();
    jack_client_close(jack);
}
    

int
TestBed::_process_callback (jack_nframes_t nframes, void* arg)
{
	return static_cast<TestBed*> (arg)->process_callback (nframes);
}

int
TestBed::process_callback (jack_nframes_t nframes)
{
    looper->run(0, nframes);
	return 0;
}
