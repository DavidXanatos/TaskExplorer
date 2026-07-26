#pragma once

//
// The helper's own precompiled header. Deliberately Qt-free: this is what makes
// TaskExplorer/Common/Variant.cpp and Buffer.cpp compile into the helper without
// dragging Qt in - they include "stdafx.h", and the helper's include path puts
// this one first.
//

#ifdef WIN32

// Prevent windows.h from including winsock.h
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_

// Windows includes (must come first)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <minidumpapiset.h>
#include <tlhelp32.h>

#endif // WIN32

// std includes
#include <string>
#include <sstream>
#include <deque>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <atomic>
#include <cstring>
#include <climits>

// other includes

#define _T(x)      L ## x

#define STR2(X) #X
#define STR(X) STR2(X)

#define ARRSIZE(x)	(sizeof(x)/sizeof(x[0]))

#ifndef Max
#define Max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef Min
#define Min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifdef _DEBUG
#define SAFE_MODE
#endif

//#include "../MiscHelpers/Common/DebugHelpers.h"
#define ASSERT(x)
