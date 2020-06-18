#!/bin/bash

version=`grep const ../version.h | cut -d" " -f7 | sed -e 's/[";]//g'`     
echo Version is ${version}$1

#./lipo.sh

if [ -f ./codesign.sh ] ; then
  ./codesign.sh
 # (cd macdist; ../notarize-app.sh SooperLooper.app )
fi

DMGNAME=SooperLooper-64bit-${version}$1.dmg

rm -f SooperLooper-64bit-${version}$1.dmg
hdiutil create -fs HFS+ -volname SooperLooper-64bit-${version}$1 -srcfolder macdist ${DMGNAME}

if [ -f ./codesign_dmg.sh ] ; then
   ./codesign_dmg.sh ${DMGNAME}
   # ./notarize-app.sh --primary-bundle-id=net.essej.slgui ${DMGNAME}
fi
