cd %~dp0

set DISPLAY=:0

start "" "%ProgramFiles(x86)%\Xming\Xming.exe" :0 -clipboard -multiwindow
start /min bin/sooperlooper.exe
start /min bin/slgui.exe
