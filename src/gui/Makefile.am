bin_PROGRAMS =  slgui


pixmapdir  = $(datadir)/sooperlooper/pixmaps
pixmap_DATA = \
	$(wildcard pixmaps/*.png)


DEFS = -DPIXMAPDIR=\""$(pixmapdir)/"\"


AM_CXXFLAGS = @LOSC_CFLAGS@ @SIGCPP_CFLAGS@ @WX_CFLAGS@

slgui_LDADD = @LOSC_LIBS@ @WX_LIBS@ @SIGCPP_LIBS@

slgui_SOURCES = \
	gui_app.cpp \
	gui_frame.cpp \
	pix_button.cpp \
	looper_panel.cpp \
	loop_control.cpp \
	time_panel.cpp \
	slider_bar.cpp



noinst_HEADERS = $(wildcard *.hpp *.h)

EXTRA_DIST = $(wildcard pixmaps/*.png)
