version=`grep const ../version.h | cut -d" " -f7 | sed -e 's/[";]//g'`     
echo Version is ${version}$1

#./lipo.sh

rm -f SooperLooper-64bit-${version}$1.dmg
hdiutil create -fs HFS+ -volname SooperLooper-64bit-${version}$1 -srcfolder macdist SooperLooper-64bit-${version}$1.dmg
