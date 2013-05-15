#include "test_engine.hpp"
#include <iostream>
using namespace std;

extern void sl_init ();
extern	void sl_fini ();

int
TestEngine::connect_to_jack (const char* name)
{
	if ((jack = jack_client_new (name)) == 0) {
		return -1;
	}

	if (jack_set_process_callback (jack, _process_callback, this) != 0) {
		return -1;
	}

	return 0;
}


TestEngine::TestEngine(const char* name)
{
    sl_init();
	if (connect_to_jack (name)) {
		return;
	}
    looper = new SooperLooper::TestLooper(jack, 0, 1);

    while (!looper->ok);//wait 

	if (jack_activate (jack)) {
		cerr << "cannot activate JACK\n";
		return;
	}
}

TestEngine::~TestEngine()
{
    //sl_fini();
    jack_client_close(jack);
}
    

int
TestEngine::_process_callback (jack_nframes_t nframes, void* arg)
{
	return static_cast<TestEngine*> (arg)->process_callback (nframes);
}

int
TestEngine::process_callback (jack_nframes_t nframes)
{
    looper->run(0, nframes);
	return 0;
}
