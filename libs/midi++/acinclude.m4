#
# Configure paths for MIDI facilities
#
# Paul Barton-Davis <pbd@op.net> Fall 1999
#
# Note: for ALSA and OSS, there is always the "raw" MIDI device file,
# which can be opened and used without any library support. However, this
# can only be tested for at run time, so we don't try to detect the
# presence of such a device file here.
#
# AM_PATH_MIDI([ACTION-IF-SYSTEMS-DISCOVERED][, ACTION-IF-NO-SYSTEMS])
#

AC_DEFUN(AM_PATH_MIDI,
[

dnl
dnl currently supports:
dnl         ALSA, OSS, IRIX libmd
dnl
dnl after execution:
dnl	ac_cv_have_alsa_midi=[yes|no]
dnl	ac_cv_have_oss_midi=[yes|no]
dnl	ac_cv_have_irix_midi=[yes|no]
dnl
dnl	use_alsa_midi=[yes|no]
dnl	use_oss_midi=[yes|no]
dnl	use_irix_midi=[yes|no]
dnl
dnl     ac_cv_midi_systems=<empty> OR [IRIX [ALSA [OSS]]]
dnl
dnl     MIDI_LIBS  set to -l... value needed to use MIDI system
dnl

use_alsa_midi=yes
use_oss_midi=no
use_solaris_midi=yes
use_irix_midi=yes

ac_cv_midi_systems=
ac_cv_have_alsa_midi=no
ac_cv_have_oss_midi=no
ac_cv_have_irix_midi=no
ac_cv_have_solaris_midi=no

AC_ARG_ENABLE(alsa-midi,
	[  --disable-alsa		Disable ALSA Midi],
	[if test x$enableval != xyes ;  then
	    use_alsa_midi=no
	else 
	    use_alsa_midi=yes
	fi
])

AC_ARG_ENABLE(oss-midi,	
	[  --disable-oss		Disable OSS Midi],
	[if test x$enableval != xyes ;  then
	    use_oss_midi=no
	else
	    use_oss_midi=yes
	fi
])

AC_ARG_ENABLE(irix-midi,
	[  --disable-irix		Disable IRIX Midi],
	[if test x$enableval != yes ;  then
	    use_irix_midi=no
	else
	    use_irix_midi=yes
	fi
])

AC_CHECK_HEADER(alsa/asoundlib.h,
	     ac_cv_have_alsa_midi=yes,ac_cv_have_alsa_midi=no)
AC_CHECK_HEADER(sys/soundcard.h,
	     ac_cv_have_oss_midi=yes,ac_cv_have_oss_midi=no)
AC_CHECK_LIB(md,mdInit,
	     ac_cv_have_irix_midi=yes,ac_cv_have_irix_midi=no)

if test x$use_alsa_midi = xyes -a x$ac_cv_have_alsa_midi = xyes; then
	ac_cv_midi_systems="$ac_cv_midi_systems ALSA"
	MIDI_LIBS="$MIDI_LIBS -lasound"
else
	use_alsa_midi=no
fi

if test x$use_oss_midi = xyes -a x$ac_cv_have_oss_midi = xyes ; then
	ac_cv_midi_systems="$ac_cv_midi_systems OSS"
else
	use_oss_midi=no
fi

if test x$use_irix_midi = xyes -a x$ac_cv_have_irix_midi = xyes ; then
	ac_cv_midi_systems="$ac_cv_midi_systems IRIX"
	MIDI_LIBS="$MIDI_LIBS -lmd"
else
	use_irix_midi=no
fi

if test x"$ac_cv_midi_systems" != x ; then
	ifelse([$1], , :,[$1])
else 
	ifelse([$1], , :,[$1])
fi

AC_SUBST(MIDI_CFLAGS)
AC_SUBST(MIDI_LIBS)
])



