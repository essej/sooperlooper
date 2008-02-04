#!/bin/bash

version=`grep const ../version.h | cut -d" " -f7 | sed -e 's/[";]//g'`

ppc_distdir=sl_macdist_${version}_powerpc
i386_distdir=sl_macdist_${version}_i386

if [ -f ${ppc_distdir}.tgz ] ; then
  tar xfz ${ppc_distdir}.tgz
fi

if [ -f ${i386_distdir}.tgz ] ; then
  tar xfz ${i386_distdir}.tgz
fi



lipo -create -output macdist/SooperLooper.app/Contents/MacOS/sooperlooper \
	 ${i386_distdir}/SooperLooper.app/Contents/MacOS/sooperlooper \
	${ppc_distdir}/SooperLooper.app/Contents/MacOS/sooperlooper	 

lipo -create  -output macdist/SooperLooper.app/Contents/MacOS/slgui \
	 ${i386_distdir}/SooperLooper.app/Contents/MacOS/slgui \
	${ppc_distdir}/SooperLooper.app/Contents/MacOS/slgui	 

lipo -create  -output macdist/SooperLooper.app/Contents/Frameworks/libxml2.2.dylib \
	 ${i386_distdir}/SooperLooper.app/Contents/Frameworks/libxml2.2.dylib \
	${ppc_distdir}/SooperLooper.app/Contents/Frameworks/libxml2.2.dylib	 

lipo -create  -output macdist/SooperLooper.app/Contents/Frameworks/libiconv.2.dylib \
	 ${i386_distdir}/SooperLooper.app/Contents/Frameworks/libiconv.2.dylib \
	${ppc_distdir}/SooperLooper.app/Contents/Frameworks/libiconv.2.dylib	 

lipo -create  -output macdist/SooperLooper.app/Contents/Frameworks/libz.1.dylib \
	 ${i386_distdir}/SooperLooper.app/Contents/Frameworks/libz.1.dylib \
	${ppc_distdir}/SooperLooper.app/Contents/Frameworks/libz.1.dylib	 



lipo -create  -output macdist/SooperLooperAU.component/Contents/MacOS/SooperLooperAU \
	 ${i386_distdir}/SooperLooperAU.component/Contents/MacOS/SooperLooperAU \
	${ppc_distdir}/SooperLooperAU.component/Contents/MacOS/SooperLooperAU	 
  
  