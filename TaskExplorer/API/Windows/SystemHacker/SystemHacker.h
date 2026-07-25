#pragma once
// Windows Header Files
//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>
#include <phnt_windows.h>
#include <phnt.h>

#include "../../../MiscHelpers/Common/FlexError.h"

STATUS InitKSH(QString DeviceName = "", QString FileName = "", int SecurityLevel = 0);

BOOLEAN NTAPI KshIsConnected(VOID);

NTSTATUS NTAPI KshGetFeatures(_Inout_ PULONG Features);