## HOW TO BUILD AND RUN SOOPERLOOPER ON WINDOWS (using Cygwin)



### 1. Required Windows packages

Cygwin 64 bits: https://cygwin.com/setup-x86_64.exe (do not forget to include package wget)

JACK 64 bits: https://github.com/jackaudio/jackaudio.github.com/releases/download/1.9.11/Jack_v1.9.11_64_setup.exe

Xming: https://sourceforge.net/projects/xming/files/latest/download



### 2. Install development toolchain

```bash
wget http://rawgit.com/transcode-open/apt-cyg/master/apt-cyg
install apt-cyg /bin
apt-cyg install gcc-g++ pkg-config make autoconf automake libtool gettext gettext-devel dos2unix
```



### 3. Install SooperLooper dependencies

```bash
apt-cyg install libxml2-devel libiconv-devel libncurses-devel libsndfile-devel libsamplerate-devel libfftw3-devel libwx_baseu3.0-devel libwx_gtk3u3.0-devel libsigc2.0_0
```

Installing libsigc++ from apt-cyg results in version 2.10. SL will fail to compile because header object.h will be missing. Solution is to also install devel package an older libsigc++ version:

```bash
wget http://cygwin.mirror.constant.com//x86_64/release/libsigc2.0/libsigc2.0-devel/libsigc2.0-devel-2.4.1-1.tar.xz
tar -C / -xvf libsigc2.0-devel-2.4.1-1.tar.xz
```



### 4. Build additional dependencies from source

**Linux Audio Developer's Simple Plugin API (LADSPA)** (for Rubber Band)

```bash
wget http://www.ladspa.org/download/ladspa_sdk_1.15.tgz
tar xvzf ladspa_sdk_1.15.tgz
cd ladspa_sdk_1.15/src
make && make install
```

**The Vamp audio analysis plugin system** (for Rubber Band)
TODO: cannot find a way to omit -std=c++89 from Makefile

```bash
wget https://code.soundsoftware.ac.uk/attachments/download/2450/vamp-plugin-sdk-2.8.0.tar.gz
tar xvzf vamp-plugin-sdk-2.8.0.tar.gz
cd vamp-plugin-sdk-2.8.0
./configure
make CXXFLAGS="-I. -fPIC -Wall -Wextra"
make install
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig
```

**Rubber Band Audio Time Stretcher Library** (for SooperLooper)
TODO: cannot find any configure flag to disable JNI support

```bash
wget https://breakfastquay.com/files/releases/rubberband-1.8.2.tar.bz2
tar xvjf rubberband-1.8.2.tar.bz2
cd rubberband-1.8.2
./configure
make
touch lib/librubberband-jni.so
make install
rm lib/librubberband-jni.so
```

**liblo: Lightweight OSC implementation** (for SooperLooper)

```bash
wget http://downloads.sourceforge.net/liblo/liblo-0.29.tar.gz
tar xvzf liblo-0.29.tar.gz
cd liblo-0.29
./configure --disable-shared --disable-examples
make && make install
```



### 5. Build SooperLooper

First allow linking against JACK dll:

```bash
cp -R "/cygdrive/c/Program Files (x86)/Jack/includes/jack" /usr/local/include
cp /cygdrive/c/Windows/libjack64.dll /usr/lib
export JACK_CFLAGS=-I/usr/local/include/jack
export JACK_LIBS=-ljack64
```

Then finally compile:

```bash
git clone https://github.com/essej/sooperlooper
cd sooperlooper
dos2unix *; dos2unix libs/*; dos2unix libs/pbd/*; dos2unix libs/midi++/*
./autogen.sh
./configure --with-wxconfig-path=/usr/bin/wx-config-3.0
make && make install
```



### 6. Test

- Open Jack and Xming

- From Cygwin terminal: `sooperlooper & slgui --display :0 &`

- From cmd: `c:\cygwin64\bin\bash.exe --login -c "sooperlooper & slgui --display :0 &"`



### Appendix

##### 

#### Compiling wxWidgets from source for wxMSW support

The goal is to remove dependency on Xming but it is not currently working.

SooperLooper compiles but then slgui starts without showing any UI, then immediately exits with no apparent signs of a crash. TODO: compile in debug mode?

Some helpful links:
https://forums.wxwidgets.org/viewtopic.php?t=33849
https://forums.wxwidgets.org/viewtopic.php?t=29596

```bash
wget https://github.com/wxWidgets/wxWidgets/releases/download/v3.1.2/wxWidgets-3.1.2.tar.bz2
tar xvjf wxWidgets-3.1.2.tar.bz2
cd wxWidgets-3.1.2
export CPPFLAGS='-w -fpermissive -D__USE_W32_SOCKETS'
export LDFLAGS='-L /lib/w32api/'
./configure --prefix=/usr/local --with-msw
Makefile: add -lGL to EXTRALIBS_OPENGL 
make && make install
```



#### MinGW

There is no easy way to compile SooperLooper using MinGW since some of its libraries (for example pbd) rely on missing POSIX headers



#### Gathering files for distribution

It would be nice to have an easy installer for Windows users, that probably contains the Cygwin runtime and all required binaries.

Here is the list of required binaries:

**SL console**

`c:\cygwin64\usr\local\bin\slconsole.exe`
`c:\cygwin64\bin\cygncursesw-10.dll`

**SooperLooper**

`c:\cygwin64\usr\local\bin\sooperlooper.exe`
`c:\cygwin64\bin\cygwin1.dll`
`c:\cygwin64\bin\cygfftw3-3.dll`
`c:\cygwin64\bin\cygFLAC-8.dll`
`c:\cygwin64\bin\cyggcc_s-seh-1.dll`
`c:\cygwin64\bin\cyggsm-1.dll`
`c:\cygwin64\bin\cygiconv-2.dll`
`c:\cygwin64\bin\cyglzma-5.dll`
`c:\cygwin64\bin\cygogg-0.dll`
`c:\cygwin64\bin\cygsamplerate-0.dll`
`c:\cygwin64\bin\cygsigc-2.0-0.dll`
`c:\cygwin64\bin\cygsndfile-1.dll`
`c:\cygwin64\bin\cygstdc++-6.dll`
`c:\cygwin64\bin\cygvorbis-0.dll`
`c:\cygwin64\bin\cygvorbisenc-2.dll`
`c:\cygwin64\bin\cygwin1.dll`
`c:\cygwin64\bin\cygxml2-2.dll`
`c:\cygwin64\bin\cygz.dll`

**GUI**

`c:\cygwin64\usr\local\bin\slgui.exe`
`c:\cygwin64\bin\cygwx_baseu-3.0-0.dll`
`c:\cygwin64\bin\cygwx_gtk3u_adv-3.0-0.dll`
`c:\cygwin64\bin\cygwx_gtk3u_core-3.0-0.dll`
`c:\cygwin64\bin\cygwx_gtk3u_html-3.0-0.dll`
`c:\cygwin64\bin\cygcairo-2.dll`
`c:\cygwin64\bin\cyggdk-3-0.dll`
`c:\cygwin64\bin\cyggtk-3-0.dll`
`c:\cygwin64\bin\cyggdk_pixbuf-2.0-0.dll`
`c:\cygwin64\bin\cygglib-2.0-0.dll`
`c:\cygwin64\bin\cyggobject-2.0-0.dll`
`c:\cygwin64\bin\cygnotify-4.dll`
`c:\cygwin64\bin\cygjpeg-8.dll`
`c:\cygwin64\bin\cygcairo-2.dll`
`c:\cygwin64\bin\cygpango-1.0-0.dll`
`c:\cygwin64\bin\cygpangocairo-1.0-0.dll`
`c:\cygwin64\bin\cygSDL2-2-0-0.dll`
`c:\cygwin64\bin\cygpng16-16.dll`
`c:\cygwin64\bin\cygtiff-6.dll`
`c:\cygwin64\bin\cygX11-6.dll`
`c:\cygwin64\bin\cygfreetype-6.dll`
`c:\cygwin64\bin\cygfontconfig-1.dll`
`c:\cygwin64\bin\cygmspack-0.dll`
`c:\cygwin64\bin\cygSM-6.dll`
`c:\cygwin64\bin\cygpixman-1-0.dll`
`c:\cygwin64\bin\cygcairo-gobject-2.dll`
`c:\cygwin64\bin\cyggio-2.0-0.dll`
`c:\cygwin64\bin\cyggmodule-2.0-0.dll`
`c:\cygwin64\bin\cygintl-8.dll`
`c:\cygwin64\bin\cygpcre-1.dll`
`c:\cygwin64\bin\cygxcb-1.dll`
`c:\cygwin64\bin\cygxcb-render-0.dll`
`c:\cygwin64\bin\cygxcb-shm-0.dll`
`c:\cygwin64\bin\cygXext-6.dll`
`c:\cygwin64\bin\cygXrender-1.dll`
`c:\cygwin64\bin\cygepoxy-0.dll`
`c:\cygwin64\bin\cygXcursor-1.dll`
`c:\cygwin64\bin\cygXcomposite-1.dll`
`c:\cygwin64\bin\cygXdamage-1.dll`
`c:\cygwin64\bin\cygXi-6.dll`
`c:\cygwin64\bin\cygXfixes-3.dll`
`c:\cygwin64\bin\cygXrandr-2.dll`
`c:\cygwin64\bin\cygXinerama-1.dll`
`c:\cygwin64\bin\cygatk-1.0-0.dll`
`c:\cygwin64\bin\cygthai-0.dll`
`c:\cygwin64\bin\cygatk-bridge-2.0-0.dll`
`c:\cygwin64\bin\cygpangoft2-1.0-0.dll`
`c:\cygwin64\bin\cygICE-6.dll`
`c:\cygwin64\bin\cyguuid-1.dll`
`c:\cygwin64\bin\cygjbig-2.dll`
`c:\cygwin64\bin\cygXdmcp-6.dll`
`c:\cygwin64\bin\cygexpat-1.dll`
`c:\cygwin64\bin\cygXau-6.dll`
`c:\cygwin64\bin\cygbz2-1.dll`
`c:\cygwin64\bin\cygdatrie-1.dll`
`c:\cygwin64\bin\cygharfbuzz-0.dll`
`c:\cygwin64\bin\cygatspi-0.dll`
`c:\cygwin64\bin\cygdbus-1-3.dll`
`c:\cygwin64\bin\cyggraphite2-3.dll`
`c:\cygwin64\bin\cygffi-6.dll`