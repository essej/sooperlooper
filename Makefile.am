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

MACDISTDIR = macdist/SooperLooper-@SOOP_VERSION@

mac: all
	-rm -rf macdist
	-mkdir -p macdist
	-mkdir -p $(MACDISTDIR)
	-mkdir -p $(MACDISTDIR)/SooperLooper.app
	-mkdir -p $(MACDISTDIR)/Presets
	-mkdir -p $(MACDISTDIR)/SooperLooper.app/Contents
	-mkdir -p $(MACDISTDIR)/SooperLooper.app/Contents/MacOS
	-mkdir -p $(MACDISTDIR)/SooperLooper.app/Contents/Resources
	`@WX_CONFIG@ --rezflags` src/gui/slgui
	strip src/gui/slgui
	strip src/sooperlooper
	cp -f src/gui/slgui $(MACDISTDIR)/SooperLooper.app/Contents/MacOS/
	cp -f src/sooperlooper  $(MACDISTDIR)/SooperLooper.app/Contents/MacOS/
	cp -f mac/Info.plist  $(MACDISTDIR)/SooperLooper.app/Contents/
	cp -f mac/PkgInfo  $(MACDISTDIR)/SooperLooper.app/Contents/
	cp -f mac/slgui.icns  $(MACDISTDIR)/SooperLooper.app/Contents/Resources/
	cp -f mac/README_AudioUnit.txt  $(MACDISTDIR)/
	cp -rf mac/SooperLooperAU/build/SooperLooperAU.component $(MACDISTDIR)/
	cp -f src/*.slb  $(MACDISTDIR)/Presets/
	cp -f README  $(MACDISTDIR)/README.txt
	cp -f COPYING  $(MACDISTDIR)/COPYING.txt
	cp -f OSC  $(MACDISTDIR)/OSC.txt
	rm -f SooperLooper-@SOOP_VERSION@.dmg
	hdiutil create -fs HFS+ -volname SooperLooper-@SOOP_VERSION@  -srcfolder macdist  SooperLooper-@SOOP_VERSION@.dmg
