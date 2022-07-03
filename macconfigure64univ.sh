#!/bin/bash

#export XML_CFLAGS="-I/usr/include/libxml2"
#export XML_LIBS="-L/usr/lib -lxml2"

STATDEV=$HOME/devstatic

WXBUILD=$HOME/src/wxWidgets-3.1.7/build64univ
#WXBUILD=$HOME/src/wxWidgets-3.1.3/build64
#WXBUILD=$HOME/src/wxWidgets-3.1.3/build64d

ARCHFLAGS="-arch x86_64 -arch arm64"

export PKG_CONFIG_PATH="$HOME/devstatic/lib/pkgconfig:/usr/local/lib/pkgconfig"
export CFLAGS="${ARCHFLAGS}"
export CXXFLAGS="${ARCHFLAGS}"
#export LDFLAGS=""
export LD=`$WXBUILD/wx-config --ld`
export CXX="`$WXBUILD/wx-config --cxx` ${ARCHFLAGS}"
export PKG_CONFIG="pkg-config --static"

#CXXFLAGS="-stdlib=libc++ -std=c++11"  PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/opt/local/lib/pkgconfig:/usr/lib/pkgconfig ./configure --prefix=/usr/local --with-wxconfig-path=/Users/jesse/src/wxWidgets-3.1.1/build64/wx-config

./configure --prefix=${STATDEV} --with-wxconfig-path=$WXBUILD/wx-config 

