dnl Process this file with autoconf to produce a configure script.
AC_INIT(src/engine.cpp)

AC_CONFIG_AUX_DIR(.)

SOOP_MAJOR_VERSION=1
SOOP_MINOR_VERSION=0
SOOP_MICRO_VERSION=0

BETA=dev1

AC_SUBST(SOOP_MAJOR_VERSION)
AC_SUBST(SOOP_MINOR_VERSION)
AC_SUBST(SOOP_MICRO_VERSION)

SOOP_SO_VERSION=${SOOP_MAJOR_VERSION}:${SOOP_MINOR_VERSION}:${SOOP_MICRO_VERSION}${BETA}
SOOP_VERSION=$SOOP_MAJOR_VERSION.$SOOP_MINOR_VERSION.${SOOP_MICRO_VERSION}${BETA}
SOOP_RELEASE=$SOOP_MAJOR_VERSION-$SOOP_MINOR_VERSION-${SOOP_MICRO_VERSION}${BETA}

AC_SUBST(SOOP_SO_VERSION)
AC_SUBST(SOOP_VERSION)
AC_SUBST(SOOP_RELEASE)

AM_INIT_AUTOMAKE(sooperlooper,$SOOP_VERSION)
AM_CONFIG_HEADER(config.h)

dnl ====================================================================
dnl you do have C++, right ?
dnl ====================================================================

AC_PROG_CC
AC_PROG_CXX
if test "$ac_cv_prog_cxx" = "no" ; then
   AC_MSG_ERROR([*** SooperLooper is written in C++. You don't appear to have a C++ compiler])
fi

CXXFLAGS="-g -D_REENTRANT"
OPT_CXXFLAGS="-D_REENTRANT -O6 -fomit-frame-pointer -ffast-math -fstrength-reduce -funroll-loops -fmove-all-movables"
if test x"$GXX" = xyes ; then
   CXXFLAGS="$CXXFLAGS -Wall"
fi
if test x"$GXX" = xyes ; then
   OPT_CXXFLAGS="$OPT_CXXFLAGS -Wall"
fi

AC_ARG_ENABLE(optimize,
     [  --disable-optimize       avoid optimizations to allow gdb debugging.],
     [ if test "x$enable_optimize" != "xno" ; then CXXFLAGS="$OPT_CXXFLAGS" ; fi ],
     [ CXXFLAGS="$OPT_CXXFLAGS" ]
)

#AM_PATH_GTK(1.0.0, , [
#   AC_MSG_ERROR([*** soop requires GTK+, but it doesn't appear to be installed])
#])

PKG_CHECK_MODULES(JACK, jack >= 0.80.0)
AC_SUBST(JACK_LIBS)
AC_SUBST(JACK_CFLAGS)


#PKG_CHECK_MODULES(XML, libxml-2.0 >= 2.5)



PKG_CHECK_MODULES(GTKMM, gtkmm-2.0 >= 2.0)
AC_SUBST(GTKMM_LIBS)
AC_SUBST(GTKMM_CFLAGS)

PKG_CHECK_MODULES(LOSC, liblo >= 0.5)
AC_SUBST(LOSC_LIBS)
AC_SUBST(LOSC_CFLAGS)

dnl curses
AC_CHECK_LIB(ncurses,initscr,have_ncurses=yes,[AC_MSG_WARN([******** you don't have the ncurses library correctly installed])])

NCURSES_LIBS=-lncurses
AC_SUBST(NCURSES_LIBS)

dnl sigc++
PKG_CHECK_MODULES(SIGCPP, sigc++-1.2 >= 0.14, have_sigc12=yes, have_sigc12=no)
AC_SUBST(SIGCPP_LIBS)
AC_SUBST(SIGCPP_CFLAGS)


EXTRA_LIBS="$JACK_LIBS $XML_LIBS $GTKMM_LIBS $LOSC_LIBS $SIGCPP_LIBS"

AC_LANG_CPLUSPLUS

CFLAGS="$CFLAGS -DSAMPLE_MEMORY=200 $GTKMM_CFLAGS $JACK_CFLAGS $LOSC_CFLAGS $SIGCPP_CFLAGS"
CXXFLAGS=$CFLAGS
#LIBS="$LIBS $EXTRA_LIBS"

#CFLAGS="$CFLAGS -DSAMPLE_MEMORY=200 -I../ardour/libs/sigc++/sigc++/config  -I../ardour/libs/sigc++ -I../ardour/libs/gtkmm -I../ardour/libs/gtkmm/gdk-- -I../ardour/libs/gtkmm/src $GTK_CFLAGS $JACK_CFLAGS $XML_CFLAGS"
#CXXFLAGS="$CXXFLAGS -DSAMPLE_MEMORY=200 -I../ardour/libs/sigc++/sigc++/config -I../ardour/libs/sigc++ -I../ardour/libs/gtkmm -I../ardour/libs/gtkmm/gdk-- -I../ardour/libs/gtkmm/src $GTK_CFLAGS $JACK_CFLAGS $XML_CFLAGS"
#LIBS="$LIBS -L../ardour/libs/gtkmm/gdk--/gdk--  -L../ardour/libs/sigc++/sigc++ -L../ardour/libs/gtkmm/src/gtk-- -lgtkmm -lgdkmm -lsigc $GTK_LIBS $JACK_LIBS $XML_LIBS $EXTRA_LIBS"

AC_OUTPUT([
Makefile
src/Makefile
version.h
])
