all:
	swig -python -c++ testbed.swg  
	g++ -fPIC -fpermissive -g -shared -o _testbed.so testbed.cpp test_looper.cpp ../plugin.cc testbed_wrap.cxx -I/usr/include/python2.7/ -ljack

clean:
	rm -f _testbed.so testbed.py testbed.pyc testbed_wrap.cxx
