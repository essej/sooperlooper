bin_PROGRAMS =  slgui


AM_CXXFLAGS = @LOSC_CFLAGS@ @SIGCPP_CFLAGS@ @WX_CFLAGS@

slgui_LDADD = @LOSC_LIBS@ @WX_LIBS@ @SIGCPP_LIBS@

slgui_SOURCES = \
	gui_app.cpp \
	gui_frame.cpp



noinst_HEADERS = $(wildcard *.hpp *.h)

EXTRA_DIST =
