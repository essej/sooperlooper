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
AC_PROG_CC
AC_PROG_CXX
if test "$ac_cv_prog_cxx" = "no" ; then
   AC_MSG_ERROR([*** libmidi is C++. You don't appear to have a C++ compiler])
fi

AM_OPT_FLAGS

AC_OBJEXT
AC_LANG_CPLUSPLUS
AC_PROG_RANLIB



AC_ARG_ENABLE(optimize,
     [  --disable-optimize       avoid optimizations to allow for gdb debugging.],
     [ if test "x$enable_optimize" != "xno" ; then CXXFLAGS="$OPT_CXXFLAGS" ; fi ],
     [ CXXFLAGS="$OPT_CXXFLAGS" ])

AC_ARG_ENABLE(profiling,
     [  --enable-profiling       build for profiling using gprof.],
     [ if test "x$enable_profiling" = "xyes" ; then CXXFLAGS="$CXXFLAGS $PROF_FLAGS" ; fi ])

AC_MSG_RESULT([CXXFLAGS is set based on $OPT_CXXFLAGS])

dnl MAC STUFF

AC_MSG_CHECKING([for --with-macosx-sdk])
AC_ARG_WITH(macosx-sdk,    [  --with-macosx-sdk=PATH  use an OS X SDK at PATH], [
        wxUSE_MACOSX_SDK=$withval
        wx_cv_use_macosx_sdk="wxUSE_MACOSX_SDK=$withval"
        #echo "wxUSE_MACOSX_SDK=$wxUSE_MACOSX_SDK" >> ${wx_arg_cache_file}.tmp
    ], [
        # NOTE: Empty string on purpose so that --without-macosx-sdk (thus no)
        # will override --enable-universal_binary's automatic SDK usage.
        wxUSE_MACOSX_SDK=
        #LINE=`grep "^wxUSE_MACOSX_SDK=" ${wx_arg_cache_file}`
        #if test "x$LINE" != x ; then
        #    eval "$LINE"
        #    echo "$LINE" >> ${wx_arg_cache_file}.tmp
        #fi
    ])
AC_MSG_RESULT([$wxUSE_MACOSX_SDK])

AC_MSG_CHECKING([for --with-macosx-version-min])
AC_ARG_WITH(macosx-version-min,    [  --with-macosx-version-min=VER   build binaries which require at least this OS X version], [
        wxUSE_MACOSX_VERSION_MIN=$withval
        wx_cv_use_macosx_version_min="wxUSE_MACOSX_VERSION_MIN=$withval"
        #echo "wxUSE_MACOSX_VERSION_MIN=$wxUSE_MACOSX_VERSION_MIN" >> ${wx_arg_cache_file}.tmp
    ], [
        # default "yes" because when not using an SDK it means the same as no
        # but when using an SDK it means to use the SDK version
        wxUSE_MACOSX_VERSION_MIN=yes
        # See if there is a value in the cache
        #LINE=`grep "^wxUSE_MACOSX_VERSION_MIN=" ${wx_arg_cache_file}`
        #if test "x$LINE" != x ; then
        #    eval "$LINE"
        #    echo "$LINE" >> ${wx_arg_cache_file}.tmp
        #fi
    ])
AC_MSG_RESULT([$wxUSE_MACOSX_VERSION_MIN])


dnl Set up the Mac OS X SDK.  We do this early so configure tests will occur
dnl with the SDK in place.
dnl NOTE: We clobber wxUSE_MACOSX_SDK with the SDK path
if test "x$wxUSE_MACOSX_SDK" = "xno"; then
    wxUSE_MACOSX_SDK=
elif test "x$wxUSE_MACOSX_SDK" = "xyes"; then
    # TODO: Search for most recent SDK and use it.
    wxUSE_MACOSX_SDK="/Developer/SDKs/MacOSX10.4u.sdk"
fi


if test "x$wxUSE_MACOSX_SDK" != "x"; then
    AC_MSG_CHECKING([for SDK directory $wxUSE_MACOSX_SDK])
    if ! test -d "$wxUSE_MACOSX_SDK"; then
        AC_MSG_FAILURE([not found])
    else
        AC_MSG_RESULT([exists])
    fi
    MACOSX_SDK_OPTS="-isysroot $wxUSE_MACOSX_SDK"
    eval "CC=\"$CC $MACOSX_SDK_OPTS\""
    eval "CXX=\"$CXX $MACOSX_SDK_OPTS\""
    eval "LD=\"$LD $MACOSX_SDK_OPTS\""
    retest_macosx_linking=yes
    dnl NOTE: When libtool is used in lieu of AR/RANLIB (i.e. in static mode)
    dnl the syslibroot makes no difference.  We aren't using libtool now but
    dnl if we ever did, be aware that you don't need to worry about it.
fi

dnl Set up the deployment target
dnl No   : Don't specify a min version even if using an SDK
dnl Yes  : Use the version from the SDK if used, otherwise same as no
dnl Param: Use the specified version
if test "x$wxUSE_MACOSX_VERSION_MIN" = "xno"; then
    wxUSE_MACOSX_VERSION_MIN=
elif test "x$wxUSE_MACOSX_VERSION_MIN" = "xyes"; then
    if test "x$wxUSE_MACOSX_SDK" != "x"; then
        AC_MSG_CHECKING([SDK deployment version])
dnl We need to quote the next line where we don't need macros and do need [] in the regex
[
        MACOSX_SDK_PLIST_VERSION_MIN=`defaults read "$wxUSE_MACOSX_SDK/SDKSettings" buildSettings | grep '^ *"\{0,1\}MACOSX_DEPLOYMENT_TARGET"\{0,1\} *= *"\{0,1\}[^"]*"\{0,1\}; *$' | sed 's/^ *"\{0,1\}MACOSX_DEPLOYMENT_TARGET"\{0,1\} *= *"\{0,1\}\([^"]*\)"\{0,1\} *; *$/\1/'`
]
        # If that failed, try again with the new key
        if test "x$MACOSX_SDK_PLIST_VERSION_MIN" == "x"; then
[
            MACOSX_SDK_PLIST_VERSION_MIN=`defaults read "$wxUSE_MACOSX_SDK/SDKSettings" DefaultProperties | grep '^ *"\{0,1\}MACOSX_DEPLOYMENT_TARGET"\{0,1\} *= *"\{0,1\}[^"]*"\{0,1\}; *$' | sed 's/^ *"\{0,1\}MACOSX_DEPLOYMENT_TARGET"\{0,1\} *= *"\{0,1\}\([^"]*\)"\{0,1\} *; *$/\1/'`
]
        fi

        if test "x$MACOSX_SDK_PLIST_VERSION_MIN" != "x"; then
            wxUSE_MACOSX_VERSION_MIN=$MACOSX_SDK_PLIST_VERSION_MIN
            AC_MSG_RESULT([$wxUSE_MACOSX_VERSION_MIN])
        else
            AC_MSG_WARN([Could not determine deployment target from SDKSettings.plist])
            wxUSE_MACOSX_VERSION_MIN=
        fi
    else
        wxUSE_MACOSX_VERSION_MIN=
    fi
fi

if test "x$wxUSE_MACOSX_VERSION_MIN" != "x"; then
    MACOSX_VERSION_MIN_OPTS="-mmacosx-version-min=$wxUSE_MACOSX_VERSION_MIN"
    eval "CC=\"$CC $MACOSX_VERSION_MIN_OPTS\""
    eval "CXX=\"$CXX $MACOSX_VERSION_MIN_OPTS\""
    eval "LD=\"$LD $MACOSX_VERSION_MIN_OPTS\""
    retest_macosx_linking=yes
fi

dnl If either an SDK or a version option was added, make sure that we can
dnl still compile and link both C and C++.  If we didn't do this, then most
dnl of the remaining tests would fail.
if test "x$retest_macosx_linking" = "xyes"; then
    AC_LANG_PUSH(C)
    AC_MSG_CHECKING([if C compiler works with SDK/version options])
    AC_TRY_LINK([],[],[AC_MSG_RESULT([yes])],[AC_MSG_FAILURE([no.  Try a different SDK]); exit 1])
    AC_LANG_POP()

    AC_LANG_PUSH(C++)
    AC_MSG_CHECKING([if C++ compiler works with SDK/version options])
    AC_TRY_LINK([],[],[AC_MSG_RESULT([yes])],[AC_MSG_FAILURE([no.  Try a different SDK]); exit 1])
    AC_LANG_POP()
fi


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

AC_CHECK_FILE(/System/Library/Frameworks/CoreMIDI.framework/Headers/CoreMIDI.h, 
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
