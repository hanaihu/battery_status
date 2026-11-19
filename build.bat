@echo off
rem Release build with MSVC. Produces a single exe with no CRT dependency.
rem Usage: build.bat
setlocal
set "ROOT=%~dp0"
set "OUT=%ROOT%build"
if not exist "%OUT%" mkdir "%OUT%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :nofound
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto :nofound

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 goto :nofound

pushd "%ROOT%"

echo [build] battery_status.exe
cl /nologo /std:c++17 /O2 /MT /W4 /utf-8 /Fo"%OUT%\\" /Fe"%OUT%\battery_status.exe" ^
   src\main.cpp src\hid.cpp src\hidpp.cpp src\bt.cpp src\devices.cpp src\text.cpp ^
   src\providers\logitech.cpp src\providers\bluetooth.cpp src\providers\generic.cpp src\providers\table.cpp ^
   /link setupapi.lib hid.lib
if errorlevel 1 goto :failed

echo [build] frame_test.exe
cl /nologo /std:c++17 /O2 /MT /W4 /utf-8 /Fo"%OUT%\\" /Fe"%OUT%\frame_test.exe" ^
   tests\frame_test.cpp src\hidpp.cpp
if errorlevel 1 goto :failed

popd
echo done: %OUT%\battery_status.exe
exit /b 0

:failed
popd
exit /b 1

:nofound
echo Visual Studio C++ toolchain not found.
echo Install "Desktop development with C++" from the Visual Studio installer.
exit /b 1
