#include "test_looper.hpp"

class TestEngine 
{
    public:
        TestEngine(const char* name);
        ~TestEngine();

        jack_client_t* jack;
        SooperLooper::TestLooper * looper;
    private:
        int connect_to_jack(const char* name);
        static int _process_callback (jack_nframes_t, void*);
        int process_callback (jack_nframes_t);
};

