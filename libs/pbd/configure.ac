dnl Process this file with autoconf to produce a configure script.
AC_INIT(pbd)

AC_CONFIG_AUX_DIR(.)
AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AC_VALIDATE_CACHED_SYSTEM_TUPLE()

LIBPBD_MAJOR_VERSION=2
LIBPBD_MINOR_VERSION=6
LIBPBD_MICRO_VERSION=0

AC_SUBST(LIBPBD_MAJOR_VERSION)
AC_SUBST(LIBPBD_MINOR_VERSION)
AC_SUBST(LIBPBD_MICRO_VERSION)

LIBPBD_VERSION=$LIBPBD_MAJOR_VERSION.$LIBPBD_MINOR_VERSION.${LIBPBD_MICRO_VERSION}
LIBPBD_RELEASE=$LIBPBD_MAJOR_VERSION-$LIBPBD_MINOR_VERSION-${LIBPBD_MICRO_VERSION}

AC_SUBST(LIBPBD_SO_VERSION)
AC_SUBST(LIBPBD_VERSION)
AC_SUBST(LIBPBD_RELEASE)

AM_INIT_AUTOMAKE(libpbd,${LIBPBD_VERSION})

AM_CONFIG_HEADER(config.h)

dnl Checks for programs.
AC_PROG_CXX
if test "$ac_cv_prog_cxx" = "no" ; then
   AC_MSG_ERROR([*** libpbd is C++. You don't appear to have a C++ compiler])
fi

AM_OPT_FLAGS

AC_ARG_ENABLE(optimize,
     [  --enable-optimize       ask the compiler for its best optimizations.],
     [ if test "x$enable_optimize" != "xno" ; then CXXFLAGS="$OPT_CXXFLAGS" ; fi ])

AC_ARG_ENABLE(profiling,
     [  --enable-profiling       build for profiling using gprof.],
     [ if test "x$enable_profiling" = "xyes" ; then CXXFLAGS="$CXXFLAGS $PROF_FLAGS" ; fi ])

AC_OBJEXT
AC_PROG_RANLIB

AC_LANG_CPLUSPLUS

dnl Checks for libraries.

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h sys/time.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
dnl AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_PID_T
dnl AC_TYPE_SIZE_T
AC_HEADER_TIME

dnl Checks for library functions.
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_FUNC_GETMNTENT
AC_CHECK_FUNCS(regcomp select strdup strerror strtol)
AC_CHECK_LIB(pthread, pthread_create, ,[AC_MSG_ERROR([you have no POSIX thread support])])

PKG_CHECK_MODULES(XML, libxml-2.0 >= 2.5)

dnl sigc++
PKG_CHECK_MODULES(SIGCPP, sigc++-1.2 >= 0.14)


AM_BUILD_ENVIRONMENT

CFLAGS="$CFLAGS $XML_CFLAGS $SIGCPP_CFLAGS"
CXXFLAGS="$CXXFLAGS $XML_CFLAGS $SIGCPP_CFLAGS"
LIBS="$LIBS $XML_LIBS $SIGCPP_LIBS"

AC_OUTPUT([Makefile
           version.cc
           pbd/Makefile
])
