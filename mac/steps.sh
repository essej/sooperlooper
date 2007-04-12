#!/bin/sh

beta="beta3"

cp ../src/sooperlooper macdist/SooperLooper.app/Contents/MacOS/
cp ../src/gui/slgui macdist/SooperLooper.app/Contents/MacOS/

cd macdist/SooperLooper.app/Contents/MacOS

install_name_tool -change /opt/local/lib/libz.1.dylib @executable_path/../Frameworks/libz.1.dylib slgui 
install_name_tool -change /opt/local/lib/libz.1.dylib @executable_path/../Frameworks/libz.1.dylib sooperlooper 
install_name_tool -change /opt/local/lib/libxml2.2.dylib @executable_path/../Frameworks/libxml2.2.dylib sooperlooper 
install_name_tool -change /opt/local/lib/libxml2.2.dylib  @executable_path/../Frameworks/libxml2.2.dylib slgui 
install_name_tool -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib sooperlooper 
install_name_tool -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib slgui 

strip sooperlooper
strip slgui

cd ../Frameworks

install_name_tool -id @executable_path/../Frameworks/libiconv.2.dylib libiconv.2.dylib 
install_name_tool -id @executable_path/../Frameworks/libxml2.2.dylib libxml2.2.dylib 
install_name_tool -id @executable_path/../Frameworks/libz.1.dylib libz.1.dylib 
install_name_tool -change /opt/local/lib/libxml2.2.dylib @executable_path/../Frameworks/libxml2.2.dylib  libxml2.2.dylib 
install_name_tool -change /opt/local/lib/libz.1.dylib @executable_path/../Frameworks/libz.1.dylib  libxml2.2.dylib 
install_name_tool -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib  libxml2.2.dylib 

cd ../../../..

hdiutil create -fs HFS+ -volname SooperLooper-1.1.0-${beta} -srcfolder macdist SooperLooper-1.1.0-${beta}-intel.dmg
