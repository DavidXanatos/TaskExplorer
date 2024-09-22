/*
 * Task Explorer -
 *   qt wrapper and helper functions
 *
 * Copyright (C) 2009-2016 wj32
 * Copyright (C) 2017-2019 dmex
 * Copyright (C) 2019-2023 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 *
 */

#include "stdafx.h"
#include "ProcessHacker.h"
#include <kphmsgdyn.h>
extern "C" {
#include <kphdyndata.h>
}
#include <settings.h>

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
	std::wstring wstr = qString.toStdWString();
	UNICODE_STRING ustr;
	RtlInitUnicodeString(&ustr, (wchar_t*)wstr.c_str());
	return PhCreateStringFromUnicodeString(&ustr);
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

	//if (!NT_SUCCESS(PhInitializePhLibEx(L"Task Explorer", ULONG_MAX, Instance, 0, 0)))
	if (!NT_SUCCESS(PhInitializePhLib(L"Task Explorer", Instance)))
		return 1;
	//if (!PhInitializeExceptionPolicy())
	//	return 1;
	//if (!PhInitializeNamespacePolicy())
	//	return 1;
	//if (!PhInitializeMitigationPolicy())
	//	return 1;
	//if (!PhInitializeRestartPolicy())
	//    return 1;

	KphInitialize();

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

	return 0;
}

bool (*g_KernelProcessMonitor)(quint64 ProcessId, quint64 ParrentId, const QString& FileName, const QString& CommandLine) = NULL;

void (*g_KernelDebugLogger)(const QString& Output) = NULL;

extern "C" {

static VOID NTAPI KsiCommsCallback(
    _In_ ULONG_PTR ReplyToken,
    _In_ PCKPH_MESSAGE Message
    )
{
	//static QMap<void*,int> killMap;
	//static QMutex killMutex;
	switch (Message->Header.MessageId)
	{
		case KphMsgProcessCreate:
		{
			PPH_FREE_LIST freelist = KphGetMessageFreeList();

			PKPH_MESSAGE msg = (PKPH_MESSAGE)PhAllocateFromFreeList(freelist);
			KphMsgInit(msg, KphMsgProcessCreate);
			if (g_KernelProcessMonitor) 
			{
				quint64 ProcessId = (quint64)Message->Kernel.ProcessCreate.TargetProcessId;
				quint64 ParrentId = (quint64)Message->Kernel.ProcessCreate.ParentProcessId;

				QString FileName;
				UNICODE_STRING fileName = { 0 };
				if (NT_SUCCESS(KphMsgDynGetUnicodeString(Message, KphMsgFieldFileName, &fileName))) {
					PPH_STRING oldFileName = PhCreateString(fileName.Buffer);
					PPH_STRING newFileName = PhGetFileName(oldFileName);
					PhDereferenceObject(oldFileName);
					FileName = CastPhString(newFileName);
				}

				QString CommandLine;
				UNICODE_STRING commandLine = { 0 };
				if (NT_SUCCESS(KphMsgDynGetUnicodeString(Message, KphMsgFieldCommandLine, &commandLine)))
					CommandLine = QString::fromWCharArray(commandLine.Buffer, commandLine.Length / sizeof(wchar_t));

				msg->Reply.ProcessCreate.CreationStatus = g_KernelProcessMonitor(ProcessId, ParrentId, FileName, CommandLine) ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
			}
			else
				msg->Reply.ProcessCreate.CreationStatus = STATUS_SUCCESS;
			KphCommsReplyMessage(ReplyToken, msg);

			PhFreeToFreeList(freelist, msg);

			break;
		}
		case KphMsgDebugPrint:
		{
			ANSI_STRING aStr;
			if (NT_SUCCESS(KphMsgDynGetAnsiString(Message, KphMsgFieldOutput, &aStr)))
			{
				if(g_KernelDebugLogger)
					g_KernelDebugLogger(QString::fromLatin1(aStr.Buffer, aStr.Length));
			}
			break;
		}
		/*case KphMsgFilePreCreate:
		{
			PPH_FREE_LIST freelist = KphGetMessageFreeList();

			PKPH_MESSAGE msg = (PKPH_MESSAGE)PhAllocateFromFreeList(freelist);
			KphMsgInit(msg, KphMsgFilePreCreate);
			msg->Reply.File.Pre.Create.Status = STATUS_SUCCESS;

			UNICODE_STRING fileName = { 0 };
			if (NT_SUCCESS(KphMsgDynGetUnicodeString(Message, KphMsgFieldFileName, &fileName))) {
				QString Name = QString::fromWCharArray(fileName.Buffer, fileName.Length / sizeof(WCHAR));
				if (Name.startsWith("\\Device\\ImDisk")) {
					qDebug() << "BAM:" << Name;
					QMutexLocker Lock(&killMutex);
					killMap[Message->Kernel.File.FileObject]++;
				}
			}

			KphCommsReplyMessage(ReplyToken, msg);

			PhFreeToFreeList(freelist, msg);

			break;
		}
		case KphMsgFilePostCreate:
		{
			PPH_FREE_LIST freelist = KphGetMessageFreeList();

			PKPH_MESSAGE msg = (PKPH_MESSAGE)PhAllocateFromFreeList(freelist);
			KphMsgInit(msg, KphMsgFilePostCreate);
			msg->Reply.File.Post.Create.Status = STATUS_SUCCESS;

			{
				QMutexLocker Lock(&killMutex);
				if (killMap[Message->Kernel.File.FileObject])
				{
					msg->Reply.File.Post.Create.Status = STATUS_ACCESS_DENIED;
					killMap[Message->Kernel.File.FileObject]--;
					qDebug() << "BAM: !!!";
				}
			}


			KphCommsReplyMessage(ReplyToken, msg);

			PhFreeToFreeList(freelist, msg);

			break;
		}*/
	}
}

NTSTATUS KsiReadConfiguration(
	_In_ PWSTR FileName,
	_Out_ PBYTE* Data,
	_Out_ PULONG Length
)
{
	NTSTATUS status;
	PPH_STRING fileName;
	HANDLE fileHandle;

	*Data = NULL;
	*Length = 0;

	status = STATUS_NO_SUCH_FILE;

	fileName = PhGetApplicationDirectoryFileNameZ(FileName, TRUE);
	if (fileName)
	{
		if (NT_SUCCESS(status = PhCreateFile(
			&fileHandle,
			&fileName->sr,
			FILE_GENERIC_READ,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
		)))
		{
			status = PhGetFileData(fileHandle, (PVOID*)Data, Length);

			NtClose(fileHandle);
		}

		PhDereferenceObject(fileName);
	}

	return status;
}

/*NTSTATUS KsiValidateDynamicConfiguration(
	_In_ PBYTE DynData,
	_In_ ULONG DynDataLength
)
{
	NTSTATUS status;
	PPH_STRING fileName;
	PVOID versionInfo;
	VS_FIXEDFILEINFO* fileInfo;

	status = STATUS_NO_SUCH_FILE;

	if (fileName = PhGetKernelFileName2())
	{
		if (versionInfo = PhGetFileVersionInfoEx(&fileName->sr))
		{
			if (fileInfo = PhGetFileVersionFixedInfo(versionInfo))
			{
				status = KphDynDataGetConfiguration(
					(PKPH_DYNDATA)DynData,
					DynDataLength,
					HIWORD(fileInfo->dwFileVersionMS),
					LOWORD(fileInfo->dwFileVersionMS),
					HIWORD(fileInfo->dwFileVersionLS),
					LOWORD(fileInfo->dwFileVersionLS),
					NULL
				);
			}

			PhFree(versionInfo);
		}

		PhDereferenceObject(fileName);
	}

	return status;
}*/

NTSTATUS KsiGetDynData(
	_Out_ PBYTE* DynData,
	_Out_ PULONG DynDataLength,
	_Out_ PBYTE* Signature,
	_Out_ PULONG SignatureLength
)
{
	NTSTATUS status;
	PBYTE data = NULL;
	ULONG dataLength;
	//PBYTE sig = NULL;
	//ULONG sigLength;

	// TODO download dynamic data from server

	*DynData = NULL;
	*DynDataLength = 0;
	*Signature = NULL;
	*SignatureLength = 0;

	status = KsiReadConfiguration(L"ksidyn.bin", &data, &dataLength);
	if (!NT_SUCCESS(status))
		goto CleanupExit;

	//status = KsiValidateDynamicConfiguration(data, dataLength);
	//if (!NT_SUCCESS(status))
	//	goto CleanupExit;

	//status = KsiReadConfiguration(L"ksidyn.sig", &sig, &sigLength);
	//if (!NT_SUCCESS(status))
	//	goto CleanupExit;
	//
	//if (!sigLength)
	//{
	//	status = STATUS_SI_DYNDATA_INVALID_SIGNATURE;
	//	goto CleanupExit;
	//}

	*DynDataLength = dataLength;
	*DynData = data;
	data = NULL;

	//*SignatureLength = sigLength;
	//*Signature = sig;
	//sig = NULL;

	status = STATUS_SUCCESS;

CleanupExit:
	if (data)
		PhFree(data);
	//if (sig)
	//	PhFree(sig);

	return status;
}

}

BOOLEAN KsiEnableLoadNative = FALSE;
BOOLEAN KsiEnableLoadFilter = FALSE;

STATUS InitKPH(QString DeviceName, QString FileName)
{
	if (DeviceName.isEmpty())
		DeviceName = QString::fromWCharArray(KPH_SERVICE_NAME);
	if (FileName.isEmpty())
		FileName = "systeminformer.sys";

	// if the file name is not a full path Add the application directory
	if (!FileName.contains("\\"))
		FileName = QApplication::applicationDirPath() + "/" + FileName;

	FileName = FileName.replace("/", "\\");
    if (!QFile::exists(FileName))
		return ERR(QObject::tr("The kernel driver file '%1' was not found.").arg(FileName), STATUS_NOT_FOUND);

    if (!PhGetOwnTokenAttributes().Elevated)
        return ERR("Driver required administrative privileges.", STATUS_ELEVATION_REQUIRED);

	PBYTE dynData = NULL;
	ULONG dynDataLength;
	PBYTE signature = NULL;
	ULONG signatureLength;

	NTSTATUS status = KsiGetDynData(&dynData, &dynDataLength, &signature, &signatureLength);
	if (!NT_SUCCESS(status)) 
		return ERR("Unsupported windows version.", STATUS_UNKNOWN_REVISION);

	// todo: fix-me
    //if (PhDoesOldKsiExist())
    //{
    //    if (PhGetIntegerSetting(L"EnableKphWarnings") && !PhStartupParameters.PhSvc)
    //    {
    //        PhShowKsiError(
    //            L"Unable to load kernel driver, the last System Informer update requires a reboot.",
    //            STATUS_PENDING 
    //            );
    //    }
    //    return;
    //}

	STATUS Status = OK;
	PPH_STRING ksiFileName = NULL;
    PPH_STRING ksiServiceName = NULL;

    //if (!(ksiServiceName = PhCreateString(KPH_SERVICE_NAME)))
	if (!(ksiServiceName = CastQString(DeviceName)))
        goto CleanupExit;
    if (!(ksiFileName = CastQString(FileName)))
        goto CleanupExit;

    {
        KPH_CONFIG_PARAMETERS config = { 0 };
        PPH_STRING objectName = NULL;
        PPH_STRING portName = NULL;
        PPH_STRING altitude = NULL;

        //if (PhIsNullOrEmptyString(objectName = PhGetStringSetting(L"KphObjectName")))
        //PhMoveReference((PVOID*)&objectName, PhCreateString(KPH_OBJECT_NAME));
		objectName = CastQString("\\Driver\\" + DeviceName);
        //if (PhIsNullOrEmptyString(portName = PhGetStringSetting(L"KphPortName")))
        //PhMoveReference((PVOID*)&portName, PhCreateString(KPH_PORT_NAME));
		portName = CastQString("\\" + DeviceName);
        //if (PhIsNullOrEmptyString(altitude = PhGetStringSetting(L"KphAltitude")))
        PhMoveReference((PVOID*)&altitude, PhCreateString(L"385210.5"));

        config.FileName = &ksiFileName->sr;
        config.ServiceName = &ksiServiceName->sr;
        config.ObjectName = &objectName->sr;
        config.PortName = &portName->sr;
        config.Altitude = &altitude->sr;
		config.Flags.Flags = 0;
		//config.Flags.DisableImageLoadProtection = !!PhGetIntegerSetting(L"KsiDisableImageLoadProtection");
		//config.Flags.RandomizedPoolTag = !!PhGetIntegerSetting(L"KsiRandomizedPoolTag");
		//config.Flags.DynDataNoEmbedded = !!PhGetIntegerSetting(L"KsiDynDataNoEmbedded");
        config.Callback = (PKPH_COMMS_CALLBACK)KsiCommsCallback;

		config.EnableNativeLoad = KsiEnableLoadNative;
		config.EnableFilterLoad = KsiEnableLoadFilter;

        status = KphConnect(&config);

        if (NT_SUCCESS(status))
        {
			KphActivateDynData(dynData, dynDataLength, signature, signatureLength);

            KPH_LEVEL level = KsiLevel();

            if (!NtCurrentPeb()->BeingDebugged && (level != KphLevelMax))
            {
                //if ((level == KphLevelHigh) &&
                //    !PhStartupParameters.KphStartupMax)
                //{
                //    PH_STRINGREF commandline = PH_STRINGREF_INIT(L" -kx");
                //    PhRestartSelf(&commandline);
                //}

                //if ((level < KphLevelHigh) &&
                //    !PhStartupParameters.KphStartupMax &&
                //    !PhStartupParameters.KphStartupHigh)
                //{
                //    PH_STRINGREF commandline = PH_STRINGREF_INIT(L" -kh");
                //    PhRestartSelf(&commandline);
                //}
            }

			/*KPH_INFORMER_SETTINGS filter;

			filter.Flags = 0;
			filter.Flags2 = 0;
			filter.Flags3 = 0;
			filter.ProcessCreate = FALSE;
			filter.FilePreCreate = TRUE;
			filter.FilePostCreate = TRUE;
			filter.FileEnablePostCreateReply = TRUE;

			KphSetInformerSettings(&filter);*/
        }
        else
        {
			Status = ERR("Unable to load the kernel driver service.", status);
        }

        PhClearReference((PVOID*)&objectName);
    }

CleanupExit:
    if (ksiServiceName)
        PhDereferenceObject(ksiServiceName);
    if (ksiFileName)
        PhDereferenceObject(ksiFileName);
	if (signature)
		PhFree(signature);
	if (dynData)
		PhFree(dynData);

	return Status;
}

//NTSTATUS PhCleanupKsi(VOID)
//{
//	NTSTATUS status;
//	PPH_STRING ksiServiceName;
//	KPH_CONFIG_PARAMETERS config = { 0 };
//	BOOLEAN shouldUnload;
//
//	if (!KphCommsIsConnected())
//		return STATUS_SUCCESS;
//
//	if (PhGetIntegerSetting(L"KsiEnableUnloadProtection"))
//		KphReleaseDriverUnloadProtection(NULL, NULL);
//
//	if (PhGetIntegerSetting(L"KsiUnloadOnExit"))
//	{
//		ULONG clientCount;
//
//		if (!NT_SUCCESS(status = KphGetConnectedClientCount(&clientCount)))
//			return status;
//
//		shouldUnload = (clientCount == 1);
//	}
//	else
//	{
//		shouldUnload = FALSE;
//	}
//
//	KphCommsStop();
//
//#ifdef DEBUG
//	KsiDebugLogDestroy();
//#endif
//
//	if (!shouldUnload)
//		return STATUS_SUCCESS;
//
//	if (!(ksiServiceName = PhGetKsiServiceName()))
//		return STATUS_UNSUCCESSFUL;
//
//	config.ServiceName = &ksiServiceName->sr;
//	config.EnableNativeLoad = KsiEnableLoadNative;
//	config.EnableFilterLoad = KsiEnableLoadFilter;
//	status = KphServiceStop(&config);
//
//	return status;
//}

bool KphSetDebugLog(bool Enable)
{
	NTSTATUS status;
	KPH_INFORMER_SETTINGS Settings;
	if (NT_SUCCESS(status = KphGetInformerSettings(&Settings))) {
		Settings.DebugPrint = Enable;
		status = KphSetInformerSettings(&Settings);
	}
	return NT_SUCCESS(status);
}

bool KphSetSystemMon(bool Enable)
{
	if (!KphCommsIsConnected())
		return false;

	NTSTATUS status;
	KPH_INFORMER_SETTINGS Settings;
	if (NT_SUCCESS(status = KphGetInformerSettings(&Settings))) {
		Settings.ProcessCreate = Enable;
		status = KphSetInformerSettings(&Settings);
	}
	return NT_SUCCESS(status);
}

bool KphGetSystemMon()
{
	if (!KphCommsIsConnected())
		return false;

	KPH_INFORMER_SETTINGS Settings;
	if (NT_SUCCESS(KphGetInformerSettings(&Settings)))
		return Settings.ProcessCreate;
	return false;
}

void PhShowAbout(QWidget* parent)
{
		QString AboutCaption = QString(
			"<h3>System Informer</h3>"
			"<p>Licensed Under the MIT License</p>"
			"<p>Copyright (c) 2022</p>"
		);
		QString AboutText = QString(
                "<p>Thanks to:<br>"
                "    <a href=\"https://github.com/wj32\">wj32</a> - Wen Jia Liu<br>"
                "    <a href=\"https://github.com/dmex\">dmex</a> - Steven G<br>"
                "    <a href=\"https://github.com/jxy-s\">jxy-s</a> - Johnny Shaw<br>"
                "    <a href=\"https://github.com/ionescu007\">ionescu007</a> - Alex Ionescu\n"
                "    <a href=\"https://github.com/yardenshafir\">yardenshafir</a> - Yarden Shafir<br>"
                "    <a href=\"https://github.com/winsiderss/systeminformer/graphs/contributors\">Contributors</a> - thank you for your additions!<br>"
                "    Donors - thank you for your support!</p>"
                "<p>System Informer uses the following components:<br>"
                "    <a href=\"https://github.com/michaelrsweet/mxml\">Mini-XML</a> by Michael Sweet<br>"
                "    <a href=\"https://www.pcre.org\">PCRE</a><br>"
                "    <a href=\"https://github.com/json-c/json-c\">json-c</a><br>"
                "    MD5 code by Jouni Malinen<br>"
                "    SHA1 code by Filip Navara, based on code by Steve Reid<br>"
                "    <a href=\"http://www.famfamfam.com/lab/icons/silk\">Silk icons</a><br>"
                "    <a href=\"https://www.fatcow.com/free-icons\">Farm-fresh web icons</a><br></p>"
			"<p></p>"
			"<p>Visit <a href=\"https://github.com/winsiderss/systeminformer\">System Informer on github</a> for more information.</p>"
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

