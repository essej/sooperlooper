bin_PROGRAMS = soop sooperlooper osctest

soop_SOURCES = soop.cc plugin.cc

soop_LDADD = @JACK_LIBS@ @GTKMM_LIBS@


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


sooperlooper_LDADD = @JACK_LIBS@ @LOSC_LIBS@ @SIGCPP_LIBS@ -lasound


osctest_SOURCES = osctest.cpp
osctest_LDADD = @LOSC_LIBS@ @NCURSES_LIBS@ -lpthread


noinst_HEADERS = $(wildcard *.hpp *.h)

EXTRA_DIST = oxy8.slb midiwizard.slb
