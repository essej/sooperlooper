#!/bin/sh

rm -rf macdist/SooperLooperAU.component

if [ -d SooperLooperAU/build/Default/SooperLooperAU.component ] ; then
	cp -Rp SooperLooperAU/build/Default/SooperLooperAU.component macdist/
elif [ -d SooperLooperAU/build/Deployment/SooperLooperAU.component ] ; then
	cp -Rp SooperLooperAU/build/Deployment/SooperLooperAU.component macdist/
elif [ -d SooperLooperAU/build/SooperLooperAU.component ] ; then
	cp -Rp SooperLooperAU/build/SooperLooperAU.component macdist/
else
	echo "no component found!"
	exit 1
fi	


#cd macdist/SooperLooperAU.component/Contents/MacOS

#install_name_tool -change /opt/local/lib/libz.1.dylib @executable_path/../Frameworks/libz.1.dylib SooperLooperAU 
#install_name_tool -change /opt/local/lib/libxml2.2.dylib @executable_path/../Frameworks/libxml2.2.dylib SooperLooperAU 
#install_name_tool -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib SooperLooperAU 


#cd ..
#mkdir -p Frameworks
#cd Frameworks

#cp -f /opt/local/lib/libz.1.dylib .
#cp -f /opt/local/lib/libxml2.2.dylib .
#cp -f /opt/local/lib/libiconv.2.dylib .


#install_name_tool -id @executable_path/../Frameworks/libiconv.2.dylib libiconv.2.dylib 
#install_name_tool -id @executable_path/../Frameworks/libxml2.2.dylib libxml2.2.dylib 
#install_name_tool -id @executable_path/../Frameworks/libz.1.dylib libz.1.dylib 
#install_name_tool -change /opt/local/lib/libxml2.2.dylib @executable_path/../Frameworks/libxml2.2.dylib  libxml2.2.dylib 
#install_name_tool -change /opt/local/lib/libz.1.dylib @executable_path/../Frameworks/libz.1.dylib  libxml2.2.dylib 
#install_name_tool -change /opt/local/lib/libiconv.2.dylib @executable_path/../Frameworks/libiconv.2.dylib  libxml2.2.dylib 

#cd ../../../..

# hdiutil create -fs HFS+ -volname SooperLooper-1.1.0-${beta} -srcfolder macdist SooperLooper-1.1.0-${beta}-intel.dmg
