bin_PROGRAMS =  slgui


#pixmapdir  = $(datadir)/sooperlooper/pixmaps
#pixmap_DATA = \
#	$(wildcard pixmaps/*.png)


#DEFS = -DPIXMAPDIR=\""$(pixmapdir)/"\"


AM_CXXFLAGS = -I.. @LOSC_CFLAGS@ @SIGCPP_CFLAGS@ @XML_CFLAGS@ @ICU_CFLAGS@  @WX_CFLAGS@

slgui_LDADD = ../libslcore.a @BASE_LIBS@ @LOSC_LIBS@ @WX_LIBS@ @SIGCPP_LIBS@ @XML_LIBS@

slgui_SOURCES = \
	gui_app.cpp \
	main_panel.cpp \
	app_frame.cpp \
	pix_button.cpp \
	looper_panel.cpp \
	loop_control.cpp \
	time_panel.cpp \
	slider_bar.cpp \
	choice_box.cpp \
	spin_box.cpp \
	keyboard_target.cpp \
	keys_panel.cpp \
	check_box.cpp \
	midi_bind_panel.cpp \
	config_panel.cpp \
	prefs_dialog.cpp \
	help_window.cpp \
	latency_panel.cpp



noinst_HEADERS = $(wildcard *.hpp *.h)

#EXTRA_DIST = $(wildcard pixmaps/*.xpm)
EXTRA_DIST = pixmaps

# 	`@WX_CONFIG@ --rezflags` slgui
mac: slgui
	/Developer/Tools/Rez -d __DARWIN__ -t APPL Carbon.r -o slgui

