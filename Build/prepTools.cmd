@echo off
REM echo Current dir: %cd%
REM echo folder: %~dp0
REM echo arch: %1

call "%~dp0.\buildVariables.cmd" %*
REM echo %*
REM IF "%~3" == "" ( set "qt6_version=6.3.1" ) ELSE ( set "qt6_version=%~3" )
REM IF "%~2" == "" ( set "qt_version=5.15.16" ) ELSE ( set "qt_version=%~2" )

set vs_install=C:\Program Files\Microsoft Visual Studio\2022\Enterprise
echo vs_install: %vs_install%

IF "%~1" == "Win32" (
  set build_arch=Win32
  
  echo qt_version: %qt_version%
  set qt_build=msvc2019
  set qt_path=%~dp0..\..\Qt\%qt_version%
 
  set qt_params= 
  
  call "%vs_install%\VC\Auxiliary\Build\vcvars32.bat"
)
IF "%~1" == "x64" (
  set build_arch=x64

  echo qt_version: %qt_version%
  set qt_build=msvc2019_64
  set qt_path=%~dp0..\..\Qt\%qt_version%
    
  set qt_params= 
  
  call "%vs_install%\VC\Auxiliary\Build\vcvars64.bat"
)
IF "%~1" == "ARM64" (
  set build_arch=ARM64
  
  echo qt6_version: %qt6_version%
  set qt_build=msvc2019_64
  set qt_path=%~dp0..\..\Qt\%qt6_version%
  
  REM  set qt_params=-qtconf "%~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\target_qt.conf"
  
  REM type %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\target_qt.conf
  
  REM
  REM The target_qt.conf as provided by the windows-2019 github action runner
  REM is non functional, hence we create our own working edition here.
  REM
  
  echo [DevicePaths] > %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf
  echo Prefix=C:/Qt/Qt-%qt6_version% >> %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf
  echo [Paths] >> %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf
  echo Prefix=../ >> %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf
  echo HostPrefix=../../msvc2019_64 >> %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf
  echo HostData=../msvc2019_arm64 >> %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf
  echo Sysroot= >> %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf
  echo SysrootifyPrefix=false >> %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf
  echo TargetSpec=win32-arm64-msvc >> %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf
  echo HostSpec=win32-msvc >> %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf
  echo Documentation=../../Docs/Qt-%qt6_version% >> %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf
  echo Examples=../../Examples/Qt-%qt6_version% >> %~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf
  
  set qt_params=-qtconf "%~dp0..\..\Qt\%qt6_version%\msvc2019_arm64\bin\my_target_qt.conf"
  
  REM  set VSCMD_DEBUG=3
  call "%vs_install%\VC\Auxiliary\Build\vcvarsamd64_arm64.bat"
)

set qt_path=%qt_path%\%qt_build%
set jom_path=%~dp0..\..\Qt\Tools\QtCreator\bin\jom

echo qt_path: %qt_path%
echo jom_path: %jom_path%
