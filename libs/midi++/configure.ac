AC_INIT(fd_midiport.cc)

AC_CONFIG_AUX_DIR(.)
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AC_VALIDATE_CACHED_SYSTEM_TUPLE()

LIBMIDI_MAJOR_VERSION=1
LIBMIDI_MINOR_VERSION=13
LIBMIDI_MICRO_VERSION=0

LIBMIDIPP_MAJOR_VERSION=${LIBMIDI_MAJOR_VERSION}
LIBMIDIPP_MINOR_VERSION=${LIBMIDI_MINOR_VERSION}
LIBMIDIPP_MICRO_VERSION=${LIBMIDI_MICRO_VERSION}

AC_SUBST(LIBMIDIPP_MAJOR_VERSION)
AC_SUBST(LIBMIDIPP_MINOR_VERSION)
AC_SUBST(LIBMIDIPP_MICRO_VERSION)

BETA=

LIBMIDI_VERSION=$LIBMIDI_MAJOR_VERSION.$LIBMIDI_MINOR_VERSION.${LIBMIDI_MICRO_VERSION}${BETA}
LIBMIDI_RELEASE=$LIBMIDI_MAJOR_VERSION-$LIBMIDI_MINOR_VERSION-${LIBMIDI_MICRO_VERSION}${BETA}

AC_SUBST(LIBMIDI_SO_VERSION)
AC_SUBST(LIBMIDI_VERSION)
AC_SUBST(LIBMIDI_RELEASE)

AM_INIT_AUTOMAKE(libmidi++,${LIBMIDI_VERSION})

AM_CONFIG_HEADER(config.h)

dnl ====================================================================
dnl you do have C++, right ?
dnl ====================================================================

AC_PROG_CXX
if test "$ac_cv_prog_cxx" = "no" ; then
   AC_MSG_ERROR([*** libmidi is C++. You don't appear to have a C++ compiler])
fi

AM_OPT_FLAGS

AC_ARG_ENABLE(optimize,
     [  --disable-optimize       avoid optimizations to allow for gdb debugging.],
     [ if test "x$enable_optimize" != "xno" ; then CXXFLAGS="$OPT_CXXFLAGS" ; fi ],
     [ CXXFLAGS="$OPT_CXXFLAGS" ])

AC_ARG_ENABLE(profiling,
     [  --enable-profiling       build for profiling using gprof.],
     [ if test "x$enable_profiling" = "xyes" ; then CXXFLAGS="$CXXFLAGS $PROF_FLAGS" ; fi ])

AC_MSG_RESULT([CXXFLAGS is set based on $OPT_CXXFLAGS])

AC_OBJEXT
AC_LANG_CPLUSPLUS
AC_PROG_RANLIB

dnl ==================================================
dnl are controller values float, or 16 bit ints ?
dnl ==================================================

use_float_for_controller_values=yes

AC_ARG_ENABLE(float_controllers,
[  --enable-float-controllers   Use floats for controller values [default=yes]],
if test "$enableval" = "no"; then
   use_float_for_controller_values=no
fi
)

if test "$use_float_for_controller_values" = "yes"; then
   CONTROLLER_VALUES_TYPE=float
else
   CONTROLLER_VALUES_TYPE=unsigned short
fi

AC_SUBST(CONTROLLER_VALUES_TYPE)

AUDIO_LIBS=
AC_CHECK_HEADER(alsa/asoundlib.h,
		[use_alsa_midi=yes
		AUDIO_LIBS=-lasound
		AC_DEFINE([WITH_ALSA], 1, [Have ALSA Headers])],
	use_alsa_midi=no)
AM_CONDITIONAL(WITH_ALSA, test x$use_alsa_midi = xyes)

AC_CHECK_HEADER(/System/Library/Frameworks/CoreMIDI.framework/Headers/CoreMIDI.h, 
		[use_coremidi=yes
		AUDIO_LIBS="-framework CoreMIDI"
		AC_DEFINE([WITH_COREMIDI], 1, [Have CoreMIDI Headers])],
	use_coremidi=no)
AM_CONDITIONAL(WITH_COREMIDI, test x$use_coremidi = xyes)

AC_SUBST(AUDIO_LIBS)

dnl sigc++
PKG_CHECK_MODULES(SIGCPP, sigc++-1.2 >= 0.14)


dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
dnl AC_C_CONST
dnl AC_TYPE_SIZE_T

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_CHECK_FUNCS(strdup)

dnl LTLIBOBJS=`echo "$LIBOBJS" | sed 's/\.o/.lo/g'`
dnl AC_SUBST(LTLIBOBJS)

AM_BUILD_ENVIRONMENT

CXXFLAGS="$CXXFLAGS $GUI_INCLUDES $SIGCPP_CFLAGS"

AC_OUTPUT([Makefile
	version.cc
	midi++/Makefile
        libmidi++.pc
	libmidi++.spec
])
