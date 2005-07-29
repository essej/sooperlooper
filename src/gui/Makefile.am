bin_PROGRAMS =  slgui


#pixmapdir  = $(datadir)/sooperlooper/pixmaps
#pixmap_DATA = \
#	$(wildcard pixmaps/*.png)


#DEFS = -DPIXMAPDIR=\""$(pixmapdir)/"\"


AM_CXXFLAGS = -I.. @LOSC_CFLAGS@ @SIGCPP_CFLAGS@ @XML_CFLAGS@ @WX_CFLAGS@

slgui_LDADD = ../libslcore.a  @LOSC_LIBS@ @WX_LIBS@ @SIGCPP_LIBS@ @XML_LIBS@

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
	help_window.cpp



noinst_HEADERS = $(wildcard *.hpp *.h)

EXTRA_DIST = $(wildcard pixmaps/*.xpm)

mac: slgui
	`@WX_CONFIG@ --rezflags` slgui
