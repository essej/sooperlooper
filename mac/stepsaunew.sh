#!/bin/sh

# this is the old one on my system
#xcodepath=/Developer/usr/bin/xcodebuild
#xcodesdk="-sdk macosx10.5"

rm -rf macdist/SooperLooperAU.component
rm -rf macdist/SooperLooperAU64.component

#xcodepath=/Developer/usr/bin/xcodebuild


cd SooperLooperAU

#${xcodepath} ${xcodesdk} -configuration Deployment
#xcodebuild -configuration Deployment

#xcodebuild -configuration Deployment -scheme "SooperLooperAU 32" -derivedDataPath .
xcodebuild -configuration Deployment -scheme "SooperLooperAU 64" -destination "generic/platform=macOS" -derivedDataPath .


cd ..

if [ -d SooperLooperAU/Build/Products/Deployment/SooperLooperAU64.component ] ; then
	#echo cp -Rp SooperLooperAU/Build/Products/Deployment/SooperLooperAU.component macdist/
	#cp -Rp SooperLooperAU/Build/Products/Deployment/SooperLooperAU.component macdist/	

	echo cp -Rp SooperLooperAU/Build/Products/Deployment/SooperLooperAU64.component macdist/
	cp -Rp SooperLooperAU/Build/Products/Deployment/SooperLooperAU64.component macdist/	

#if [ -d SooperLooperAU/build/Deployment/SooperLooperAU.component ] ; then
#	echo cp -Rp SooperLooperAU/build/Deployment/SooperLooperAU.component macdist/
#	cp -Rp SooperLooperAU/build/Deployment/SooperLooperAU.component macdist/	
#elif [ -d SooperLooperAU/build/Default/SooperLooperAU.component ] ; then
#	echo cp -Rp SooperLooperAU/build/Default/SooperLooperAU.component macdist/
#	cp -Rp SooperLooperAU/build/Default/SooperLooperAU.component macdist/
#elif [ -d SooperLooperAU/build/SooperLooperAU.component ] ; then
#	echo cp -Rp SooperLooperAU/build/SooperLooperAU.component macdist/
#	cp -Rp SooperLooperAU/build/SooperLooperAU.component macdist/
else
	echo "no component found!"
	exit 1
fi	


#dylibbundler -od -b -x ./macdist/SooperLooperAU64.component/Contents/MacOS/SooperLooperAU64 -d ./macdist/SooperLooperAU64.component/Contents/libs/

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
