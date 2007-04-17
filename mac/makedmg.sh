version=1.1.0

rm -f SooperLooper-${version}.dmg
hdiutil create -fs HFS+ -volname SooperLooper-${version} -srcfolder macdist SooperLooper-${version}.dmg
