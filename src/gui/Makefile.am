bin_PROGRAMS =  slgui


pixmapdir  = $(datadir)/sooperlooper/pixmaps
pixmap_DATA = \
	$(wildcard pixmaps/*.xpm)


DEFS = -DPIXMAPDIR=\""$(pixmapdir)/"\"


AM_CXXFLAGS = @LOSC_CFLAGS@ @SIGCPP_CFLAGS@ @WX_CFLAGS@

slgui_LDADD = @LOSC_LIBS@ @WX_LIBS@ @SIGCPP_LIBS@

slgui_SOURCES = \
	gui_app.cpp \
	gui_frame.cpp \
	pix_button.cpp \
	looper_panel.cpp



noinst_HEADERS = $(wildcard *.hpp *.h)

EXTRA_DIST =
