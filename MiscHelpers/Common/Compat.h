#pragma once

#include <stdarg.h>
#include <wchar.h>

//
// Small platform shims that must stay usable from code with no Qt dependency.
//
// This exists so that TaskExplorer/Common/Exception.h - which is included by the
// STL-only variant serialisation shared with TaskHelper - does not have to pull
// in MiscHelpers/Common/Common.h, and with it all of Qt. The helper is
// deliberately Qt-free; see TaskHelper/CMakeLists.txt.
//

#ifndef WIN32
//
// vswprintf_l is an MSVC extension. The replacement is not a straight alias for
// vswprintf, because the two disagree on wide format specifiers: in a wide
// format string MSVC reads %s as a wide string and %S as a narrow one, while
// glibc does the opposite. The shim swaps them so that format strings written
// for MSVC behave the same way here.
//
int vswprintf_l(wchar_t* _String, size_t _Count, const wchar_t* _Format, va_list _Ap);
#endif
