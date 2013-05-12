#include "test_looper.hpp"

class TestBed 
{
    public:
        TestBed();
        ~TestBed();

        jack_client_t* jack;
        SooperLooper::TestLooper * looper;
    private:
        int connect_to_jack();
        static int _process_callback (jack_nframes_t, void*);
        int process_callback (jack_nframes_t);
};

