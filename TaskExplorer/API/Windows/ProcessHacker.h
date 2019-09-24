#pragma once

#define _PHLIB_
#define _PHAPP_
#define _WINDOWS

#ifdef _DEBUG
#define DEBUG
#endif

#ifdef WIN64
#ifndef _WIN64
#deinfe _WIN64
#endif
#endif

#if !defined(_PHAPP_)
#define PHAPPAPI __declspec(dllimport)
#else
#define PHAPPAPI __declspec(dllexport)
#endif

#include <ph.h>
#include <guisup.h>
#include <provider.h>
#include <filestream.h>
#include <fastlock.h>
#include <treenew.h>
#include <graph.h>
#include <circbuf.h>
#include <dltmgr.h>
#include <phnet.h>
#include <kphuser.h>
#include <appresolver.h>
#include <hndlinfo.h>
#include <verify.h>
#include <ref.h>
#include <subprocesstag.h>
#include <secedit.h>
#include <symprv.h>
#include <svcsup.h>
#include <mapimg.h>
#include <combaseapi.h>
#include <lsasup.h>
#include <phnative.h>
#include <ntgdi.h>


#include "../../Common/FlexError.h"

// begin_phapppub
#define DPCS_PROCESS_ID ((HANDLE)(LONG_PTR)-2)
#define INTERRUPTS_PROCESS_ID ((HANDLE)(LONG_PTR)-3)

// DPCs, Interrupts and System Idle Process are not real.
// Non-"real" processes can never be opened.
#define PH_IS_REAL_PROCESS_ID(ProcessId) ((LONG_PTR)(ProcessId) > 0)

// DPCs and Interrupts are fake, but System Idle Process is not.
#define PH_IS_FAKE_PROCESS_ID(ProcessId) ((LONG_PTR)(ProcessId) < 0)

// The process item has been removed.
#define PH_PROCESS_ITEM_REMOVED 0x1
// end_phapppub

#define PH_INTEGRITY_STR_LEN 10
#define PH_INTEGRITY_STR_LEN_1 (PH_INTEGRITY_STR_LEN + 1)


QString CastPhString(PPH_STRING phString, bool bDeRef = true);
PPH_STRING CastQString(const QString& qString);

quint64 FILETIME2ms(quint64 fileTime);
time_t FILETIME2time(quint64 fileTime);

// Missing phlib definitions
extern "C" {
	VERIFY_RESULT NTAPI PhVerifyFileCached(_In_ PPH_STRING FileName, _In_opt_ PPH_STRING PackageFullName, _Out_opt_ PPH_STRING *SignerName, _In_ BOOLEAN CachedOnly);
}

// initialization call
int InitPH(bool bSvc = false);

STATUS InitKPH(QString DeviceName = QString(), QString FileName = QString(), bool bPrivilegeCheck = true);

void PhShowAbout(QWidget* parent);



