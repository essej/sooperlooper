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
	-mkdir -p SooperLooper.app
	-mkdir -p SooperLooper.app/Contents
	-mkdir -p SooperLooper.app/Contents/MacOS
	-mkdir -p SooperLooper.app/Contents/Resources
	`wx-config --rezflags` src/gui/slgui
	strip src/gui/slgui
	strip src/sooperlooper
	cp src/gui/slgui SooperLooper.app/Contents/MacOS
	cp src/sooperlooper SooperLooper.app/Contents/MacOS
