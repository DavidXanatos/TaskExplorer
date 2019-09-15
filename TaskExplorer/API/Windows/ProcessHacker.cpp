/*
 * Task Explorer -
 *   qt wrapper and helper functions
 *
 * Copyright (C) 2009-2016 wj32
 * Copyright (C) 2017-2019 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains Process Hacker code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "ProcessHacker.h"
#include <settings.h>
extern "C" {
#include <kphuserp.h>
}

#include "../../SVC/TaskService.h"

QString CastPhString(PPH_STRING phString, bool bDeRef)
{
	QString qString;
	if (phString)
	{
		qString = QString::fromWCharArray(phString->Buffer, phString->Length / sizeof(wchar_t));
		if (bDeRef)
			PhDereferenceObject(phString);
	}
	return qString;
}

PPH_STRING CastQString(const QString& qString)
{
	wstring wstr = qString.toStdWString();
	UNICODE_STRING ustr;
	RtlInitUnicodeString(&ustr, (wchar_t*)wstr.c_str());
	return PhCreateStringFromUnicodeString(&ustr);
}

// MSDN: FILETIME Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).

quint64 FILETIME2ms(quint64 fileTime)
{
	if (fileTime < 116444736000000000ULL)
		return 0;
	return (fileTime - 116444736000000000ULL) / 10000ULL;
}

time_t FILETIME2time(quint64 fileTime)
{
	return FILETIME2ms(fileTime) / 1000ULL;
}

/*
BOOLEAN PhInitializeNamespacePolicy(
	VOID
)
{
	HANDLE mutantHandle;
	WCHAR objectName[PH_INT64_STR_LEN_1];
	OBJECT_ATTRIBUTES objectAttributes;
	UNICODE_STRING objectNameUs;
	PH_FORMAT format[2];

	PhInitFormatS(&format[0], L"PhMutant_");
	PhInitFormatU(&format[1], HandleToUlong(NtCurrentProcessId()));

	if (!PhFormatToBuffer(
		format,
		RTL_NUMBER_OF(format),
		objectName,
		sizeof(objectName),
		NULL
	))
	{
		return FALSE;
	}

	RtlInitUnicodeString(&objectNameUs, objectName);
	InitializeObjectAttributes(
		&objectAttributes,
		&objectNameUs,
		OBJ_CASE_INSENSITIVE,
		PhGetNamespaceHandle(),
		NULL
	);

	if (NT_SUCCESS(NtCreateMutant(
		&mutantHandle,
		MUTANT_QUERY_STATE,
		&objectAttributes,
		TRUE
	)))
	{
		return TRUE;
	}

	return FALSE;
}
*/

VOID PhpEnablePrivileges(
	VOID
)
{
	HANDLE tokenHandle;

	if (NT_SUCCESS(PhOpenProcessToken(
		NtCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES,
		&tokenHandle
	)))
	{
		CHAR privilegesBuffer[FIELD_OFFSET(TOKEN_PRIVILEGES, Privileges) + sizeof(LUID_AND_ATTRIBUTES) * 9];
		PTOKEN_PRIVILEGES privileges;
		ULONG i;

		privileges = (PTOKEN_PRIVILEGES)privilegesBuffer;
		privileges->PrivilegeCount = 9;

		for (i = 0; i < privileges->PrivilegeCount; i++)
		{
			privileges->Privileges[i].Attributes = SE_PRIVILEGE_ENABLED;
			privileges->Privileges[i].Luid.HighPart = 0;
		}

		privileges->Privileges[0].Luid.LowPart = SE_DEBUG_PRIVILEGE;
		privileges->Privileges[1].Luid.LowPart = SE_INC_BASE_PRIORITY_PRIVILEGE;
		privileges->Privileges[2].Luid.LowPart = SE_INC_WORKING_SET_PRIVILEGE;
		privileges->Privileges[3].Luid.LowPart = SE_LOAD_DRIVER_PRIVILEGE;
		privileges->Privileges[4].Luid.LowPart = SE_PROF_SINGLE_PROCESS_PRIVILEGE;
		privileges->Privileges[5].Luid.LowPart = SE_BACKUP_PRIVILEGE;
		privileges->Privileges[6].Luid.LowPart = SE_RESTORE_PRIVILEGE;
		privileges->Privileges[7].Luid.LowPart = SE_SHUTDOWN_PRIVILEGE;
		privileges->Privileges[8].Luid.LowPart = SE_TAKE_OWNERSHIP_PRIVILEGE;

		NtAdjustPrivilegesToken(
			tokenHandle,
			FALSE,
			privileges,
			0,
			NULL,
			NULL
		);

		NtClose(tokenHandle);
	}
}

NTSTATUS PhpReadSignature(
    _In_ PWSTR FileName,
    _Out_ PUCHAR *Signature,
    _Out_ PULONG SignatureSize
    )
{
    NTSTATUS status;
    HANDLE fileHandle;
    PUCHAR signature;
    ULONG bufferSize;
    IO_STATUS_BLOCK iosb;

    if (!NT_SUCCESS(status = PhCreateFileWin32(&fileHandle, FileName, FILE_GENERIC_READ, FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ, FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT)))
    {
        return status;
    }

    bufferSize = 1024;
    signature = (PUCHAR)PhAllocate(bufferSize);

    status = NtReadFile(fileHandle, NULL, NULL, NULL, &iosb, signature, bufferSize, NULL, NULL);
    NtClose(fileHandle);

    if (NT_SUCCESS(status))
    {
        *Signature = signature;
        *SignatureSize = (ULONG)iosb.Information;
        return status;
    }
    else
    {
        PhFree(signature);
        return status;
    }
}

VOID PhInitializeKph(
    VOID
    )
{
    NTSTATUS status;
    PPH_STRING applicationDirectory;
    PPH_STRING kprocesshackerFileName;
    //PPH_STRING processhackerSigFileName;
    KPH_PARAMETERS parameters;

    if (!(applicationDirectory = PhGetApplicationDirectory()))
        return;

    kprocesshackerFileName = PhConcatStringRefZ(&applicationDirectory->sr, L"kprocesshacker.sys");
    //processhackerSigFileName = PhConcatStringRefZ(&applicationDirectory->sr, L"ProcessHacker.sig");
    PhDereferenceObject(applicationDirectory);

    if (!RtlDoesFileExists_U(kprocesshackerFileName->Buffer))
    {
		qDebug() << "The Process Hacker kernel driver 'kprocesshacker.sys' was not found in the application directory.";
        // return;
    }

    parameters.SecurityLevel = KphSecurityPrivilegeCheck;
    parameters.CreateDynamicConfiguration = TRUE;

    if (NT_SUCCESS(status = KphConnect2Ex(
        //KPH_DEVICE_SHORT_NAME,
		L"XProcessHacker3",
        kprocesshackerFileName->Buffer,
        &parameters
        )))
    {
		qDebug() << "Process Hacker kernel driver connected.";

/*
		PUCHAR signature;
        ULONG signatureSize;

        status = PhpReadSignature(
            processhackerSigFileName->Buffer, 
            &signature, 
            &signatureSize
            );

        if (NT_SUCCESS(status))
        {
            status = KphVerifyClient(signature, signatureSize);

            if (!NT_SUCCESS(status))
            {
				qDebug() << "Unable to verify the kernel driver signature.";
            }

            PhFree(signature);
        }
        else
        {
			qDebug() << "Unable to load the kernel driver signature.";
        }
*/
    }
    else
    {
		qDebug() << "Unable to load the kernel driver.";
    }

    PhDereferenceObject(kprocesshackerFileName);
    //PhDereferenceObject(processhackerSigFileName);
}

HMODULE GetThisModuleHandle()
{
    //Returns module handle where this function is running in: EXE or DLL
    HMODULE hModule = NULL;
    ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, 
        (LPCTSTR)GetThisModuleHandle, &hModule);

    return hModule;
}

int InitPH(bool bSvc)
{
	HINSTANCE Instance = GetThisModuleHandle(); // (HINSTANCE)::GetModuleHandle(NULL);
	LONG result;
#ifdef DEBUG
	PHP_BASE_THREAD_DBG dbg;
#endif

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (!NT_SUCCESS(PhInitializePhLibEx(L"Task Explorer", ULONG_MAX, Instance, 0, 0)))
		return 1;
	//if (!PhInitializeExceptionPolicy())
	//	return 1;
	//if (!PhInitializeNamespacePolicy())
	//	return 1;
	//if (!PhInitializeMitigationPolicy())
	//	return 1;
	//if (!PhInitializeRestartPolicy())
	//    return 1;

	//PhpProcessStartupParameters();
	PhpEnablePrivileges();

	if (bSvc)
	{
		// Enable some required privileges.
		HANDLE tokenHandle;
		if (NT_SUCCESS(PhOpenProcessToken(NtCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tokenHandle)))
		{
			PhSetTokenPrivilege2(tokenHandle, SE_ASSIGNPRIMARYTOKEN_PRIVILEGE, SE_PRIVILEGE_ENABLED);
			PhSetTokenPrivilege2(tokenHandle, SE_INCREASE_QUOTA_PRIVILEGE, SE_PRIVILEGE_ENABLED);
			PhSetTokenPrivilege2(tokenHandle, SE_BACKUP_PRIVILEGE, SE_PRIVILEGE_ENABLED);
			PhSetTokenPrivilege2(tokenHandle, SE_RESTORE_PRIVILEGE, SE_PRIVILEGE_ENABLED);
			PhSetTokenPrivilege2(tokenHandle, SE_IMPERSONATE_PRIVILEGE, SE_PRIVILEGE_ENABLED);
			NtClose(tokenHandle);
		}

		return 0;
	}

	PhSettingsInitialization();
    //PhpInitializeSettings();
	// note: this is needed to open the permissions panel
	PhpAddIntegerSetting(L"EnableSecurityAdvancedDialog", L"1");
	PhpAddStringSetting(L"FileBrowseExecutable", L"%SystemRoot%\\explorer.exe /select,\"%s\"");
	// for permissions diaog on win 7
	PhpAddIntegerSetting(L"EnableThemeSupport", L"0");
	PhpAddIntegerSetting(L"GraphColorMode", L"1");
	PhpAddIntegerSetting(L"TreeListBorderEnable", L"0");

    /*if (!PhIsExecutingInWow64() && theConf->GetBool("Options/UseKProcessHacker", true))
    {
        PhInitializeKph();
    }*/

	return 0;
}

STATUS InitKPH(QString DeviceName, QString FileName, bool bPrivilegeCheck)
{
	if (DeviceName.isEmpty())
		DeviceName = QString::fromWCharArray(KPH_DEVICE_SHORT_NAME);
	if (FileName.isEmpty())
		FileName = "kprocesshacker.sys";

	// if the file name is not a full path Add the application directory
	if (!FileName.contains("\\"))
		FileName = QApplication::applicationDirPath() + "/" + FileName;

	FileName = FileName.replace("/", "\\");
    if (!QFile::exists(FileName))
		return ERR(QObject::tr("The Process Hacker kernel driver '%1' was not found.").arg(FileName), STATUS_NOT_FOUND);

	KPH_PARAMETERS parameters;
    parameters.SecurityLevel = bPrivilegeCheck ? KphSecurityPrivilegeCheck : KphSecurityNone;
    parameters.CreateDynamicConfiguration = TRUE;
	NTSTATUS status = KphConnect2Ex((wchar_t*)DeviceName.toStdWString().c_str(), (wchar_t*)FileName.toStdWString().c_str(), &parameters);
    if (!NT_SUCCESS(status))
		return ERR(QObject::tr("Unable to load the kernel driver, Error: %1").arg(CastPhString(PhGetNtMessage(status))), status);
    
	PUCHAR signature = NULL;
    ULONG signatureSize = 0;

	QString SigFileName = QApplication::applicationDirPath() + "/TaskExplorer.sig";
	SigFileName = SigFileName.replace("/", "\\");
	if (QFile::exists(SigFileName))
		PhpReadSignature((wchar_t*)SigFileName.toStdWString().c_str(), &signature, &signatureSize);

	// Note: Debug driver accepts a empty signature as valid
	if (NT_SUCCESS(KphVerifyClient(signature, signatureSize)))
	{
#ifdef _DEBUG
		KPH_KEY Key;
		KPHP_GET_L1_KEY_CONTEXT Context = { &Key };
		KphpGetL1KeyContinuation(-1, &Context); // "Master"-Key LOL
#endif
		qDebug() << "Successfully verified the client signature.";
	}
	else
		qDebug() << "Unable to verify the client signature.";

	if(signature)
		PhFree(signature);

	return OK;
}

void PhShowAbout(QWidget* parent)
{
		QString AboutCaption = QString(
			"<h3>Process Hacker</h3>"
			"<p>Licensed under the GNU GPL, v3.</p>"
			"<p>Copyright (c) 2008-2019</p>"
		).arg("3.0.2450");
		QString AboutText = QString(
                "<p>Thanks to:<br>"
                "    <a href=\"https://github.com/wj32\">wj32</a> - Wen Jia Liu<br>"
                "    <a href=\"https://github.com/dmex\">dmex</a> - Steven G<br>"
                "    <a href=\"https://github.com/xhmikosr\">XhmikosR</a><br>"
                "    <a href=\"https://github.com/processhacker/processhacker/graphs/contributors\">Contributors</a> - thank you for your additions!<br>"
                "    Donors - thank you for your support!</p>"
                "<p>Process Hacker uses the following components:<br>"
                "    <a href=\"https://github.com/michaelrsweet/mxml\">Mini-XML</a> by Michael Sweet<br>"
                "    <a href=\"https://www.pcre.org\">PCRE</a><br>"
                "    <a href=\"https://github.com/json-c/json-c\">json-c</a><br>"
                "    MD5 code by Jouni Malinen<br>"
                "    SHA1 code by Filip Navara, based on code by Steve Reid<br>"
                "    <a href=\"http://www.famfamfam.com/lab/icons/silk\">Silk icons</a><br>"
                "    <a href=\"https://www.fatcow.com/free-icons\">Farm-fresh web icons</a><br></p>"
			"<p></p>"
			"<p>Visit <a href=\"https://github.com/processhacker/processhacker\">Process Hacker on github</a> for more information.</p>"
		);
		QMessageBox *msgBox = new QMessageBox(parent);
		msgBox->setAttribute(Qt::WA_DeleteOnClose);
		msgBox->setWindowTitle(QString("About ProcessHacker Library"));
		msgBox->setText(AboutCaption);
		msgBox->setInformativeText(AboutText);

		QIcon ico(QLatin1String(":/ProcessHacker.png"));
		msgBox->setIconPixmap(ico.pixmap(64, 64));
#if defined(Q_WS_WINCE)
		msgBox->setDefaultButton(msgBox->addButton(QMessageBox::Ok));
#endif
		
		msgBox->exec();
}

extern "C" {
	VOID NTAPI PhAddDefaultSettings()
	{
	}

	VOID NTAPI PhUpdateCachedSettings()
	{
	}
}

////////////////////////////////////////////////////////////////////////////////////
// appsup.c

PH_KNOWN_PROCESS_TYPE PhGetProcessKnownTypeEx(quint64 ProcessId, const QString& FileName)
{
	int knownProcessType;
	PH_STRINGREF systemRootPrefix;
	PPH_STRING fileName;
	PH_STRINGREF name;
#ifdef _WIN64
	BOOLEAN isWow64 = FALSE;
#endif

	if (ProcessId == (quint64)SYSTEM_PROCESS_ID || ProcessId == (quint64)SYSTEM_IDLE_PROCESS_ID)
		return SystemProcessType;

	if (FileName.isEmpty())
		return UnknownProcessType;

	PhGetSystemRoot(&systemRootPrefix);

	fileName = CastQString(FileName);
	name = fileName->sr;

	knownProcessType = UnknownProcessType;

	if (PhStartsWithStringRef(&name, &systemRootPrefix, TRUE))
	{
		// Skip the system root, and we now have three cases:
		// 1. \\xyz.exe - Windows executable.
		// 2. \\System32\\xyz.exe - system32 executable.
		// 3. \\SysWow64\\xyz.exe - system32 executable + WOW64.
		PhSkipStringRef(&name, systemRootPrefix.Length);

		if (PhEqualStringRef2(&name, L"\\explorer.exe", TRUE))
		{
			knownProcessType = ExplorerProcessType;
		}
		else if (
			PhStartsWithStringRef2(&name, L"\\System32", TRUE)
#ifdef _WIN64
			|| (PhStartsWithStringRef2(&name, L"\\SysWow64", TRUE) && (isWow64 = TRUE, TRUE)) // ugly but necessary
#endif
			)
		{
			// SysTem32 and SysWow64 are both 8 characters long.
			PhSkipStringRef(&name, 9 * sizeof(WCHAR));

			if (FALSE)
				; // Dummy
			else if (PhEqualStringRef2(&name, L"\\smss.exe", TRUE))
				knownProcessType = SessionManagerProcessType;
			else if (PhEqualStringRef2(&name, L"\\csrss.exe", TRUE))
				knownProcessType = WindowsSubsystemProcessType;
			else if (PhEqualStringRef2(&name, L"\\wininit.exe", TRUE))
				knownProcessType = WindowsStartupProcessType;
			else if (PhEqualStringRef2(&name, L"\\services.exe", TRUE))
				knownProcessType = ServiceControlManagerProcessType;
			else if (PhEqualStringRef2(&name, L"\\lsass.exe", TRUE))
				knownProcessType = LocalSecurityAuthorityProcessType;
			else if (PhEqualStringRef2(&name, L"\\lsm.exe", TRUE))
				knownProcessType = LocalSessionManagerProcessType;
			else if (PhEqualStringRef2(&name, L"\\winlogon.exe", TRUE))
				knownProcessType = WindowsLogonProcessType;
			else if (PhEqualStringRef2(&name, L"\\svchost.exe", TRUE))
				knownProcessType = ServiceHostProcessType;
			else if (PhEqualStringRef2(&name, L"\\rundll32.exe", TRUE))
				knownProcessType = RunDllAsAppProcessType;
			else if (PhEqualStringRef2(&name, L"\\dllhost.exe", TRUE))
				knownProcessType = ComSurrogateProcessType;
			else if (PhEqualStringRef2(&name, L"\\taskeng.exe", TRUE))
				knownProcessType = TaskHostProcessType;
			else if (PhEqualStringRef2(&name, L"\\taskhost.exe", TRUE))
				knownProcessType = TaskHostProcessType;
			else if (PhEqualStringRef2(&name, L"\\taskhostex.exe", TRUE))
				knownProcessType = TaskHostProcessType;
			else if (PhEqualStringRef2(&name, L"\\taskhostw.exe", TRUE))
				knownProcessType = TaskHostProcessType;
			else if (PhEqualStringRef2(&name, L"\\wudfhost.exe", TRUE))
				knownProcessType = UmdfHostProcessType;
			else if (PhEqualStringRef2(&name, L"\\wbem\\WmiPrvSE.exe", TRUE))
				knownProcessType = WmiProviderHostType;
			else if (PhEqualStringRef2(&name, L"\\MicrosoftEdgeCP.exe", TRUE)) // RS5
				knownProcessType = EdgeProcessType;
			else if (PhEqualStringRef2(&name, L"\\MicrosoftEdgeSH.exe", TRUE)) // RS5
				knownProcessType = EdgeProcessType;
		}
		else
		{
			if (PhEndsWithStringRef2(&name, L"\\MicrosoftEdgeCP.exe", TRUE)) // RS4
				knownProcessType = EdgeProcessType;
			else if (PhEndsWithStringRef2(&name, L"\\MicrosoftEdge.exe", TRUE))
				knownProcessType = EdgeProcessType;
			else if (PhEndsWithStringRef2(&name, L"\\ServiceWorkerHost.exe", TRUE))
				knownProcessType = EdgeProcessType;
			else if (PhEndsWithStringRef2(&name, L"\\Windows.WARP.JITService.exe", TRUE))
				knownProcessType = EdgeProcessType;
		}
	}

	PhDereferenceObject(fileName);

#ifdef _WIN64
	if (isWow64)
		knownProcessType |= KnownProcessWow64;
#endif

	return (PH_KNOWN_PROCESS_TYPE)knownProcessType;
}

BOOLEAN NTAPI PhpSvchostCommandLineCallback(_In_opt_ PPH_COMMAND_LINE_OPTION Option, _In_opt_ PPH_STRING Value, _In_opt_ PVOID Context)
{
    PPH_KNOWN_PROCESS_COMMAND_LINE knownCommandLine = (PPH_KNOWN_PROCESS_COMMAND_LINE)Context;

	if (Option && Option->Id == 1)
	{
		PhSwapReference((PVOID*)&knownCommandLine->ServiceHost.GroupName, Value);
	}

    return TRUE;
}


BOOLEAN PhaGetProcessKnownCommandLine(
    _In_ PPH_STRING CommandLine,
    _In_ PH_KNOWN_PROCESS_TYPE KnownProcessType,
    _Out_ PPH_KNOWN_PROCESS_COMMAND_LINE KnownCommandLine
    )
{
    switch (KnownProcessType & KnownProcessTypeMask)
    {
    case ServiceHostProcessType:
        {
            // svchost.exe -k <GroupName>

            static PH_COMMAND_LINE_OPTION options[] =
            {
                { 1, L"k", MandatoryArgumentType }
            };

            KnownCommandLine->ServiceHost.GroupName = NULL;

            PhParseCommandLine(
                &CommandLine->sr,
                options,
                sizeof(options) / sizeof(PH_COMMAND_LINE_OPTION),
                PH_COMMAND_LINE_IGNORE_UNKNOWN_OPTIONS,
                PhpSvchostCommandLineCallback,
                KnownCommandLine
                );

            if (KnownCommandLine->ServiceHost.GroupName)
            {
                PH_AUTO(KnownCommandLine->ServiceHost.GroupName);
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
        break;
    case RunDllAsAppProcessType:
        {
            // rundll32.exe <DllName>,<ProcedureName> ...

            SIZE_T i;
            PH_STRINGREF dllNamePart;
            PH_STRINGREF procedureNamePart;
            PPH_STRING dllName;
            PPH_STRING procedureName;

            i = 0;

            // Get the rundll32.exe part.

            dllName = PhParseCommandLinePart(&CommandLine->sr, &i);

            if (!dllName)
                return FALSE;

            PhDereferenceObject(dllName);

            // Get the DLL name part.

            while (i < CommandLine->Length / sizeof(WCHAR) && CommandLine->Buffer[i] == ' ')
                i++;

            dllName = PhParseCommandLinePart(&CommandLine->sr, &i);

            if (!dllName)
                return FALSE;

            PH_AUTO(dllName);

            // The procedure name begins after the last comma.

            if (!PhSplitStringRefAtLastChar(&dllName->sr, ',', &dllNamePart, &procedureNamePart))
                return FALSE;

            dllName = (PPH_STRING)PH_AUTO(PhCreateString2(&dllNamePart));
            procedureName = (PPH_STRING)PH_AUTO(PhCreateString2(&procedureNamePart));

            // If the DLL name isn't an absolute path, assume it's in system32.
            // TO-DO: Use a proper search function.

            if (RtlDetermineDosPathNameType_U(dllName->Buffer) == RtlPathTypeRelative)
            {
                dllName = PhaConcatStrings(
                    3,
                    PH_AUTO_T(PH_STRING, PhGetSystemDirectory())->Buffer,
                    L"\\",
                    dllName->Buffer
                    );
            }

            KnownCommandLine->RunDllAsApp.FileName = dllName;
            KnownCommandLine->RunDllAsApp.ProcedureName = procedureName;
        }
        break;
    case ComSurrogateProcessType:
        {
            // dllhost.exe /processid:<Guid>

            static PH_STRINGREF inprocServer32Name = PH_STRINGREF_INIT(L"InprocServer32");

            SIZE_T i;
            ULONG_PTR indexOfProcessId;
            PPH_STRING argPart;
            PPH_STRING guidString;
            UNICODE_STRING guidStringUs;
            GUID guid;
            HANDLE rootKeyHandle;
            HANDLE inprocServer32KeyHandle;
            PPH_STRING fileName;

            i = 0;

            // Get the dllhost.exe part.

            argPart = PhParseCommandLinePart(&CommandLine->sr, &i);

            if (!argPart)
                return FALSE;

            PhDereferenceObject(argPart);

            // Get the argument part.

            while (i < (ULONG)CommandLine->Length / sizeof(WCHAR) && CommandLine->Buffer[i] == ' ')
                i++;

            argPart = PhParseCommandLinePart(&CommandLine->sr, &i);

            if (!argPart)
                return FALSE;

            PH_AUTO(argPart);

            // Find "/processid:"; the GUID is just after that.

            _wcsupr(argPart->Buffer);
            indexOfProcessId = PhFindStringInString(argPart, 0, L"/PROCESSID:");

            if (indexOfProcessId == -1)
                return FALSE;

            guidString = PhaSubstring(
                argPart,
                indexOfProcessId + 11,
                (ULONG)argPart->Length / sizeof(WCHAR) - indexOfProcessId - 11
                );
            PhStringRefToUnicodeString(&guidString->sr, &guidStringUs);

            if (!NT_SUCCESS(RtlGUIDFromString(
                &guidStringUs,
                &guid
                )))
                return FALSE;

            KnownCommandLine->ComSurrogate.Guid = guid;
            KnownCommandLine->ComSurrogate.Name = NULL;
            KnownCommandLine->ComSurrogate.FileName = NULL;

            // Lookup the GUID in the registry to determine the name and file name.

            if (NT_SUCCESS(PhOpenKey(
                &rootKeyHandle,
                KEY_READ,
                PH_KEY_CLASSES_ROOT,
                &PhaConcatStrings2(L"CLSID\\", guidString->Buffer)->sr,
                0
                )))
            {
                KnownCommandLine->ComSurrogate.Name =
                    (PPH_STRING)PH_AUTO(PhQueryRegistryString(rootKeyHandle, NULL));

                if (NT_SUCCESS(PhOpenKey(
                    &inprocServer32KeyHandle,
                    KEY_READ,
                    rootKeyHandle,
                    &inprocServer32Name,
                    0
                    )))
                {
                    KnownCommandLine->ComSurrogate.FileName =
                        (PPH_STRING)PH_AUTO(PhQueryRegistryString(inprocServer32KeyHandle, NULL));

                    if (fileName = (PPH_STRING)PH_AUTO(PhExpandEnvironmentStrings(
                        &KnownCommandLine->ComSurrogate.FileName->sr
                        )))
                    {
                        KnownCommandLine->ComSurrogate.FileName = fileName;
                    }

                    NtClose(inprocServer32KeyHandle);
                }

                NtClose(rootKeyHandle);
            }
            else if (NT_SUCCESS(PhOpenKey(
                &rootKeyHandle,
                KEY_READ,
                PH_KEY_CLASSES_ROOT,
                &PhaConcatStrings2(L"AppID\\", guidString->Buffer)->sr,
                0
                )))
            {
                KnownCommandLine->ComSurrogate.Name =
                    (PPH_STRING)PH_AUTO(PhQueryRegistryString(rootKeyHandle, NULL));
                NtClose(rootKeyHandle);
            }
        }
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

GUID XP_CONTEXT_GUID = { 0xbeb1b341, 0x6837, 0x4c83, { 0x83, 0x66, 0x2b, 0x45, 0x1e, 0x7c, 0xe6, 0x9b } };
GUID VISTA_CONTEXT_GUID = { 0xe2011457, 0x1546, 0x43c5, { 0xa5, 0xfe, 0x00, 0x8d, 0xee, 0xe3, 0xd3, 0xf0 } };
GUID WIN7_CONTEXT_GUID = { 0x35138b9a, 0x5d96, 0x4fbd, { 0x8e, 0x2d, 0xa2, 0x44, 0x02, 0x25, 0xf9, 0x3a } };
GUID WIN8_CONTEXT_GUID = { 0x4a2f28e3, 0x53b9, 0x4441, { 0xba, 0x9c, 0xd6, 0x9d, 0x4a, 0x4a, 0x6e, 0x38 } };
GUID WINBLUE_CONTEXT_GUID = { 0x1f676c76, 0x80e1, 0x4239, { 0x95, 0xbb, 0x83, 0xd0, 0xf6, 0xd0, 0xda, 0x78 } };
GUID WIN10_CONTEXT_GUID = { 0x8e0f7a12, 0xbfb3, 0x4fe8, { 0xb9, 0xa5, 0x48, 0xfd, 0x50, 0xa1, 0x5a, 0x9a } };

/**
 * Determines the OS compatibility context of a process.
 *
 * \param ProcessHandle A handle to a process.
 * \param Guid A variable which receives a GUID identifying an
 * operating system version.
 */
NTSTATUS PhGetProcessSwitchContext(
    _In_ HANDLE ProcessHandle,
    _Out_ PGUID Guid
    )
{
    NTSTATUS status;
    PROCESS_BASIC_INFORMATION basicInfo;
#ifdef _WIN64
    PVOID peb32;
    ULONG data32;
#endif
    PVOID data;

    // Reverse-engineered from WdcGetProcessSwitchContext (wdc.dll).
    // On Windows 8, the function is now SdbGetAppCompatData (apphelp.dll).
    // On Windows 10, the function is again WdcGetProcessSwitchContext.

#ifdef _WIN64
    if (NT_SUCCESS(PhGetProcessPeb32(ProcessHandle, &peb32)) && peb32)
    {
        if (WindowsVersion >= WINDOWS_8)
        {
            if (!NT_SUCCESS(status = NtReadVirtualMemory(
                ProcessHandle,
                PTR_ADD_OFFSET(peb32, FIELD_OFFSET(PEB32, pShimData)),
                &data32,
                sizeof(ULONG),
                NULL
                )))
                return status;
        }
        else
        {
            if (!NT_SUCCESS(status = NtReadVirtualMemory(
                ProcessHandle,
                PTR_ADD_OFFSET(peb32, FIELD_OFFSET(PEB32, pContextData)),
                &data32,
                sizeof(ULONG),
                NULL
                )))
                return status;
        }

        data = UlongToPtr(data32);
    }
    else
    {
#endif
        if (!NT_SUCCESS(status = PhGetProcessBasicInformation(ProcessHandle, &basicInfo)))
            return status;

        if (WindowsVersion >= WINDOWS_8)
        {
            if (!NT_SUCCESS(status = NtReadVirtualMemory(
                ProcessHandle,
                PTR_ADD_OFFSET(basicInfo.PebBaseAddress, FIELD_OFFSET(PEB, pShimData)),
                &data,
                sizeof(PVOID),
                NULL
                )))
                return status;
        }
        else
        {
            if (!NT_SUCCESS(status = NtReadVirtualMemory(
                ProcessHandle,
                PTR_ADD_OFFSET(basicInfo.PebBaseAddress, FIELD_OFFSET(PEB, pUnused)),
                &data,
                sizeof(PVOID),
                NULL
                )))
                return status;
        }
#ifdef _WIN64
    }
#endif

    if (!data)
        return STATUS_UNSUCCESSFUL; // no compatibility context data

    if (WindowsVersion >= WINDOWS_10_RS5)
    {
        if (!NT_SUCCESS(status = NtReadVirtualMemory(
            ProcessHandle,
            PTR_ADD_OFFSET(data, 2040 + 24), // Magic value from SbReadProcContextByHandle
            Guid,
            sizeof(GUID),
            NULL
            )))
            return status;
    }
    else if (WindowsVersion >= WINDOWS_10_RS2)
    {
        if (!NT_SUCCESS(status = NtReadVirtualMemory(
            ProcessHandle,
            PTR_ADD_OFFSET(data, 1544),
            Guid,
            sizeof(GUID),
            NULL
            )))
            return status;
    }
    else if (WindowsVersion >= WINDOWS_10)
    {
        if (!NT_SUCCESS(status = NtReadVirtualMemory(
            ProcessHandle,
            PTR_ADD_OFFSET(data, 2040 + 24), // Magic value from SbReadProcContextByHandle
            Guid,
            sizeof(GUID),
            NULL
            )))
            return status;
    }
    else if (WindowsVersion >= WINDOWS_8_1)
    {
        if (!NT_SUCCESS(status = NtReadVirtualMemory(
            ProcessHandle,
            PTR_ADD_OFFSET(data, 2040 + 16), // Magic value from SbReadProcContextByHandle
            Guid,
            sizeof(GUID),
            NULL
            )))
            return status;
    }
    else if (WindowsVersion >= WINDOWS_8)
    {
        if (!NT_SUCCESS(status = NtReadVirtualMemory(
            ProcessHandle,
            PTR_ADD_OFFSET(data, 2040), // Magic value from SbReadProcContextByHandle
            Guid,
            sizeof(GUID),
            NULL
            )))
            return status;
    }
    else
    {
        if (!NT_SUCCESS(status = NtReadVirtualMemory(
            ProcessHandle,
            PTR_ADD_OFFSET(data, 32), // Magic value from WdcGetProcessSwitchContext
            Guid,
            sizeof(GUID),
            NULL
            )))
            return status;
    }

    return STATUS_SUCCESS;
}

ULONG GetProcessDpiAwareness(HANDLE QueryHandle)
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static BOOL (WINAPI *getProcessDpiAwarenessInternal)(_In_ HANDLE hprocess,_Out_ ULONG *value);

    if (PhBeginInitOnce(&initOnce))
    {
        getProcessDpiAwarenessInternal = (BOOL (WINAPI *)(_In_ HANDLE hprocess,_Out_ ULONG *value))PhGetDllProcedureAddress(L"user32.dll", "GetProcessDpiAwarenessInternal", 0);
        PhEndInitOnce(&initOnce);
    }

    if (!getProcessDpiAwarenessInternal)
        return 0;

    if (QueryHandle)
    {
        ULONG dpiAwareness;
        if (getProcessDpiAwarenessInternal(QueryHandle, &dpiAwareness))
            return dpiAwareness + 1;
    }
	return 0;
}

VOID PhpWorkaroundWindows10ServiceTypeBug(_Inout_ LPENUM_SERVICE_STATUS_PROCESS ServieEntry)
{
    // https://github.com/processhacker2/processhacker/issues/120 (dmex)
    if (ServieEntry->ServiceStatusProcess.dwServiceType == SERVICE_WIN32)
        ServieEntry->ServiceStatusProcess.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
    if (ServieEntry->ServiceStatusProcess.dwServiceType == (SERVICE_WIN32 | SERVICE_USER_SHARE_PROCESS | SERVICE_USERSERVICE_INSTANCE))
        ServieEntry->ServiceStatusProcess.dwServiceType = SERVICE_USER_SHARE_PROCESS | SERVICE_USERSERVICE_INSTANCE;
}

VOID WeInvertWindowBorder(_In_ HWND hWnd)
{
    RECT rect;
    HDC hdc;

    GetWindowRect(hWnd, &rect);
    hdc = GetWindowDC(hWnd);

    if (hdc)
    {
        ULONG penWidth = GetSystemMetrics(SM_CXBORDER) * 3;
        INT oldDc;
        HPEN pen;
        HBRUSH brush;

        oldDc = SaveDC(hdc);

        // Get an inversion effect.
        SetROP2(hdc, R2_NOT);

        pen = CreatePen(PS_INSIDEFRAME, penWidth, RGB(0x00, 0x00, 0x00));
        SelectObject(hdc, pen);

        brush = (HBRUSH)GetStockObject(NULL_BRUSH);
        SelectObject(hdc, brush);

        // Draw the rectangle.
        Rectangle(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top);

        // Cleanup.
        DeleteObject(pen);

        RestoreDC(hdc, oldDc);
        ReleaseDC(hWnd, hdc);
    }
}


BOOLEAN PhpSelectFavoriteInRegedit(
    _In_ HWND RegeditWindow,
    _In_ PPH_STRINGREF FavoriteName,
    _In_ BOOLEAN UsePhSvc
    )
{
    HMENU menu;
    HMENU favoritesMenu;
    ULONG count;
    ULONG i;
    ULONG id = ULONG_MAX;

    if (!(menu = GetMenu(RegeditWindow)))
        return FALSE;

    // Cause the Registry Editor to refresh the Favorites menu.
    if (UsePhSvc)
        PhSvcCallSendMessage(RegeditWindow, WM_MENUSELECT, MAKEWPARAM(3, MF_POPUP), (LPARAM)menu);
    else
        SendMessage(RegeditWindow, WM_MENUSELECT, MAKEWPARAM(3, MF_POPUP), (LPARAM)menu);

    if (!(favoritesMenu = GetSubMenu(menu, 3)))
        return FALSE;

    // Find our entry.

    count = GetMenuItemCount(favoritesMenu);

    if (count == -1)
        return FALSE;
    if (count > 1000)
        count = 1000;

    for (i = 3; i < count; i++)
    {
        MENUITEMINFO info = { sizeof(MENUITEMINFO) };
        WCHAR buffer[MAX_PATH];

        info.fMask = MIIM_ID | MIIM_STRING;
        info.dwTypeData = buffer;
        info.cch = RTL_NUMBER_OF(buffer);
        GetMenuItemInfo(favoritesMenu, i, TRUE, &info);

        if (info.cch == FavoriteName->Length / sizeof(WCHAR))
        {
            PH_STRINGREF text;

            text.Buffer = buffer;
            text.Length = info.cch * sizeof(WCHAR);

            if (PhEqualStringRef(&text, FavoriteName, TRUE))
            {
                id = info.wID;
                break;
            }
        }
    }

    if (id == ULONG_MAX)
        return FALSE;

    // Activate our entry.
    if (UsePhSvc)
        PhSvcCallSendMessage(RegeditWindow, WM_COMMAND, MAKEWPARAM(id, 0), 0);
    else
        SendMessage(RegeditWindow, WM_COMMAND, MAKEWPARAM(id, 0), 0);

    // "Close" the Favorites menu and restore normal status bar text.
    if (UsePhSvc)
        PhSvcCallPostMessage(RegeditWindow, WM_MENUSELECT, MAKEWPARAM(0, 0xffff), 0);
    else
        PostMessage(RegeditWindow, WM_MENUSELECT, MAKEWPARAM(0, 0xffff), 0);

    // Bring regedit to the top.
    //if (IsMinimized(RegeditWindow))
	if (IsIconic(RegeditWindow))
    {
        ShowWindow(RegeditWindow, SW_RESTORE);
        SetForegroundWindow(RegeditWindow);
    }
    else
    {
        SetForegroundWindow(RegeditWindow);
    }

    return TRUE;
}

/**
 * Opens a key in the Registry Editor. If the Registry Editor is already open,
 * the specified key is selected in the Registry Editor.
 *
 * \param hWnd A handle to the parent window.
 * \param KeyName The key name to open.
 */
BOOLEAN PhShellOpenKey2(
    _In_ HWND hWnd,
    _In_ PPH_STRING KeyName
    )
{
    static PH_STRINGREF favoritesKeyName = PH_STRINGREF_INIT(L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit\\Favorites");

    BOOLEAN result = FALSE;
    HWND regeditWindow;
    HANDLE favoritesKeyHandle;
    WCHAR favoriteName[32];
    UNICODE_STRING valueName;
    PH_STRINGREF valueNameSr;
    PPH_STRING expandedKeyName;

    regeditWindow = FindWindow(L"RegEdit_RegEdit", NULL);

    if (!regeditWindow)
    {
        PhShellOpenKey(hWnd, KeyName);
        return TRUE;
    }

	BOOLEAN UsePhSvc = !PhGetOwnTokenAttributes().Elevated;

    //if (UsePhSvc)
    //{
    //    if (!PhUiConnectToPhSvc(hWnd, FALSE))
    //        return FALSE;
    //}

    // Create our entry in Favorites.

    if (!NT_SUCCESS(PhCreateKey(
        &favoritesKeyHandle,
        KEY_WRITE,
        PH_KEY_CURRENT_USER,
        &favoritesKeyName,
        0,
        0,
        NULL
        )))
        goto CleanupExit;

    memcpy(favoriteName, L"A_ProcessHacker", 15 * sizeof(WCHAR));
    PhGenerateRandomAlphaString(&favoriteName[15], 16);
    RtlInitUnicodeString(&valueName, favoriteName);
    PhUnicodeStringToStringRef(&valueName, &valueNameSr);

    expandedKeyName = PhExpandKeyName(KeyName, FALSE);
    NtSetValueKey(favoritesKeyHandle, &valueName, 0, REG_SZ, expandedKeyName->Buffer, (ULONG)expandedKeyName->Length + sizeof(UNICODE_NULL));
    PhDereferenceObject(expandedKeyName);

    // Select our entry in regedit.
    result = PhpSelectFavoriteInRegedit(regeditWindow, &valueNameSr, UsePhSvc);

    NtDeleteValueKey(favoritesKeyHandle, &valueName);
    NtClose(favoritesKeyHandle);

CleanupExit:
    //if (UsePhSvc)
    //    PhUiDisconnectFromPhSvc();

    return result;
}

#include <shellapi.h>

VOID PhShellExecuteUserString(
    _In_ HWND hWnd,
    _In_ PWSTR Setting,
    _In_ PWSTR String,
    _In_ BOOLEAN UseShellExecute,
    _In_opt_ PWSTR ErrorMessage
    )
{
    static PH_STRINGREF replacementToken = PH_STRINGREF_INIT(L"%s");
    PPH_STRING applicationDirectory;
    PPH_STRING executeString;
    PH_STRINGREF stringBefore;
    PH_STRINGREF stringAfter;
    PPH_STRING ntMessage;

    if (!(applicationDirectory = PhGetApplicationDirectory()))
    {
        PhShowStatus(hWnd, L"Unable to locate the application directory.", STATUS_NOT_FOUND, 0);
        return;
    }

    // Get the execute command.
    executeString = PhGetStringSetting(Setting);

    // Expand environment strings.
    PhMoveReference((PVOID*)&executeString, PhExpandEnvironmentStrings(&executeString->sr));

    // Make sure the user executable string is absolute. We can't use RtlDetermineDosPathNameType_U
    // here because the string may be a URL.
    if (PhFindCharInString(executeString, 0, ':') == -1)
    {
        INT stringArgCount;
        PWSTR* stringArgList;

        // (dmex) HACK: Escape the individual executeString components.
        if ((stringArgList = CommandLineToArgvW(executeString->Buffer, &stringArgCount)) && stringArgCount == 2)
        {
            PPH_STRING fileName = PhCreateString(stringArgList[0]);
            PPH_STRING fileArgs = PhCreateString(stringArgList[1]);

            // Make sure the string is absolute and escape the filename.
            if (RtlDetermineDosPathNameType_U(fileName->Buffer) == RtlPathTypeRelative)
                PhMoveReference((PVOID*)&fileName, PhConcatStrings(4, L"\"", applicationDirectory->Buffer, fileName->Buffer, L"\""));
            else
                PhMoveReference((PVOID*)&fileName, PhConcatStrings(3, L"\"", fileName->Buffer, L"\""));

            // Escape the parameters.
            PhMoveReference((PVOID*)&fileArgs, PhConcatStrings(3, L"\"", fileArgs->Buffer, L"\""));

            // Create the escaped execute string.
            PhMoveReference((PVOID*)&executeString, PhConcatStrings(3, fileName->Buffer, L" ", fileArgs->Buffer));

            PhDereferenceObject(fileArgs);
            PhDereferenceObject(fileName);
            LocalFree(stringArgList);
        }
        else
        {
            if (RtlDetermineDosPathNameType_U(executeString->Buffer) == RtlPathTypeRelative)
                PhMoveReference((PVOID*)&executeString, PhConcatStrings(4, L"\"", applicationDirectory->Buffer, executeString->Buffer, L"\""));
            else
                PhMoveReference((PVOID*)&executeString, PhConcatStrings(3, L"\"", executeString->Buffer, L"\""));
        }
    }

    // Replace the token with the string, or use the original string if the token is not present.
    if (PhSplitStringRefAtString(&executeString->sr, &replacementToken, FALSE, &stringBefore, &stringAfter))
    {
        PPH_STRING stringTemp;
        PPH_STRING stringMiddle;

        // Note: This code is needed to solve issues with faulty RamDisk software that doesn't use the Mount Manager API
        // and instead returns \device\ FileName strings. We also can't change the way the process provider stores 
        // the FileName string since it'll break various features and use-cases required by developers 
        // who need the raw untranslated FileName string.
        stringTemp = PhCreateString(String);
        stringMiddle = PhGetFileName(stringTemp);

        PhMoveReference((PVOID*)&executeString, PhConcatStringRef3(&stringBefore, &stringMiddle->sr, &stringAfter));

        PhDereferenceObject(stringMiddle);
        PhDereferenceObject(stringTemp);
    }

    if (UseShellExecute)
    {
        PhShellExecute(hWnd, executeString->Buffer, NULL);
    }
    else
    {
        NTSTATUS status;

        status = PhCreateProcessWin32(NULL, executeString->Buffer, NULL, NULL, 0, NULL, NULL, NULL);

        if (!NT_SUCCESS(status))
        {
            if (ErrorMessage)
            {
                ntMessage = PhGetNtMessage(status);
                PhShowError2(
                    hWnd,
                    L"Unable to execute the command.",
                    L"%s\n%s",
                    PhGetStringOrDefault(ntMessage, L"An unknown error occurred."),
                    ErrorMessage
                    );
                PhDereferenceObject(ntMessage);
            }
            else
            {
                PhShowStatus(hWnd, L"Unable to execute the command.", status, 0);
            }
        }
    }

    PhDereferenceObject(executeString);
    PhDereferenceObject(applicationDirectory);
}

////////////////////////////////////////////////////////////////////////////////////
// other

NTSTATUS PhSvcCallSendMessage(
	_In_opt_ HWND hWnd,
	_In_ UINT Msg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam,
	_In_ BOOLEAN Post
)
{
	QString SocketName = CTaskService::RunWorker();
	if (SocketName.isEmpty())
		return STATUS_ACCESS_DENIED;

	QVariantMap Parameters;
	Parameters["hWnd"] = (quint64)hWnd;
	Parameters["Msg"] = (quint64)Msg;
	Parameters["wParam"] = (quint64)wParam;
	Parameters["lParam"] = (quint64)lParam;
	Parameters["Post"] = (bool)Post;

	QVariantMap Request;
	Request["Command"] = "SendMessage";
	Request["Parameters"] = Parameters;

	CTaskService::SendCommand(SocketName, Request);

	return STATUS_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////
// syssccpu.c

BOOLEAN PhSipGetCpuFrequencyFromDistribution(PSYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION Current, PSYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION Previous, double* Fraction)
{
    ULONG stateSize;
    PVOID differences;
    PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION stateDistribution;
    PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION stateDifference;
    PSYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT_WIN8 hitcountOld;
    ULONG i;
    ULONG j;
    DOUBLE count;
    DOUBLE total;

    // Calculate the differences from the last performance distribution.

    if (!Current || Current->ProcessorCount != PhSystemBasicInformation.NumberOfProcessors || !Previous || Previous->ProcessorCount != PhSystemBasicInformation.NumberOfProcessors)
        return FALSE;

    stateSize = FIELD_OFFSET(SYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION, States) + sizeof(SYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT) * 2;
    differences = PhAllocate(stateSize * PhSystemBasicInformation.NumberOfProcessors);

    for (i = 0; i < PhSystemBasicInformation.NumberOfProcessors; i++)
    {
        stateDistribution = (PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION)PTR_ADD_OFFSET(Current, Current->Offsets[i]);
        stateDifference = (PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION)PTR_ADD_OFFSET(differences, stateSize * i);

        if (stateDistribution->StateCount != 2)
        {
            PhFree(differences);
            return FALSE;
        }

        for (j = 0; j < stateDistribution->StateCount; j++)
        {
            if (WindowsVersion >= WINDOWS_8_1)
            {
                stateDifference->States[j] = stateDistribution->States[j];
            }
            else
            {
                hitcountOld = (PSYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT_WIN8)PTR_ADD_OFFSET(stateDistribution->States, sizeof(SYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT_WIN8) * j);
                stateDifference->States[j].Hits = hitcountOld->Hits;
                stateDifference->States[j].PercentFrequency = hitcountOld->PercentFrequency;
            }
        }
    }

    for (i = 0; i < PhSystemBasicInformation.NumberOfProcessors; i++)
    {
        stateDistribution = (PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION)PTR_ADD_OFFSET(Previous, Previous->Offsets[i]);
        stateDifference = (PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION)PTR_ADD_OFFSET(differences, stateSize * i);

        if (stateDistribution->StateCount != 2)
        {
            PhFree(differences);
            return FALSE;
        }

        for (j = 0; j < stateDistribution->StateCount; j++)
        {
            if (WindowsVersion >= WINDOWS_8_1)
            {
                stateDifference->States[j].Hits -= stateDistribution->States[j].Hits;
            }
            else
            {
                hitcountOld = (PSYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT_WIN8)PTR_ADD_OFFSET(stateDistribution->States, sizeof(SYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT_WIN8) * j);
                stateDifference->States[j].Hits -= hitcountOld->Hits;
            }
        }
    }

    // Calculate the frequency.

    count = 0;
    total = 0;

    for (i = 0; i < PhSystemBasicInformation.NumberOfProcessors; i++)
    {
        stateDifference = (PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION)PTR_ADD_OFFSET(differences, stateSize * i);

        for (j = 0; j < 2; j++)
        {
            count += stateDifference->States[j].Hits;
            total += stateDifference->States[j].Hits * stateDifference->States[j].PercentFrequency;
        }
    }

    PhFree(differences);

    if (count == 0)
        return FALSE;

    total /= count;
    total /= 100;
    *Fraction = total;

    return TRUE;
}

NTSTATUS PhSipQueryProcessorPerformanceDistribution(_Out_ PVOID *Buffer)
{
    NTSTATUS status;
    PVOID buffer;
    ULONG bufferSize;
    ULONG attempts;

    bufferSize = 0x100;
    buffer = PhAllocate(bufferSize);

    status = NtQuerySystemInformation(SystemProcessorPerformanceDistribution, buffer, bufferSize, &bufferSize);
    attempts = 0;

    while (status == STATUS_INFO_LENGTH_MISMATCH && attempts < 8)
    {
        PhFree(buffer);
        buffer = PhAllocate(bufferSize);

        status = NtQuerySystemInformation(SystemProcessorPerformanceDistribution, buffer, bufferSize, &bufferSize);
        attempts++;
    }

    if (NT_SUCCESS(status))
        *Buffer = buffer;
    else
        PhFree(buffer);

    return status;
}

////////////////////////////////////////////////////////////////////////////////////
// atom table

NTSTATUS EnumAtomTable(
    _Out_ PATOM_TABLE_INFORMATION* AtomTable
    )
{
    NTSTATUS status;
    PVOID buffer;
    ULONG bufferSize = 0x1000;

    buffer = PhAllocate(bufferSize);
    memset(buffer, 0, bufferSize);

    status = NtQueryInformationAtom(
        RTL_ATOM_INVALID_ATOM,
        AtomTableInformation,
        buffer,
        bufferSize,
        &bufferSize // Not used...
        );

    if (!NT_SUCCESS(status))
    {
        PhFree(buffer);
        return status;
    }

    *AtomTable = (PATOM_TABLE_INFORMATION)buffer;

    return status;
}

NTSTATUS QueryAtomTableEntry(
    _In_ RTL_ATOM Atom,
    _Out_ PATOM_BASIC_INFORMATION* AtomInfo
    )
{
    NTSTATUS status;
    PVOID buffer;
    ULONG bufferSize = 0x1000;

    buffer = PhAllocate(bufferSize);
    memset(buffer, 0, bufferSize);

    status = NtQueryInformationAtom(
        Atom,
        AtomBasicInformation,
        buffer,
        bufferSize,
        &bufferSize // Not used...
        );

    if (!NT_SUCCESS(status))
    {
        PhFree(buffer);
        return status;
    }

    *AtomInfo = (PATOM_BASIC_INFORMATION)buffer;

    return status;
}