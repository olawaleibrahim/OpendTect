@echo off
SETLOCAL
set PATH=%PATH%;C:\appman\apps\ninja\

SET MYPATH=%~p0

buildconsole /COMMAND="ninja -j70" /PROFILE=%MYPATH%increadibuild_profile.xml

ENDLOCAL
