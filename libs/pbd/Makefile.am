MAINTAINERCLEANFILES = autom4te.cache  Makefile.in aclocal.m4 configure 
						config.h.in stamp-h.in pbd-config libpbd.spec \
                        config.guess mkinstalldirs \
                        missing install-sh config.sub ltconfig \
                        ltmain.sh acinclude.m4 \
						config.log config.status depcomp

SUBDIRS = pbd

noinst_LIBRARIES    = libpbd.a

libpbd_a_SOURCES = \
     basename.cc \
     dirname.cc \
     dmalloc.cc \
	 ftw.cc \
     mountpoint.cc \
     pathscanner.cc \
     pool.cc \
     pthread_utils.cc \
     receiver.cc \
     textreceiver.cc \
     transmitter.cc \
     undo.cc \
     unescape.cc \
     version.cc \
     xml++.cc 

EXTRA_DIST = libpbd.spec.in \
	     libpbd.pc.in
