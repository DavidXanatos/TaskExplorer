
TEMPLATE = app
TARGET = TaskExplorer
PRECOMPILED_HEADER = stdafx.h

QT += core gui network widgets concurrent

CONFIG += lrelease

MY_ARCH=$$(build_arch)
equals(MY_ARCH, ARM64) {
#  message("Building ARM64")
  CONFIG(debug, debug|release):LIBS = -L../../Bin/ARM64/Debug -L../../ProcessHacker/phlib/bin/DebugARM64 -L../../ProcessHacker/tools/thirdparty/bin/DebugARM64
  CONFIG(release, debug|release):LIBS = -L../../Bin/ARM64/Release -L../../ProcessHacker/phlib/bin/ReleaseARM64 -L../../ProcessHacker/tools/thirdparty/bin/ReleaseARM64
} else:equals(MY_ARCH, x64) {
#  message("Building x64")
  CONFIG(debug, debug|release):LIBS += -L../../Bin/x64/Debug -L../../ProcessHacker/phlib/bin/Debug64 -L../../ProcessHacker/tools/thirdparty/bin/Debug64
  CONFIG(release, debug|release):LIBS += -L../../Bin/x64/Release -L../../ProcessHacker/phlib/bin/Release64 -L../../ProcessHacker/tools/thirdparty/bin/Release64
  QT += winextras
} else {
#  message("Building x86")
  CONFIG(debug, debug|release):LIBS = -L../../Bin/Win32/Debug -L../../ProcessHacker/phlib/bin/Debug32 -L../../ProcessHacker/tools/thirdparty/bin/Debug32
  CONFIG(release, debug|release):LIBS = -L../../Bin/Win32/Release -L../../ProcessHacker/phlib/bin/Release32 -L../../ProcessHacker/tools/thirdparty/bin/Release32
  QT += winextras
}

DEFINES += _UNICODE _ENABLE_EXTENDED_ALIGNED_STORAGE
!equals(MY_ARCH, Win32) {
  DEFINES += WIN64 _WIN64
}

LIBS += -lntdll -luserenv -luxtheme -laclui -lwinsta -liphlpapi -lws2_32 -lphlib -lNetapi32 -lRasapi32 -ltaskschd -lShlwapi -lcfgmgr32 -lGdi32 -lwbemuuid -ldnsapi -lrpcrt4 -lthirdparty -lwindowscodecs -lcomdlg32
LIBS += -lqextwidgets -lMiscHelpers -lqhexedit -lqtservice -lqtsingleapp -lqwt

CONFIG(release, debug|release):{
QMAKE_CXXFLAGS_RELEASE = $$QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO
QMAKE_LFLAGS_RELEASE = $$QMAKE_LFLAGS_RELEASE_WITH_DEBUGINFO
}

QMAKE_CXXFLAGS -= -Zc:strictStrings

equals(MY_ARCH, ARM64) {
#  message("Building ARM64")
  CONFIG(debug, debug|release):DESTDIR = ../../Bin/ARM64/Debug
  CONFIG(release, debug|release):DESTDIR = ../../Bin/ARM64/Release
} else:equals(MY_ARCH, x64) {
#  message("Building x64")
  CONFIG(debug, debug|release):DESTDIR = ../../Bin/x64/Debug
  CONFIG(release, debug|release):DESTDIR = ../../Bin/x64/Release
} else {
#  message("Building x86")
  CONFIG(debug, debug|release):DESTDIR = ../../Bin/Win32/Debug
  CONFIG(release, debug|release):DESTDIR = ../../Bin/Win32/Release
}


INCLUDEPATH += .
INCLUDEPATH += ..\ProcessHacker\phlib\include
INCLUDEPATH += ..\ProcessHacker\kphlib\include
INCLUDEPATH += ..\ProcessHacker\phnt\include

DEPENDPATH += .
MOC_DIR += .
OBJECTS_DIR += debug
UI_DIR += .
RCC_DIR += .



include(TaskExplorer.pri)

win32:RC_FILE = Resources\TaskExplorer.rc


