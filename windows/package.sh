#!/bin/sh

if [ "$#" -lt 1 ]; then
	echo "Usage $0 OUTPUT_DIR"
	exit 1
fi

output_dir=$1

echo Copying files to $output_dir...

bin_dir=$output_dir/bin
lib_dir=$output_dir/lib
usr_share_dir=$output_dir/usr/share
var_lib_dir=$output_dir/var/lib

mkdir -p $bin_dir
mkdir -p $lib_dir
mkdir -p $usr_share_dir
mkdir -p $var_lib_dir

# create a minimal cygwin installation (required to run slgui)

cp -r /lib/gdk-pixbuf-2.0 $lib_dir
cp -r /var/lib/dbus $var_lib_dir
cp -r /usr/share/fonts $usr_share_dir
cp -r /usr/share/glib-2.0 $usr_share_dir
cp -r /usr/share/icons $usr_share_dir
cp -r /usr/share/mime $usr_share_dir

# copy a script for easy start
cp start_with_gui.cmd $output_dir

# copy executables

cp /usr/local/bin/slconsole.exe $bin_dir
cp /usr/local/bin/slregister.exe $bin_dir
cp /usr/local/bin/slgui.exe $bin_dir
cp /usr/local/bin/sooperlooper.exe $bin_dir

# copy libraries

cp /bin/cygGL-1.dll $bin_dir
cp /bin/cygX11-xcb-1.dll $bin_dir
cp /bin/cygxcb-glx-0.dll $bin_dir
cp /bin/cygglapi-0.dll $bin_dir
cp /bin/cygatk-1.0-0.dll $bin_dir
cp /bin/cygatk-bridge-2.0-0.dll $bin_dir
cp /bin/cygatspi-0.dll $bin_dir
cp /bin/cygbz2-1.dll $bin_dir
cp /bin/cygcairo-2.dll $bin_dir
cp /bin/cygcairo-gobject-2.dll $bin_dir
cp /bin/cygdatrie-1.dll $bin_dir
cp /bin/cygdbus-1-3.dll $bin_dir
cp /bin/cygepoxy-0.dll $bin_dir
cp /bin/cygexpat-1.dll $bin_dir
cp /bin/cygffi-6.dll $bin_dir
cp /bin/cygfftw3-3.dll $bin_dir
cp /bin/cygfftw3f-3.dll $bin_dir
cp /bin/cygFLAC-8.dll $bin_dir
cp /bin/cygfontconfig-1.dll $bin_dir
cp /bin/cygfreetype-6.dll $bin_dir
cp /bin/cyggcc_s-seh-1.dll $bin_dir
cp /bin/cyggdk_pixbuf-2.0-0.dll $bin_dir
cp /bin/cyggdk-3-0.dll $bin_dir
cp /bin/cyggio-2.0-0.dll $bin_dir
cp /bin/cygglib-2.0-0.dll $bin_dir
cp /bin/cyggmodule-2.0-0.dll $bin_dir
cp /bin/cyggobject-2.0-0.dll $bin_dir
cp /bin/cyggraphite2-3.dll $bin_dir
cp /bin/cyggsm-1.dll $bin_dir
cp /bin/cyggtk-3-0.dll $bin_dir
cp /bin/cygharfbuzz-0.dll $bin_dir
cp /bin/cygICE-6.dll $bin_dir
cp /bin/cygiconv-2.dll $bin_dir
cp /bin/cygintl-8.dll $bin_dir
cp /bin/cygjbig-2.dll $bin_dir
cp /bin/cygjpeg-8.dll $bin_dir
cp /bin/cyglzma-5.dll $bin_dir
cp /bin/cygmspack-0.dll $bin_dir
cp /bin/cygncursesw-10.dll $bin_dir
cp /bin/cygnotify-4.dll $bin_dir
cp /bin/cygogg-0.dll $bin_dir
cp /bin/cygpango-1.0-0.dll $bin_dir
cp /bin/cygpangocairo-1.0-0.dll $bin_dir
cp /bin/cygpangoft2-1.0-0.dll $bin_dir
cp /bin/cygpcre-1.dll $bin_dir
cp /bin/cygpixman-1-0.dll $bin_dir
cp /bin/cygpng16-16.dll $bin_dir
cp /bin/cygsamplerate-0.dll $bin_dir
cp /bin/cygSDL-1-2-0.dll $bin_dir
cp /bin/cygSDL2-2-0-0.dll $bin_dir
cp /bin/cygsigc-2.0-0.dll $bin_dir
cp /bin/cygSM-6.dll $bin_dir
cp /bin/cygsndfile-1.dll $bin_dir
cp /bin/cygstdc++-6.dll $bin_dir
cp /bin/cygthai-0.dll $bin_dir
cp /bin/cygtiff-6.dll $bin_dir
cp /bin/cyguuid-1.dll $bin_dir
cp /bin/cygvorbis-0.dll $bin_dir
cp /bin/cygvorbisenc-2.dll $bin_dir
cp /bin/cygwin1.dll $bin_dir
cp /bin/cygwx_baseu-3.0-0.dll $bin_dir
cp /bin/cygwx_gtk3u_adv-3.0-0.dll $bin_dir
cp /bin/cygwx_gtk3u_core-3.0-0.dll $bin_dir
cp /bin/cygwx_gtk3u_html-3.0-0.dll $bin_dir
cp /bin/cygX11-6.dll $bin_dir
cp /bin/cygXau-6.dll $bin_dir
cp /bin/cygxcb-1.dll $bin_dir
cp /bin/cygxcb-render-0.dll $bin_dir
cp /bin/cygxcb-shm-0.dll $bin_dir
cp /bin/cygXcomposite-1.dll $bin_dir
cp /bin/cygXcursor-1.dll $bin_dir
cp /bin/cygXdamage-1.dll $bin_dir
cp /bin/cygXdmcp-6.dll $bin_dir
cp /bin/cygXext-6.dll $bin_dir
cp /bin/cygXfixes-3.dll $bin_dir
cp /bin/cygXi-6.dll $bin_dir
cp /bin/cygXinerama-1.dll $bin_dir
cp /bin/cygxml2-2.dll $bin_dir
cp /bin/cygXrandr-2.dll $bin_dir
cp /bin/cygXrender-1.dll $bin_dir
cp /bin/cygz.dll $bin_dir
