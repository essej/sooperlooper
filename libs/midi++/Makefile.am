AUTOMAKE_OPTIONS=1.7
MAINTAINERCLEANFILES = autom4te.cache Makefile.in aclocal.m4 configure config.h.in \
                          stamp-h.in libmidi++.spec \
			  config.guess mkinstalldirs \
                          missing install-sh config.sub ltconfig \
                          ltmain.sh config.log config.status \
			  depcomp

SUBDIRS                 = midi++
INCLUDES                = -I$(top_srcdir)

SYSDEP_SRCS=
if WITH_ALSA
SYSDEP_SRCS += alsa_sequencer_midiport.cc
endif

if WITH_COREMIDI
SYSDEP_SRCS += coremidi_midiport.cc
endif

noinst_LIBRARIES           = libmidipp.a

libmidipp_a_SOURCES      = \
   fd_midiport.cc \
   fifomidi.cc \
   midi.cc \
   midichannel.cc \
   midicontrollable.cc \
   midifactory.cc \
   midimanager.cc \
   midiparser.cc \
   midiport.cc \
   mmc.cc \
   mtc.cc \
   port_request.cc \
   version.cc \
   $(SYSDEP_SRCS)

XTRA_SRC = alsa_sequencer_midiport.cc coremidi_midiport.cc
EXTRA_DIST = $(XTRA_SRC) libmidi++.spec.in \
	     libmidi++.pc.in autogen.sh

#bin_DIR = .
#noinst_PROGRAMS = mmc miditrace

#mmc_SOURCES = mmctest.cc
#mmc_LDFLAGS = `midi++-config --libs`

#miditrace_SOURCES = miditrace.cc
#miditrace_LDFLAGS = `midi++-config --libs`
