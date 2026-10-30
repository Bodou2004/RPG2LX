@echo off
rem Windows development entry point. The PortMaster entry point is RPG2LX.sh.
set "RPG2LX_PROJECT=%~dp0"
set "RPG2LX_BINARY=%RPG2LX_PROJECT%build\native-release\rpg2lx-sdl.exe"
if not exist "%RPG2LX_BINARY%" set "RPG2LX_BINARY=%RPG2LX_PROJECT%build\cmake-release\rpg2lx.exe"
if not exist "%RPG2LX_BINARY%" set "RPG2LX_BINARY=%RPG2LX_PROJECT%build\cmake-debug\rpg2lx.exe"
if not exist "%RPG2LX_BINARY%" (
    echo RPG2LX must be built with SDL2 first. See docs\LAUNCHER.md.
    exit /b 1
)
if not "%~1"=="" if exist "%~1" goto direct_project
"%RPG2LX_BINARY%" launcher --home "%RPG2LX_PROJECT%." %*
exit /b %ERRORLEVEL%

:direct_project
"%RPG2LX_BINARY%" run "%~1" --home "%RPG2LX_PROJECT%."
exit /b %ERRORLEVEL%
