SooperLooper Audio Unit README

INSTALLATION

Just copy the SooperLooperAU.component file to the
Library/Audio/Plug-Ins/Components/ folder beneath your home directory
or your system disk.  If it doesn't show up in your plugin host, you
might need to log out then back in.


USAGE

The AU is a Music Effect style of plugin, which will accept MIDI input
from compatible hosts.  Such hosts include Rax, Logic, Metro, Digital
Performer, Ableton Live, Numerology, and Plogue Bidule and maybe
others.  Note that GarageBand does not support sending MIDI to these
kind of plugins, but you can control it from the GUI.

The AU version, the primary inputs and outputs are the "main" ones,
and will only support up to a stereo i/o.  The plugin does provide
additional buses for dedicated plugin i/o for up to 8 loops.

The Jack/Host syncto setting now lets you use the host's tempo and position
for the normal sync operations.

Although I attempted to embed the current GUI into the plugin's view,
I was ultimately unsuccessful.  Instead, the plugin view provides a
way to start the normal standalone GUI app, whose location will need
to be specified using the provided Browse button.  You must select the
correct SooperLooper.app application on your system.  After you
specify and start a gui the location will be saved on future runs, and
it will start automatically.  The one advantage to the standalone GUI
is that it can't crash your plugin host if it decides to be naughty :)

The state and setup of a plugin instance should be saved and restored
properly by your host.  Loop audio is not part of this state, but all
settings and midi bindings are.


