@echo off
setlocal

rem If this is an ordinary Command Prompt or PowerShell session, discover the
rem newest Visual Studio installation and load its x64 compiler environment.
rem SETLOCAL ensures those environment changes disappear when this script exits.
where cl >nul 2>nul
if errorlevel 1 call :load_msvc
if errorlevel 1 exit /b 1

if not exist build mkdir build

set COMMON=/nologo /std:c11 /utf-8 /W4 /WX- /O2 /MT /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS /Isrc /Fo:build\
set LIBS=user32.lib gdi32.lib comctl32.lib shell32.lib uxtheme.lib

echo Compiling Taskboard resources...
rc /nologo /fo build\taskboard.res resources\taskboard.rc
if errorlevel 1 exit /b 1

echo Building taskboard.exe...
cl %COMMON% /Fe:build\taskboard.exe src\taskboard.c src\task_model.c src\agent_protocol.c src\ui_agent_panel.c src\ui_theme.c build\taskboard.res /link /SUBSYSTEM:WINDOWS %LIBS%
if errorlevel 1 exit /b 1

echo Building taskagent_stub.exe...
cl %COMMON% /Fe:build\taskagent_stub.exe src\taskagent_stub.c src\agent_protocol.c /link /SUBSYSTEM:CONSOLE
if errorlevel 1 exit /b 1

echo.
echo Build complete: build\taskboard.exe
endlocal
exit /b 0

:load_msvc
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: Visual Studio Installer's vswhere.exe was not found.
    echo Install Visual Studio or Build Tools with "Desktop development with C++".
    exit /b 1
)

set "VSINSTALL="
for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"

if not defined VSINSTALL (
    echo ERROR: No Visual Studio installation with the x64 C++ tools was found.
    echo Add the "Desktop development with C++" workload in Visual Studio Installer.
    exit /b 1
)

if not exist "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" (
    echo ERROR: VsDevCmd.bat was not found under:
    echo   %VSINSTALL%
    exit /b 1
)

echo Loading the Visual Studio x64 build environment...
call "%VSINSTALL%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 (
    echo ERROR: Visual Studio's build environment could not be initialized.
    exit /b 1
)

where cl >nul 2>nul
if errorlevel 1 (
    echo ERROR: Visual Studio initialized, but cl.exe is still unavailable.
    exit /b 1
)
exit /b 0
