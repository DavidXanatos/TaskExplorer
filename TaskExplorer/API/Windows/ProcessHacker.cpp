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

