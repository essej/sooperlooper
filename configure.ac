dnl Process this file with autoconf to produce a configure script.
AC_INIT(src/engine.cpp)

AC_CONFIG_AUX_DIR(.)
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AC_VALIDATE_CACHED_SYSTEM_TUPLE()

SOOP_MAJOR_VERSION=1
SOOP_MINOR_VERSION=7
SOOP_MICRO_VERSION=9

BETA=

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
AC_CONFIG_HEADERS(config.h)

dnl ====================================================================
dnl you do have C++, right ?
dnl ====================================================================

AC_PROG_CC
AC_PROG_CXX
if test "$ac_cv_prog_cxx" = "no" ; then
    AC_MSG_ERROR([*** SooperLooper is written in C++. You don't appear to have a C++ compiler])
    fi


    AC_C_BIGENDIAN

    #if test x"$GXX" = xyes ; then
    #   CXXFLAGS="$CXXFLAGS -Wall"
    #fi

    #OPT_CXXFLAGS="-D_REENTRANT -O6 -fomit-frame-pointer -ffast-math -fstrength-reduce -funroll-loops -fmove-all-movables"



   dnl We may need C++-11 for some versions of sigc
   AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])
   dnl AX_CXX_COMPILE_STDCXX_11


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
    echo JACKLIBS is ${JACK_LIBS}
    AC_CHECK_LIB(jack, jack_client_open, 
                 [ AC_DEFINE([HAVE_JACK_CLIENT_OPEN], 1, [Have newer JACK connect call])], 
                 [],
                 [${JACK_LIBS}])
    fi

    AC_SUBST(JACK_LIBS)
    AC_SUBST(JACK_CFLAGS)

    PKG_CHECK_MODULES(LOSC, liblo >= 0.10)
    AC_SUBST(LOSC_LIBS)
    AC_SUBST(LOSC_CFLAGS)

    dnl curses

    AC_CHECK_LIB(ncurses,initscr,have_ncurses=yes,[AC_MSG_WARN([******** you don't have the ncurses library correctly installed])])
    NCURSES_LIBS="-lncurses"
    AC_CHECK_LIB(tinfo,nodelay, NCURSES_LIBS="-lncurses -ltinfo" ,[AC_MSG_WARN([ncurses doesnt need ltinfo])])
    AC_SUBST(NCURSES_LIBS)

    dnl sigc++
    PKG_CHECK_MODULES(SIGCPP, sigc++-2.0 >= 2.2.10)

    AC_SUBST(SIGCPP_LIBS)
    AC_SUBST(SIGCPP_CFLAGS)

 dnl   PKG_CHECK_MODULES(ICU, icu-i18n, , [AC_MSG_WARN([ICU not found, no problem])])
    AC_SUBST(ICU_CFLAGS)
    AC_SUBST(ICU_LIBS)    

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

    dnl librubberband
    PKG_CHECK_MODULES(RUBBERBAND, rubberband >= 0.0.13, ac_cv_rubberband=1, ac_cv_rubberband=0)
    AC_DEFINE_UNQUOTED([HAVE_RUBBERBAND],${ac_cv_rubberband},[Set to 1 if you have librubberband.])
    AC_SUBST(RUBBERBAND_CFLAGS)
    AC_SUBST(RUBBERBAND_LIBS)

    dnl fftw (needed by librubberband)
    PKG_CHECK_MODULES(FFTW, fftw3 >= 3.0, have_fftw=yes , have_fftw=no )
    AC_SUBST(FFTW_CFLAGS)
    AC_SUBST(FFTW_LIBS)

    dnl wxwindows
    AC_ARG_WITH([gui],
                [AS_HELP_STRING([--without-gui],
                                [disable building the GUI])],
                                [],
                                [with_gui=yes])

    AC_ARG_WITH(wxconfig-path,
                [  --with-wxconfig-path=PATH    full path to wx-config to use])

    if test "x$with_gui" != xno ; then
        if test "x$with_wxconfig_path" != "x"  ; then
            WX_CONFIG=$with_wxconfig_path
        else
            WX_CONFIG=""
        fi

        echo WXCONFIG IS \"$WX_CONFIG\"

        if [[ "$WX_CONFIG" = "" ]] ; then

            AC_PATH_PROG(WX_CONFIG0, wx-config-2.6, no, $PATH:/usr/local/bin )
            if [[ "$WX_CONFIG0" = "no" ]] ; then
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
                else
                    WX_CONFIG=$WX_CONFIG0
                fi
            fi

            WX_LIBS="`$WX_CONFIG --libs`"

            if $WX_CONFIG --cxxflags > /dev/null 2>&1 ; then
                WX_CFLAGS="`$WX_CONFIG --cxxflags`"
            else
                WX_CFLAGS="`$WX_CONFIG --cflags`"
            fi

            AC_SUBST(WX_CFLAGS)
            AC_SUBST(WX_LIBS)
            AC_SUBST(WX_CONFIG)
            SL_DIRS=". gui"
        else
            SL_DIRS="."
        fi
        AC_SUBST(SL_DIRS)

        AUDIO_LIBS=
        AC_CHECK_HEADER(alsa/asoundlib.h,
                        [use_alsa_midi=yes
                         AUDIO_LIBS=-lasound
                         AC_DEFINE([WITH_ALSA], 1, [Have ALSA Headers])],
                         use_alsa_midi=no)
        AM_CONDITIONAL(WITH_ALSA, test x$use_alsa_midi = xyes)

      dnl  AC_CHECK_FILE(/System/Library/Frameworks/CoreMIDI.framework/Headers/CoreMIDI.h, 
	AC_CHECK_FILE(/System/Library/Frameworks/CoreMIDI.framework,       
                      [use_coremidi=yes
                       AUDIO_LIBS="-framework CoreMIDI -framework CoreFoundation -framework CoreServices -framework ApplicationServices -framework Accelerate"
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

        #CFLAGS="$CFLAGS -DSAMPLE_MEMORY=200 -I../ardour/libs/sigc++/sigc++/config  -I../ardour/libs/sigc++ -I../ardour/libs/gtkmm -I../ardour/libs/gtkmm/gdk-- -I../ardour/libs/gtkmm/src $GTK_CFLAGS $JACK_CFLAGS $XML_CFLAGS $ICU_CFLAGS"
        #CXXFLAGS="$CXXFLAGS -DSAMPLE_MEMORY=200 -I../ardour/libs/sigc++/sigc++/config -I../ardour/libs/sigc++ -I../ardour/libs/gtkmm -I../ardour/libs/gtkmm/gdk-- -I../ardour/libs/gtkmm/src $GTK_CFLAGS $JACK_CFLAGS $XML_CFLAGS $ICU_CFLAGS"
        #LIBS="$LIBS -L../ardour/libs/gtkmm/gdk--/gdk--  -L../ardour/libs/sigc++/sigc++ -L../ardour/libs/gtkmm/src/gtk-- -lgtkmm -lgdkmm -lsigc $GTK_LIBS $JACK_LIBS $XML_LIBS $ICU_LIBS $EXTRA_LIBS"

        AC_CONFIG_SUBDIRS(libs)


        AC_OUTPUT([
                   Makefile
                   src/Makefile
                   src/gui/Makefile
                   version.h
                   ])
