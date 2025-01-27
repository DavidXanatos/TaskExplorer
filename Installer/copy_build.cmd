REM @ECHO OFF

echo %*
IF "%~4" == "" ( set "openssl_version=3.4.0" ) ELSE ( set "openssl_version=%~4" )
IF "%~3" == "" ( set "qt6_version=6.8.1" ) ELSE ( set "qt6_version=%~3" )
IF "%~2" == "" ( set "qt_version=5.15.2" ) ELSE ( set "qt_version=%~2" )

REM IF "%openssl_version:~0,3%" == "1.1" ( set "sslMajorVersion=1_1" ) ELSE ( set "sslMajorVersion=3" )

IF %1 == x86 (
  set archPath=Win32
  set phPath=%~dp0..\ProcessHacker\bin\Release32
REM  set siPath=%~dp0..\ProcessHacker\KSystemInformer\bin\Release32
  set siPath=%~dp0..\ProcessHacker\KSystemInformer\bin-signed\i386
REM call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
  call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
  set qtPath=%~dp0..\..\Qt\%qt_version%\msvc2019
  set instPath=%~dp0\Build\x86
)
IF %1 == x64 (
  set archPath=x64
  set phPath=%~dp0..\ProcessHacker\bin\Release64
REM  set siPath=%~dp0..\ProcessHacker\KSystemInformer\bin\Release64
  set siPath=%~dp0..\ProcessHacker\KSystemInformer\bin-signed\x64
REM  call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
  call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
  set qtPath=%~dp0..\..\Qt\%qt_version%\msvc2019_64
  set instPath=%~dp0\Build\x64
)
IF %1 == ARM64 (
  set archPath=ARM64
  set phPath=%~dp0..\ProcessHacker\bin\ReleaseARM64
REM  set siPath=%~dp0..\ProcessHacker\KSystemInformer\bin\ReleaseARM64
  set siPath=%~dp0..\ProcessHacker\KSystemInformer\bin-signed\AMD64
REM  call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsamd64_arm64.bat"
  call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsamd64_arm64.bat"
  set qtPath=%~dp0..\..\Qt\%qt6_version%\msvc2022_arm64
  set instPath=%~dp0\Build\a64
  set "sslMajorVersion=1_1"
)
set srcPath=%~dp0..\%archPath%\Release

set VCToolsVersion=14.29.30133

REM set redistPath=%VCToolsRedistDir%\%1\Microsoft.VC142.CRT
REM set redistPath=C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Redist\MSVC\%VCToolsVersion%\%1\Microsoft.VC142.CRT
set redistPath=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\%VCToolsVersion%\%1\Microsoft.VC142.CRT

@echo on

echo inst: %instPath%
echo arch: %archPath%
echo redistr: %redistPath%
echo source: %srcPath%
echo ph: %phPath%
echo si: %siPath%

mkdir %~dp0\Build
mkdir %instPath%

ECHO Copying VC Runtime files
copy "%redistPath%\*" %instPath%\


ECHO Copying Qt libraries

IF NOT %archPath% == ARM64 (
REM IF %archPath% == Win32 (
	copy %qtPath%\bin\Qt5Core.dll %instPath%\
	copy %qtPath%\bin\Qt5Gui.dll %instPath%\
	copy %qtPath%\bin\Qt5Network.dll %instPath%\
	copy %qtPath%\bin\Qt5Widgets.dll %instPath%\
	copy %qtPath%\bin\Qt5WinExtras.dll %instPath%\
	copy %qtPath%\bin\Qt5Svg.dll %instPath%\
REM	copy %qtPath%\bin\Qt5Qml.dll %instPath%\
) ELSE (
	copy %qtPath%\bin\Qt6Core.dll %instPath%\
	copy %qtPath%\bin\Qt6Gui.dll %instPath%\
	copy %qtPath%\bin\Qt6Network.dll %instPath%\
	copy %qtPath%\bin\Qt6Widgets.dll %instPath%\
REM	copy %qtPath%\bin\Qt6Qml.dll %instPath%\
)


mkdir %instPath%\platforms
copy %qtPath%\plugins\platforms\qdirect2d.dll %instPath%\platforms\
copy %qtPath%\plugins\platforms\qminimal.dll %instPath%\platforms\
copy %qtPath%\plugins\platforms\qoffscreen.dll %instPath%\platforms\
copy %qtPath%\plugins\platforms\qwindows.dll %instPath%\platforms\

mkdir %instPath%\styles
copy %qtPath%\plugins\styles\qwindowsvistastyle.dll %instPath%\styles\

mkdir %instPath%\imageformats
copy %qtPath%\plugins\imageformats\qwebp.dll %instPath%\imageformats\
copy %qtPath%\plugins\imageformats\qwbmp.dll %instPath%\imageformats\
copy %qtPath%\plugins\imageformats\qtiff.dll %instPath%\imageformats\
copy %qtPath%\plugins\imageformats\qtga.dll %instPath%\imageformats\
copy %qtPath%\plugins\imageformats\qsvg.dll %instPath%\imageformats\
copy %qtPath%\plugins\imageformats\qjpeg.dll %instPath%\imageformats\
copy %qtPath%\plugins\imageformats\qico.dll %instPath%\imageformats\
copy %qtPath%\plugins\imageformats\qicns.dll %instPath%\imageformats\
copy %qtPath%\plugins\imageformats\qgif.dll %instPath%\imageformats\

IF %archPath% == ARM64 (
mkdir %instPath%\tls
copy %qtPath%\plugins\tls\qopensslbackend.dll %instPath%\tls\
)

REM ECHO Copying OpenSSL libraries
REM IF %archPath% == Win32 (
REM   copy /y %~dp0OpenSSL\Win_x86\bin\libssl-%sslMajorVersion%.dll %instPath%\
REM   copy /y %~dp0OpenSSL\Win_x86\bin\libcrypto-%sslMajorVersion%.dll %instPath%\
REM )
REM IF NOT %archPath% == Win32 (
REM   copy /y %~dp0OpenSSL\Win_%archPath%\bin\libssl-%sslMajorVersion%-%archPath%.dll %instPath%\
REM   copy /y %~dp0OpenSSL\Win_%archPath%\bin\libcrypto-%sslMajorVersion%-%archPath%.dll %instPath%\
REM )


REM ECHO Copying 7zip library
REM copy /y %~dp07-Zip\7-Zip-%archPath%\7z.dll %instPath%\


ECHO Copying project and libraries

copy %srcPath%\MiscHelpers.dll %instPath%\
copy %srcPath%\MiscHelpers.pdb %instPath%\
copy %srcPath%\qextwidgets.dll %instPath%\
copy %srcPath%\qhexedit.pdb %instPath%\
copy %srcPath%\qtservice.dll %instPath%\
copy %srcPath%\qtsingleapp.dll %instPath%\
copy %srcPath%\qhexedit.dll %instPath%\
copy %srcPath%\qwt.dll %instPath%\
copy %srcPath%\TaskExplorer.exe %instPath%\
copy %srcPath%\TaskExplorer.pdb %instPath%\

ECHO Copying translations

mkdir %instPath%\translations\
rem copy /y %~dp0..\TaskExplorer\taskexplorer_*.qm %instPath%\translations\
copy /y %~dp0..\TaskExplorer\Build_SandMan_%archPath%\release\taskexplorer_*.qm %instPath%\translations\
copy /y %~dp0\qttranslations\qm\qt_*.qm %instPath%\translations\
copy /y %~dp0\qttranslations\qm\qtbase_*.qm %instPath%\translations\
copy /y %~dp0\qttranslations\qm\qtmultimedia_*.qm %instPath%\translations\

IF NOT %archPath% == ARM64 (
REM IF %archPath% == Win32 (
REM copy /y %qtPath%\translations\qtscript_*.qm %instPath%\translations\
copy /y %qtPath%\translations\qtxmlpatterns_*.qm %instPath%\translations\
)

"C:\Program Files\7-Zip\7z.exe" a %instPath%\translations.7z %instPath%\translations\*
rmdir /S /Q %instPath%\translations\

ECHO Copying Driver

IF NOT %archPath% == Win32 (
    copy /y %siPath%\ksi.dll %instPath%\
    copy /y %siPath%\ksi.pdb %instPath%\
    copy /y %siPath%\systeminformer.sys %instPath%\
    copy /y %siPath%\systeminformer.pdb %instPath%\

    REM copy /y %phPath%\ksidyn.bin %instPath%\
    REM copy /y %phPath%\ksidyn.sig %instPath%\

    copy /y %siPath%\ksidyn.bin %instPath%\
    copy /y %siPath%\ksidyn.sig %instPath%\
)

copy /y %phPath%\peview.exe %instPath%\
copy /y %phPath%\peview.pdb %instPath%\

copy /y %~dp0.\capslist.txt %instPath%\
copy /y %~dp0.\etwguids.txt %instPath%\
copy /y %~dp0.\pooltag.txt %instPath%\

ECHO sign binaries

for /r "%instPath%" %%f in (*.dll *.exe) do (
    call kph-sig.cmd "%%f"
)

del %instPath%\ksi.sig