#!/bin/sh

version="1.4.1"

cp ../src/sooperlooper macdist/SooperLooper.app/Contents/MacOS/
cp ../src/gui/slgui macdist/SooperLooper.app/Contents/MacOS/

cp ../OSC macdist/OSC.txt

cp Info.plist macdist/SooperLooper.app/Contents/

cd macdist/SooperLooper.app/Contents/MacOS

install_name_tool -change /opt/local/lib/libz.1.dylib @executable_path/../Frameworks/libz.1.dylib slgui 
install_name_tool -change /opt/local/lib/libz.1.dylib @executable_path/../Frameworks/libz.1.dylib sooperlooper 
install_name_tool -change /opt/local/lib/libxml2.2.dylib @executable_path/../Frameworks/libxml2.2.dylib sooperlooper 
install_name_tool -change /opt/local/lib/libxml2.2.dylib  @executable_path/../Frameworks/libxml2.2.dylib slgui 
install_name_tool -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib sooperlooper 
install_name_tool -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib slgui 

strip sooperlooper
strip slgui

cd .. 
mkdir -p Frameworks
cd Frameworks

cp -f /opt/local/lib/libz.1.dylib .
cp -f /opt/local/lib/libxml2.2.dylib .
cp -f /opt/local/lib/libiconv.2.dylib .

install_name_tool -id @executable_path/../Frameworks/libiconv.2.dylib libiconv.2.dylib 
install_name_tool -id @executable_path/../Frameworks/libxml2.2.dylib libxml2.2.dylib 
install_name_tool -id @executable_path/../Frameworks/libz.1.dylib libz.1.dylib 
install_name_tool -change /opt/local/lib/libxml2.2.dylib @executable_path/../Frameworks/libxml2.2.dylib  libxml2.2.dylib 
install_name_tool -change /opt/local/lib/libz.1.dylib @executable_path/../Frameworks/libz.1.dylib  libxml2.2.dylib 
install_name_tool -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib  libxml2.2.dylib 

cd ../../../..

# now tar it up
ARCH=`uname -p`
distdir=sl_macdist_${version}_${ARCH}
ln -sf macdist $distdir
tar chfz ${distdir}.tgz $distdir
rm $distdir

#rm -f SooperLooper-${version}.dmg
#hdiutil create -fs HFS+ -volname SooperLooper-${version} -srcfolder macdist SooperLooper-${version}.dmg
