dnl Process this file with autoconf to produce a configure script.
AC_INIT(src/engine.cpp)

AC_CONFIG_AUX_DIR(.)
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AC_VALIDATE_CACHED_SYSTEM_TUPLE()

SOOP_MAJOR_VERSION=1
SOOP_MINOR_VERSION=0
SOOP_MICRO_VERSION=0

BETA=beta29

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


#if test x"$GXX" = xyes ; then
#   CXXFLAGS="$CXXFLAGS -Wall"
#fi

#OPT_CXXFLAGS="-D_REENTRANT -O6 -fomit-frame-pointer -ffast-math -fstrength-reduce -funroll-loops -fmove-all-movables"

ORIG_CXXFLAGS=$CXXFLAGS

AM_OPT_FLAGS

CXXFLAGS=$ORIG_CXXFLAGS

AC_ARG_ENABLE(optimize,
     [  --disable-optimize       avoid optimizations to allow for gdb debugging.],
     [ if test "x$enable_optimize" = "xno" ; then OPT_CXXFLAGS="" ; fi ],
     [ OPT_CXXFLAGS="$OPT_CXXFLAGS" ])


AC_MSG_RESULT([OPT_CXXFLAGS is set based on $OPT_CXXFLAGS])


AC_PROG_RANLIB
AC_LANG_CPLUSPLUS

#[  --enable-optimize       enable optimizations.],
#     [ if test "x$enable_optimize" != "xno" ; then OPT_CXXFLAGS="$OPT_CXXFLAGS" ; fi ],
#     [ OPT_CXXFLAGS="" ]
#)

#AM_PATH_GTK(1.0.0, , [
#   AC_MSG_ERROR([*** soop requires GTK+, but it doesn't appear to be installed])
#])

PKG_CHECK_MODULES(JACK, jack >= 0.80.0, have_jack=yes, have_jack=no)

if test "x$have_jack" != "xno"
then
AC_CHECK_LIB(jack, jack_client_open, 
		   [ AC_DEFINE([HAVE_JACK_CLIENT_OPEN], 1, [Have newer JACK connect call])], 
		   [],
		   [])
fi

AC_SUBST(JACK_LIBS)
AC_SUBST(JACK_CFLAGS)

PKG_CHECK_MODULES(LOSC, liblo >= 0.10)
AC_SUBST(LOSC_LIBS)
AC_SUBST(LOSC_CFLAGS)

dnl curses
AC_CHECK_LIB(ncurses,initscr,have_ncurses=yes,[AC_MSG_WARN([******** you don't have the ncurses library correctly installed])])

NCURSES_LIBS=-lncurses
AC_SUBST(NCURSES_LIBS)

dnl sigc++
PKG_CHECK_MODULES(SIGCPP, sigc++-1.2 >= 0.14)

AC_SUBST(SIGCPP_LIBS)
AC_SUBST(SIGCPP_CFLAGS)

dnl LIBXML
PKG_CHECK_MODULES(XML, libxml-2.0 >= 2.5)
AC_SUBST(XML_CFLAGS)
AC_SUBST(XML_LIBS)

dnl libsndfile
PKG_CHECK_MODULES(SNDFILE, sndfile >= 1.0.2, ac_cv_sndfile=1, ac_cv_sndfile=0)

AC_DEFINE_UNQUOTED([HAVE_SNDFILE],${ac_cv_sndfile},[Set to 1 if you have libsndfile.])
AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)

dnl libsamplerate
PKG_CHECK_MODULES(SAMPLERATE, samplerate >= 0.0.13, ac_cv_samplerate=1, ac_cv_samplerate=0)
AC_DEFINE_UNQUOTED([HAVE_SAMPLERATE],${ac_cv_samplerate},[Set to 1 if you have libsamplerate.])
AC_SUBST(SAMPLERATE_CFLAGS)
AC_SUBST(SAMPLERATE_LIBS)


dnl wxwindows
AC_PATH_PROG(WX_CONFIG1, wxgtk2-2.5-config, no, $PATH:/usr/local/bin )
if [[ "$WX_CONFIG1" = "no" ]] ; then

	AC_PATH_PROG(WX_CONFIG2, wxgtk2-2.4-config, no, $PATH:/usr/local/bin )
	if [[ "$WX_CONFIG2" = "no" ]] ; then

		AC_PATH_PROG(WX_CONFIG3, wxgtk-2.4-config, no, $PATH:/usr/local/bin )
		if [[ "$WX_CONFIG3" = "no" ]] ; then

			AC_PATH_PROG(WX_CONFIG4, wx-config, no, $PATH:/usr/local/bin )
			if [[ "$WX_CONFIG4" = "no" ]] ; then
        			AC_MSG_ERROR("Could not find any wx-config scripts: is wxWindows installed?")
			else	
	   			WX_CONFIG=$WX_CONFIG4
			fi
		else
	   		WX_CONFIG=$WX_CONFIG3
		fi
	else
		WX_CONFIG=$WX_CONFIG2
	fi
else	
   WX_CONFIG=$WX_CONFIG1
fi

WX_LIBS="`$WX_CONFIG --libs`"

if $WX_CONFIG --cxxflags > /dev/null 2>&1 ; then
    WX_CFLAGS="`$WX_CONFIG --cxxflags`"
else
    WX_CFLAGS="`$WX_CONFIG --cflags`"
fi

AC_SUBST(WX_CFLAGS)
AC_SUBST(WX_LIBS)

AUDIO_LIBS=
AC_CHECK_HEADER(alsa/asoundlib.h,
                [use_alsa_midi=yes
                AUDIO_LIBS=-lasound
                AC_DEFINE([WITH_ALSA], 1, [Have ALSA Headers])],
        use_alsa_midi=no)
AM_CONDITIONAL(WITH_ALSA, test x$use_alsa_midi = xyes)

AC_CHECK_HEADER(/System/Library/Frameworks/CoreMIDI.framework/Headers/CoreMIDI.h, 
                [use_coremidi=yes
                AUDIO_LIBS="-framework CoreMIDI -framework CoreFoundation"
                AC_DEFINE([WITH_COREMIDI], 1, [Have CoreMIDI Headers])],
        use_coremidi=no)
AM_CONDITIONAL(WITH_COREMIDI, test x$use_coremidi = xyes)
AC_SUBST(AUDIO_LIBS)

EXTRA_LIBS="$JACK_LIBS $XML_LIBS $GTKMM_LIBS $LOSC_LIBS $SIGCPP_LIBS $AUDIO_LIBS"

AC_LANG_CPLUSPLUS

AC_ARG_ENABLE(debug,
      [  --enable-debug    not optimized and includes debug symbols],
      [ if test "x$enable_debug" != "xno" ; then
          CXXFLAGS="-g -Wall -D_REENTRANT -DDEBUG"
          CFLAGS="$CXXFLAGS"
	fi
      ],
        [
          CXXFLAGS="$CXXFLAGS $OPT_CXXFLAGS"
          CFLAGS="$CFLAGS $OPT_CXXFLAGS"
        ]
)

AM_BUILD_ENVIRONMENT


AC_SUBST(BASE_LIBS)
AC_SUBST(BASE_INCLUDES)


#CFLAGS="$CFLAGS -DSAMPLE_MEMORY=200 $JACK_CFLAGS $LOSC_CFLAGS $SIGCPP_CFLAGS"
#CXXFLAGS=$CFLAGS
#LIBS="$LIBS $EXTRA_LIBS"

#CFLAGS="$CFLAGS -DSAMPLE_MEMORY=200 -I../ardour/libs/sigc++/sigc++/config  -I../ardour/libs/sigc++ -I../ardour/libs/gtkmm -I../ardour/libs/gtkmm/gdk-- -I../ardour/libs/gtkmm/src $GTK_CFLAGS $JACK_CFLAGS $XML_CFLAGS"
#CXXFLAGS="$CXXFLAGS -DSAMPLE_MEMORY=200 -I../ardour/libs/sigc++/sigc++/config -I../ardour/libs/sigc++ -I../ardour/libs/gtkmm -I../ardour/libs/gtkmm/gdk-- -I../ardour/libs/gtkmm/src $GTK_CFLAGS $JACK_CFLAGS $XML_CFLAGS"
#LIBS="$LIBS -L../ardour/libs/gtkmm/gdk--/gdk--  -L../ardour/libs/sigc++/sigc++ -L../ardour/libs/gtkmm/src/gtk-- -lgtkmm -lgdkmm -lsigc $GTK_LIBS $JACK_LIBS $XML_LIBS $EXTRA_LIBS"

AC_CONFIG_SUBDIRS(libs)


AC_OUTPUT([
Makefile
src/Makefile
src/gui/Makefile
version.h
])
