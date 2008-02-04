version=`grep const ../version.h | cut -d" " -f7 | sed -e 's/[";]//g'`     
echo Version is $version

./lipo.sh

rm -f SooperLooper-${version}.dmg
hdiutil create -fs HFS+ -volname SooperLooper-${version} -srcfolder macdist SooperLooper-${version}.dmg
