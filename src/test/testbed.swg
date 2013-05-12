%module testbed
%{
#include "testbed.hpp"
#include "test_looper.hpp"
#include "../plugin.hpp"
using namespace SooperLooper;
%}

%include "testbed.hpp"
%include "test_looper.hpp"
%include "../plugin.hpp"

//%include "std_vector.i"
//%include "soop.h"
//%include "plugin.hpp"
//
//%template(vectorSoopInstance) std::vector<SoopInstance * >;
//typedef float LADSPA_Data;
//
//%include "carrays.i"
//%array_class(float, floatArray);
