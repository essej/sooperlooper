# Copyright (C) 2004 Jesse Chappell <jesse@essej.net>
#  
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without 
# modifications, as long as this notice is preserved.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

EXTRA_DIST = autogen.sh configure OSC top_marker
SUBDIRS =  libs src

mac: all
	-mkdir -p SooperLooper
	-mkdir -p SooperLooper/SooperLooper.app
	-mkdir -p SooperLooper/Presets
	-mkdir -p SooperLooper/SooperLooper.app/Contents
	-mkdir -p SooperLooper/SooperLooper.app/Contents/MacOS
	-mkdir -p SooperLooper/SooperLooper.app/Contents/Resources
	`wx-config --rezflags` src/gui/slgui
	strip src/gui/slgui
	strip src/sooperlooper
	cp -f src/gui/slgui SooperLooper/SooperLooper.app/Contents/MacOS/
	cp -f src/sooperlooper SooperLooper/SooperLooper.app/Contents/MacOS/
	cp -f mac/Info.plist SooperLooper/SooperLooper.app/Contents/
	cp -f mac/PkgInfo SooperLooper/SooperLooper.app/Contents/
	cp -f mac/slgui.icns SooperLooper/SooperLooper.app/Contents/Resources/
	cp -f src/*.slb SooperLooper/Presets/
	cp -f README SooperLooper/README.txt
	cp -f COPYING SooperLooper/COPYING.txt
	rm -f SooperLooper-@SOOP_VERSION@.dmg
	hdiutil create -fs HFS+ -volname SooperLooper  -srcfolder SooperLooper  SooperLooper-@SOOP_VERSION@.dmg
