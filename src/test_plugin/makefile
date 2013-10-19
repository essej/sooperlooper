all:
	swig -python -c++ test_engine.swg  
	g++ -fPIC -fpermissive -g -shared -o _test_engine.so test_engine.cpp test_looper.cpp ../plugin.cc test_engine_wrap.cxx -I/usr/include/python2.7/ -ljack

clean:
	rm -f _test_engine.so test_engine.py test_engine.pyc testbed_wrap.cxx
