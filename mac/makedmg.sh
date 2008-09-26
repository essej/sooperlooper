version=`grep const ../version.h | cut -d" " -f7 | sed -e 's/[";]//g'`     
echo Version is ${version}$1

./lipo.sh

rm -f SooperLooper-${version}$1.dmg
hdiutil create -fs HFS+ -volname SooperLooper-${version}$1 -srcfolder macdist SooperLooper-${version}$1.dmg
