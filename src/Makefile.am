SUBDIRS = . gui

bin_PROGRAMS =  sooperlooper slconsole

slpresetdir  = $(datadir)/sooperlooper/presets
slpreset_DATA =  midiwizard.slb oxy8.slb edp4.slb bcf2000.slb

AM_CXXFLAGS =  @JACK_CFLAGS@ @LOSC_CFLAGS@ @SIGCPP_CFLAGS@ @SNDFILE_CFLAGS@ @SAMPLERATE_CFLAGS@

#SYSDEP_SRCS=
#if WITH_ALSA
#SYSDEP_SRCS += alsa_midi_bridge.cpp
#endif

#if WITH_COREMIDI
#SYSDEP_SRCS += coremidi_bridge.cpp
#endif

noinst_LIBRARIES           = libslcore.a

libslcore_a_SOURCES      = \
	engine.cpp \
	control_osc.cpp \
	looper.cpp \
	plugin.cc \
	event.cpp \
	midi_bridge.cpp \
	midi_bind.cpp \
	audio_driver.cpp \
	jack_audio_driver.cpp \
	command_map.cpp \
	$(SYSDEP_SRCS)

sooperlooper_SOURCES = \
	sooperlooper.cpp

sooperlooper_LDADD =  libslcore.a @JACK_LIBS@ @LOSC_LIBS@ @SIGCPP_LIBS@ @SNDFILE_LIBS@ @SAMPLERATE_LIBS@ @AUDIO_LIBS@

slconsole_SOURCES = slconsole.cpp
slconsole_LDADD = @LOSC_LIBS@ @NCURSES_LIBS@ -lpthread

noinst_HEADERS = $(wildcard *.hpp *.h)

EXTRA_DIST = oxy8.slb midiwizard.slb bcf2000.slb edp4.slb
