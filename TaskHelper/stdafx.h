#pragma once

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
