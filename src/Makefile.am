SUBDIRS = . gui


bin_PROGRAMS =  sooperlooper slconsole


AM_CXXFLAGS = @JACK_CFLAGS@ @LOSC_CFLAGS@ @SIGCPP_CFLAGS@ @SNDFILE_CFLAGS@

sooperlooper_SOURCES = \
	sooperlooper.cpp \
	engine.cpp \
	control_osc.cpp \
	looper.cpp \
	plugin.cc \
	event.cpp \
	midi_bridge.cpp \
	alsa_midi_bridge.cpp \
	audio_driver.cpp \
	jack_audio_driver.cpp


sooperlooper_LDADD = @JACK_LIBS@ @LOSC_LIBS@ @SIGCPP_LIBS@ @SNDFILE_LIBS@-lasound


slconsole_SOURCES = slconsole.cpp
slconsole_LDADD = @LOSC_LIBS@ @NCURSES_LIBS@ -lpthread



noinst_HEADERS = $(wildcard *.hpp *.h)

EXTRA_DIST = oxy8.slb midiwizard.slb
