#!/bin/sh

if [ "x$CC" = "x" ] ; then
    export CC=gcc
fi

pwd=`pwd`

base_libs="pbd midi++"
gui_libs=""
gui_progs=""
nogui_progs=""

prog_dirs="$gui_progs $nogui_progs"
lib_dirs="$base_libs $gui_libs"
xtra_path=
libs_to_process=

while [ $# -gt 0 ]
do

    case $1 in

    -gui*) prog_dirs="$nogui_progs"
           lib_dirs="$base_libs"
	   ;;

    -ksi*) prog_dirs="$gui_progs"
           lib_dirs="$base_libs $gui_libs"
           ;;

    -path=*)
           xtra_path=$xtra_path`expr "x$1" : 'x-path=\(.*\)'`:
           ;;

    *)    libs_to_process="$libs_to_process $1"
          ;;
    esac
    shift

done

PATH=$xtra_path$PATH
export PATH

acmacrodir=$pwd/aclocal
system_macrodir=`aclocal --print-ac-dir`	

acargs=
pkgpath=
auto_lib_dirs=

echo "
----------------------------------------------------------------------
Checking basic compilation tools ...
"

for tool in pkg-config autoconf aclocal automake libtool gettext autopoint
do
    if which $tool >/dev/null 2>&1 ; then
	echo "	$tool: found."
    else
	echo "\
You do not have $tool correctly installed. You cannot build SooperLooper
without this tool."
	exit 1
    fi
done

# check the version of autoconf, because it matters a LOT
# and while i'm here: what the f*ck? we now have to
# write our own configuration tests to see if the configuration
# test system is adequate? gag....
#
# Correct. time for #ifdef AUTO_ME_HARDER ... ?
#

autoconf --version | perl -e '
while(<>) { 
    @x=split(/[ \t\n]+/,$_); 
    $version=$x[3]; $v =~ s/[a-z]+$//; 
    if($version >= 2.52){
        exit 0;
    }else{
        print "\n\tversion $version of autoconf found: sooperlooper requires 2.52 or above.\n";
        exit 1;
    }
}
'      

if [ $? != 0 ] ; then
    exit 1
fi

# Check version of automake.  Equally frustrating as checking 
# the version for autoconf.

automake --version | perl -e '
while(<>) { 
    @x=split(/[ \t\n]+/,$_); 
    $version=$x[3]; $v =~ s/[a-z]+$//; 
    if($version >= 1.7){
        exit 0;
    }else{
        print "\n\tversion $version of automake found: sooperlooper requires 1.7 or above.\n";
        exit 1;
    }
}
'      

if [ $? != 0 ] ; then
    exit 1
fi

echo "
----------------------------------------------------------------------
linking autoconf macros to $acmacrodir ... 
"

for x in $lib_dirs ; do

	pkgpath="${pkgpath}$pwd/libs/$x:"

	# catch any autoconf m4 files that we'll need
	# need to have access to during the autogen step

	macros=`echo $pwd/libs/$x/*.m4`

	if [ x"$macros" != "x$pwd/libs/$x/*.m4" ] ; then
	        for m4 in $macros  
		do
		   bm4=`basename $m4`

		   if [ -f $system_macrodir/$bm4 ] ; then
		      echo "\
----------------------------------------------------------------------

You already have a version of $bm4 installed in $system_macrodir.
This isn't going to work, because aclocal is too stupid to use
an ordered search path. I'm therefore going to ignore the
one in this tree, and rely on the installed one. If this
results in errors, you will have deinstall the library that 
$bm4 is associated with.

If you don't like this policy, I suggest that you write to the
authors of aclocal and suggest that they improve their
program.

----------------------------------------------------------------------
"
		       continue
		   fi

		   if [ $bm4 != aclocal.m4 ] && [ $bm4 != acinclude.m4 ] ; then
			m4copy="$m4copy $m4"
		   fi	       
		done
        fi

	# damn!

	if [ $x = "midi++" ] ; then 
	    libname=midipp
        else 
	    libname=$x
	fi

	auto_lib_dirs="$auto_lib_dirs libs/$x"
done

if [ ! -d $acmacrodir ] ; then 
    mkdir $acmacrodir ; 
else
    rm -rf $acmacrodir/*.m4
fi

for m4 in $m4copy ; do
   ln -s $m4 $acmacrodir
done

export ACLOCAL_FLAGS="$ACLOCAL_FLAGS -I $acmacrodir"

cat > $acmacrodir/optflags.m4 <<EOF
AC_DEFUN([AM_OPT_FLAGS],[
dnl
dnl figure out how best to optimize
dnl 

gcc_major_version=`$CC -dumpversion | sed -e 's/\..*//'`

if test "\$target_cpu" = "powerpc"; then
  AC_DEFINE(POWERPC, 1, "Are we running a ppc CPU?")
  altivecLinux=`cat /proc/cpuinfo | grep -i altivec >/dev/null`
  if test "\$?" = "0"; then
    AC_DEFINE(HAVE_ALTIVEC_LINUX, 1, "Is there Altivec Support ?")
    if test "\$gcc_major_version" = "3"; then
dnl -mcpu=7450 does not reliably work with gcc 3.*
      OPT_FLAGS="-D_REENTRANT -O3 -mcpu=7400 -maltivec -mabi=altivec"
    else
      OPT_FLAGS="-D_REENTRANT -O3 -mcpu=7400"
    fi
  else
    OPT_FLAGS="-D_REENTRANT -O3 -mcpu=750 -mmultiple"
  fi
  OPT_FLAGS="\$OPT_FLAGS -mhard-float -mpowerpc-gfxopt"
elif echo \$target_cpu | grep "i*86" >/dev/null; then
  cat /proc/cpuinfo | grep mmx >/dev/null
  if test \$? = 0; then
    mmx="-mmmx"
  fi
  cat /proc/cpuinfo | grep sse >/dev/null
  if test \$? = 0; then
    sse="-msse -mfpmath=sse"
  fi
  cat /proc/cpuinfo | grep 3dnow >/dev/null
  if test \$? = 0; then
    dreidnow="-m3dnow"
  fi
  AC_DEFINE(x86, 1, "Nope its intel")
  if test "\$target_cpu" = "i586"; then
    OPT_FLAGS="-DREENTRANT -O2 -march=i586 -fomit-frame-pointer -ffast-math -fstrength-reduce"
  elif test "\$target_cpu" = "i686"; then
    OPT_FLAGS="-D_REENTRANT -O2 -march=i686 -fomit-frame-pointer -ffast-math -fstrength-reduce"
    if test "\$gcc_major_version" = "3"; then
      OPT_FLAGS="\$OPT_FLAGS \$mmx \$sse \$dreidnow"
    fi
  else
    OPT_FLAGS="-D_REENTRANT -O2 -fomit-frame-pointer -ffast-math -fstrength-reduce"
  fi
fi

OPT_FLAGS="\$OPT_FLAGS -pipe"

# LARGEFILE_FLAGS="\`getconf LFS_CFLAGS\`"
LARGEFILE_FLAGS="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE"

CXXFLAGS="-g -D_REENTRANT \$LARGEFILE_FLAGS"
if test x"\$GXX" = xyes ; then
   CXXFLAGS="\$CXXFLAGS -Wall"
fi
if test x"\$GXX" = xyes ; then
   OPT_CXXFLAGS="\$OPT_FLAGS -Wall \$LARGEFILE_FLAGS"
fi

PROF_FLAGS="-pg"

dnl 
dnl some link flags to try to speed linking
dnl

CXXFLAGS="\$CXXFLAGS -fno-merge-constants"

])
EOF

cat > $acmacrodir/buildenv.m4 <<EOF

AC_DEFUN([AM_BUILD_ENVIRONMENT],[
top_marker=top_marker
depth=0
while test x\$SOOPERLOOPER_TOP = x -a \$depth -lt 255 ; do
    if test -f \$top_marker ; then
        SOOPERLOOPER_TOP=\`dirname \$top_marker\`
    else 
        top_marker=../\$top_marker
    fi
    depth=\`expr \$depth + 1\`
done

if test \$depth -eq 255 ; then
   AC_MSG_ERROR([ the top of the SooperLooper source tree was not found.])
fi

dnl
dnl convert to absolute path
dnl 

SOOPERLOOPER_TOP=\`cd \$SOOPERLOOPER_TOP; pwd\`

export LIBDIRS="$lib_dirs"

ACLOCAL_FLAGS="-I \$SOOPERLOOPER_TOP/aclocal"
ACLOCAL_AMFLAGS="-I \$SOOPERLOOPER_TOP/aclocal \`if test -d m4 ; then echo -I m4; fi\`"
AC_SUBST(ACLOCAL_FLAGS)
AC_SUBST(ACLOCAL_AMFLAGS)

BASE_LIBS="\$SOOPERLOOPER_TOP/libs/midi++/libmidipp.a \$SOOPERLOOPER_TOP/libs/pbd/libpbd.a"
BASE_INCLUDES="-I\$SOOPERLOOPER_TOP/libs/pbd -I\$SOOPERLOOPER_TOP/libs/midi++"
CFLAGS="\$CFLAGS  -I\$SOOPERLOOPER_TOP/libs/pbd -I\$SOOPERLOOPER_TOP/libs/midi++"
CXXFLAGS="\$CXXFLAGS -I\$SOOPERLOOPER_TOP/libs/pbd -I\$SOOPERLOOPER_TOP/libs/midi++"
LIBS="\$LIBS \$SOOPERLOOPER_TOP/libs/midi++/libmidipp.a \$SOOPERLOOPER_TOP/libs/pbd/libpbd.a"
])
EOF

echo "
----------------------------------------------------------------------
Bootstrapping makefiles etc.

Ignore any warnings about AC_TRY_RUN, AC_PROG_LEX, and AC_DEFINE ...
"

if [ "$libs_to_process" = "" ] ; then
    libs_to_process="libs $auto_lib_dirs $prog_dirs"
fi

for d in $libs_to_process
do
    (cd $d && echo "Building autoconf files for $d ..." && sh ./autogen.sh) || exit 1
done

echo "Building autoconf/automake files for the top level ..."
aclocal $ACLOCAL_FLAGS && autoheader && automake --foreign --add-missing && autoconf


echo "


----------------------------------------------------------------------
Bootstrapping is complete. 

You can now run:

    ./configure
    make
    make install

"


exit 0

