## HOW TO BUILD AND RUN SOOPERLOOPER ON WINDOWS (using Cygwin)



### 1. Required Windows packages

MSYS64: https://www.msys2.org/

JACK 64 bits: https://github.com/jackaudio/jackaudio.github.com/releases/download/1.9.11/Jack_v1.9.11_64_setup.exe

Xming: https://sourceforge.net/projects/xming/files/latest/download



### 2. Install development toolchain

```bash
winget install --id GCC --exact
winget install --id msys2.msys2
winget install --id GnuWin32.Make
winget install --id GnuWin32.Autoconf
winget install --id GnuWin32.Automake
winget install --id GnuWin32.LibTool
winget install --id GnuWin32.GetText
winget install --id GnuWin32.Dos2Unix
```



### 3. Install SooperLooper dependencies

```bash
winget install --id MSYS2.MSYS2 --exact
pacman -S mingw-w64-x86_64-libxml2 mingw-w64-x86_64-libiconv mingw-w64-x86_64-ncurses mingw-w64-x86_64-libsndfile mingw-w64-x86_64-libsamplerate mingw-w64-x86_64-fftw mingw-w64-x86_64-wxBase mingw-w64-x86_64-wxWidgets mingw-w64-x86_64-libsigc++
```

Installing libsigc++ from apt-cyg results in version 2.10. SL will fail to compile because header object.h will be missing. Solution is to also install devel package an older libsigc++ version:

```bash
winget install 7zip.7zip
Invoke-WebRequest -Uri "https://github.com/libsigcplusplus/libsigcplusplus/releases/download/3.6.0/libsigc++-3.6.0.tar.xz" -OutFile "$env:TEMP\libsigc++-3.6.0.tar.xz"
& "C:\Program Files\7-Zip\7z.exe" x "$env:TEMP\libsigc++-3.6.0.tar.xz" -o"C:\path\to\destination"
```



### 4. Build additional dependencies from source

**Linux Audio Developer's Simple Plugin API (LADSPA)** (for Rubber Band)

```bash
winget install GnuWin32.Make
Invoke-WebRequest -Uri "http://www.ladspa.org/download/ladspa_sdk_1.17.tgz" -OutFile "$env:TEMP\ladspa_sdk_1.17.tgz"
winget install 7zip.7zip
& "C:\Program Files\7-Zip\7z.exe" x "$env:TEMP\ladspa_sdk_1.17.tgz" -o"$env:TEMP\ladspa_sdk"
cd "$env:TEMP\ladspa_sdk\ladspa_sdk_1.17\src"
make
make install
```

**The Vamp audio analysis plugin system** (for Rubber Band)
TODO: cannot find a way to omit -std=c++89 from Makefile

```bash
pacman -S base-devel mingw-w64-x86_64-gcc mingw-w64-x86_64-pkg-config wget
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
winget install --id Microsoft.Powershell --exact
winget install --id 7zip.7zip --exact
curl -o rubberband-4.0.0-gpl-executable-windows.zip https://breakfastquay.com/files/releases/rubberband-4.0.0-gpl-executable-windows.zip
7z x rubberband-4.0.0-gpl-executable-windows.zip -o"rubberband-4.0.0"
```

**liblo: Lightweight OSC implementation** (for SooperLooper)

```bash
winget install --id MSYS2.MSYS2 --exact
pacman -Syu
pacman -S base-devel mingw-w64-x86_64-gcc
wget http://downloads.sourceforge.net/liblo/liblo-0.29.tar.gz
tar xvzf liblo-0.29.tar.gz
cd liblo-0.29
./configure --disable-shared --disable-examples
make
make install
vcpkg install liblo
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
