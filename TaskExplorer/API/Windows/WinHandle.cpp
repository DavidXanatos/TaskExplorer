/*
 * Task Explorer -
 *   qt wrapper and support functions based on hndlprv.c
 *
 * Copyright (C) 2010-2015 wj32
 * Copyright (C) 2017 dmex
 * Copyright (C) 2019-2022 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 * 
 */

#include "stdafx.h"
#include "WinHandle.h"
#include "ProcessHacker.h"
#include "WindowsAPI.h"
#include "../../../MiscHelpers/Common/Settings.h"

CWinHandle::CWinHandle(QObject *parent) 
	: CHandleInfo(parent) 
{
	m_Object = -1;
	m_Attributes = 0;
	m_GrantedAccess = 0;
	m_TypeIndex = 0;
	m_FileFlags = 0;
}

CWinHandle::~CWinHandle()
{
}

quint64 CWinHandle::MakeID(quint64 HandleValue, quint64 UniqueProcessId)
{
	quint64 HandleID = HandleValue;
	HandleID ^= (UniqueProcessId << 32);
	HandleID ^= (UniqueProcessId >> 32);
	return HandleID;
}

bool CWinHandle::InitStaticData(struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* handle, quint64 TimeStamp)
{
	QWriteLocker Locker(&m_Mutex);

	m_ProcessId = handle->UniqueProcessId;
	m_HandleId = handle->HandleValue;
	m_Object = (quint64)handle->Object;
	m_Attributes = handle->HandleAttributes;
	m_GrantedAccess = handle->GrantedAccess;
	m_TypeIndex = handle->ObjectTypeIndex;

	m_CreateTimeStamp = TimeStamp;
	m_RemoveTimeStamp = 0; // handles can be reused

	return true;
}

bool CWinHandle::InitExtData(struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* handle, quint64 ProcessHandle, bool bFull)
{
	QWriteLocker Locker(&m_Mutex);

	PPH_STRING TypeName;
    PPH_STRING ObjectName; // Original Name
    PPH_STRING BestObjectName; // File Name

	if (!NT_SUCCESS(PhGetHandleInformationEx((HANDLE)ProcessHandle, (HANDLE)handle->HandleValue, handle->ObjectTypeIndex, 0, NULL, NULL, &TypeName, &ObjectName, &BestObjectName, NULL)))
		return false;

	if (bFull)
	{
		// HACK: Some security products block NtQueryObject with ObjectTypeInformation and return an invalid type
		// so we need to lookup the TypeName using the TypeIndex. We should improve PhGetHandleInformationEx for this case
		// but for now we'll preserve backwards compat by doing the lookup here. (dmex)
		if (PhIsNullOrEmptyString(TypeName))
		{
			PPH_STRING typeName;

			if (typeName = PhGetObjectTypeName(m_TypeIndex))
			{
				PhMoveReference((PVOID*)&TypeName, typeName);
			}
		}

		if (TypeName && PhEqualString2(TypeName, L"File", TRUE) && KphCommsIsConnected())
		{
			KPH_FILE_OBJECT_INFORMATION objectInfo;

			if (NT_SUCCESS(KphQueryInformationObject((HANDLE)ProcessHandle, (HANDLE)handle->HandleValue, KphObjectFileObjectInformation, &objectInfo, sizeof(KPH_FILE_OBJECT_INFORMATION), NULL)))
			{
				if (objectInfo.SharedRead)
					m_FileFlags |= PH_HANDLE_FILE_SHARED_READ;
				if (objectInfo.SharedWrite)
					m_FileFlags |= PH_HANDLE_FILE_SHARED_WRITE;
				if (objectInfo.SharedDelete)
					m_FileFlags |= PH_HANDLE_FILE_SHARED_DELETE;
			}
		}
	}

	m_TypeName = CastPhString(TypeName);
    m_OriginalName = CastPhString(ObjectName);
    m_FileName = CastPhString(BestObjectName);

	return true;
}

QFutureWatcher<bool>* CWinHandle::InitExtDataAsync(struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* handle, quint64 ProcessHandle)
{
	QFutureWatcher<bool>* pWatcher = new QFutureWatcher<bool>(this);
	pWatcher->setFuture(QtConcurrent::run([this, handle, ProcessHandle]() {
		return CWinHandle::InitExtDataAsync(this, handle, ProcessHandle);
	}));
	return pWatcher;
}

bool CWinHandle::InitExtDataAsync(CWinHandle* This, struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* handle, quint64 ProcessHandle)
{
	return This->InitExtData(handle, ProcessHandle, false);
}

bool CWinHandle__UpdateFileData(HANDLE ProcessHandle, HANDLE HandleId, QString& SubTypeName, quint64& FileSize, quint64& FilePosition, quint32* FileMode = NULL, int* FileType = NULL)
{
	NTSTATUS status;
	IO_STATUS_BLOCK isb;

	HANDLE fileHandle = NULL;

	FILE_FS_DEVICE_INFORMATION fileDeviceInfo;
	if (!KphCommsIsConnected() || !NT_SUCCESS(status = KphQueryVolumeInformationFile(ProcessHandle, HandleId, FileFsDeviceInformation, &fileDeviceInfo, sizeof(FILE_FS_DEVICE_INFORMATION), &isb)))
	{
		if (!NT_SUCCESS(status = NtDuplicateObject(ProcessHandle, HandleId, NtCurrentProcess(), &fileHandle, MAXIMUM_ALLOWED, 0, 0)))
			return false;
	
		status = NtQueryVolumeInformationFile(fileHandle, &isb, &fileDeviceInfo, sizeof(FILE_FS_DEVICE_INFORMATION), FileFsDeviceInformation);
	}

    BOOLEAN isFileOrDirectory = FALSE;
    BOOLEAN isConsoleHandle = FALSE;
	BOOLEAN isPipeHandle = FALSE;
    BOOLEAN isNetworkHandle = FALSE;

	if (NT_SUCCESS(status))
	{
		switch (fileDeviceInfo.DeviceType)
		{
		case FILE_DEVICE_NAMED_PIPE:
			isPipeHandle = TRUE;
			SubTypeName = "Pipe";
			break;
		case FILE_DEVICE_NETWORK:
			isNetworkHandle = TRUE;
			SubTypeName = "Network";
			break;
		case FILE_DEVICE_CD_ROM:
		case FILE_DEVICE_CD_ROM_FILE_SYSTEM:
		case FILE_DEVICE_CONTROLLER:
		case FILE_DEVICE_DATALINK:
		case FILE_DEVICE_DFS:
		case FILE_DEVICE_DISK:
		case FILE_DEVICE_DISK_FILE_SYSTEM:
		case FILE_DEVICE_VIRTUAL_DISK:
			isFileOrDirectory = TRUE;
			SubTypeName = "File or directory";
			break;
		case FILE_DEVICE_CONSOLE:
			isConsoleHandle = TRUE;
			SubTypeName = "Console";
			break;
		default:
			SubTypeName = "Other";
			break;
		}
	}

	if (FileMode)
	{
		// Note: These devices deadlock without a timeout (dmex)
        // 1) Named pipes
        // 2) \Device\ConDrv\CurrentIn
        // 3) \Device\VolMgrControl

		FILE_MODE_INFORMATION fileModeInfo;
		if (NT_SUCCESS(status = ((fileHandle != NULL)
			? PhCallNtQueryFileInformationWithTimeout(fileHandle, FileModeInformation, &fileModeInfo, sizeof(FILE_MODE_INFORMATION))
			: PhCallKphQueryFileInformationWithTimeout(ProcessHandle, HandleId, FileModeInformation, &fileModeInfo, sizeof(FILE_MODE_INFORMATION))
			)))
		{
			*FileMode = fileModeInfo.Mode;
		}
	}

	// NOTE: NtQueryInformationFile can hang on windows 7 at \Device\VolMgrControl (DX)
    if (isFileOrDirectory)
	// if(!isConsoleHandle)
    {
		FILE_STANDARD_INFORMATION fileStandardInfo;
        //if (NT_SUCCESS(NtQueryInformationFile(fileHandle, &isb, &fileStandardInfo, sizeof(FILE_STANDARD_INFORMATION), FileStandardInformation)))
		if (NT_SUCCESS(status = ((fileHandle != NULL)
			? PhCallNtQueryFileInformationWithTimeout(fileHandle, FileStandardInformation, &fileStandardInfo, sizeof(FILE_STANDARD_INFORMATION))
			: PhCallKphQueryFileInformationWithTimeout(ProcessHandle, HandleId, FileStandardInformation, &fileStandardInfo, sizeof(FILE_STANDARD_INFORMATION))
			)))
        {
			if (FileType) *FileType = fileStandardInfo.Directory ? 2 : 1;

			SubTypeName = fileStandardInfo.Directory ? "Directory" : "File";

			FileSize = fileStandardInfo.EndOfFile.QuadPart;
        }

		FILE_POSITION_INFORMATION filePositionInfo;
		//if (NT_SUCCESS(NtQueryInformationFile(fileHandle, &isb, &filePositionInfo, sizeof(FILE_POSITION_INFORMATION), FilePositionInformation)))
		if (NT_SUCCESS(status = ((fileHandle != NULL)
			? PhCallNtQueryFileInformationWithTimeout(fileHandle, FilePositionInformation, &filePositionInfo, sizeof(FILE_POSITION_INFORMATION))
			: PhCallKphQueryFileInformationWithTimeout(ProcessHandle, HandleId, FilePositionInformation, &filePositionInfo, sizeof(FILE_POSITION_INFORMATION))
			)))
		{
			FilePosition = filePositionInfo.CurrentByteOffset.QuadPart;
		}
    }
	
	if(fileHandle) 
		NtClose(fileHandle);
	
	return true;
}

bool CWinHandle::UpdateDynamicData(struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* handle, quint64 ProcessHandle)
{
	QWriteLocker Locker(&m_Mutex);

	BOOLEAN modified = FALSE;

	if (m_Attributes != handle->HandleAttributes)
	{
		m_Attributes = handle->HandleAttributes;
		modified = TRUE;
	}

	if (m_TypeName == "File")
    {
		QString SubTypeName;
		quint64 FileSize = 0;
		quint64 FilePosition = 0;

		CWinHandle__UpdateFileData((HANDLE)ProcessHandle, (HANDLE)m_HandleId, SubTypeName, FileSize, FilePosition);

		if (m_SubTypeName != SubTypeName)
		{
			m_SubTypeName = SubTypeName;
			modified = TRUE;
		}
		if (m_Size != FileSize)
		{
			m_Size = FileSize;
			modified = TRUE;
		}
		if (m_Position != FilePosition)
		{
			m_Position = FilePosition;
			modified = TRUE;
		}
    }
	else if(m_TypeName == "Section")
	{
		HANDLE sectionHandle;
		NTSTATUS status = NtDuplicateObject((HANDLE)ProcessHandle, (HANDLE)m_HandleId, NtCurrentProcess(), &sectionHandle, SECTION_QUERY | SECTION_MAP_READ, 0, 0 );
		if (!NT_SUCCESS(status))
			status = NtDuplicateObject((HANDLE)ProcessHandle, (HANDLE)m_HandleId, NtCurrentProcess(), &sectionHandle, SECTION_QUERY, 0, 0);

		if (NT_SUCCESS(status))
		{
			quint64 SectionSize = 0;

			SECTION_BASIC_INFORMATION sectionInfo;
			if (NT_SUCCESS(PhGetSectionBasicInformation(sectionHandle, &sectionInfo)))
			{
				SectionSize = sectionInfo.MaximumSize.QuadPart;
			}

			if (m_Size != SectionSize)
			{
				m_Size = SectionSize;
				modified = TRUE;
			}

			NtClose(sectionHandle);
		}
	}


	return modified;
}

/**
 * Enumerates all handles in a process.
 *
 * \param ProcessId The ID of the process.
 * \param ProcessHandle A handle to the process.
 * \param Handles A variable which receives a pointer to a buffer containing
 * information about the handles.
 * \param FilterNeeded A variable which receives a boolean indicating
 * whether the handle information needs to be filtered by process ID.
 */
NTSTATUS PhEnumHandlesGeneric(
	_In_ HANDLE ProcessId,
	_In_ HANDLE ProcessHandle,
	_Out_ PSYSTEM_HANDLE_INFORMATION_EX *Handles,
	_Out_ PBOOLEAN FilterNeeded
)
{
	NTSTATUS status;

	// There are three ways of enumerating handles:
	// * On Windows 8 and later, NtQueryInformationProcess with ProcessHandleInformation is the most efficient method.
	// * On Windows XP and later, NtQuerySystemInformation with SystemExtendedHandleInformation.
	// * Otherwise, NtQuerySystemInformation with SystemHandleInformation can be used.

	if (KphCommsIsConnected())
	{
		PKPH_PROCESS_HANDLE_INFORMATION handles;
		PSYSTEM_HANDLE_INFORMATION_EX convertedHandles;
		ULONG i;

		// Enumerate handles using KProcessHacker. Unlike with NtQuerySystemInformation,
		// this only enumerates handles for a single process and saves a lot of processing.

		if (!NT_SUCCESS(status = KsiEnumerateProcessHandles(ProcessHandle, &handles)))
			goto FAILED;

		convertedHandles = (PSYSTEM_HANDLE_INFORMATION_EX)PhAllocate(
			FIELD_OFFSET(SYSTEM_HANDLE_INFORMATION_EX, Handles) +
			sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX) * handles->HandleCount
		);

		convertedHandles->NumberOfHandles = handles->HandleCount;

		for (i = 0; i < handles->HandleCount; i++)
		{
			convertedHandles->Handles[i].Object = handles->Handles[i].Object;
			convertedHandles->Handles[i].UniqueProcessId = (ULONG_PTR)ProcessId;
			convertedHandles->Handles[i].HandleValue = (ULONG_PTR)handles->Handles[i].Handle;
			convertedHandles->Handles[i].GrantedAccess = (ULONG)handles->Handles[i].GrantedAccess;
			convertedHandles->Handles[i].CreatorBackTraceIndex = 0;
			convertedHandles->Handles[i].ObjectTypeIndex = handles->Handles[i].ObjectTypeIndex;
			convertedHandles->Handles[i].HandleAttributes = handles->Handles[i].HandleAttributes;
		}

		PhFree(handles);

		*Handles = convertedHandles;
		*FilterNeeded = FALSE;
	}
	else if (WindowsVersion >= WINDOWS_8 && theConf->GetBool("Options/EnableHandleSnapshot", true))
	{
		PPROCESS_HANDLE_SNAPSHOT_INFORMATION handles;
		PSYSTEM_HANDLE_INFORMATION_EX convertedHandles;
		ULONG i;

		if (!NT_SUCCESS(status = PhEnumHandlesEx2(ProcessHandle, &handles)))
			goto FAILED;

		convertedHandles = (PSYSTEM_HANDLE_INFORMATION_EX)PhAllocate(
			FIELD_OFFSET(SYSTEM_HANDLE_INFORMATION_EX, Handles) +
			sizeof(SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX) * handles->NumberOfHandles
		);

		convertedHandles->NumberOfHandles = handles->NumberOfHandles;

		for (i = 0; i < handles->NumberOfHandles; i++)
		{
			convertedHandles->Handles[i].Object = 0;
			convertedHandles->Handles[i].UniqueProcessId = (ULONG_PTR)ProcessId;
			convertedHandles->Handles[i].HandleValue = (ULONG_PTR)handles->Handles[i].HandleValue;
			convertedHandles->Handles[i].GrantedAccess = handles->Handles[i].GrantedAccess;
			convertedHandles->Handles[i].CreatorBackTraceIndex = 0;
			convertedHandles->Handles[i].ObjectTypeIndex = (USHORT)handles->Handles[i].ObjectTypeIndex;
			convertedHandles->Handles[i].HandleAttributes = handles->Handles[i].HandleAttributes;
		}

		PhFree(handles);

		*Handles = convertedHandles;
		*FilterNeeded = FALSE;
	}
	else
	{
		PSYSTEM_HANDLE_INFORMATION_EX handles;
	FAILED:
		if (!NT_SUCCESS(status = PhEnumHandlesEx(&handles)))
			return status;

		*Handles = handles;
		*FilterNeeded = TRUE;
	}

	return status;
}

QString CWinHandle::GetAttributesString() const
{
	QReadLocker Locker(&m_Mutex);

    switch (m_Attributes & (OBJ_PROTECT_CLOSE | OBJ_INHERIT))
    {
	case OBJ_PROTECT_CLOSE:					return tr("Protected");
    case OBJ_INHERIT:						return tr("Inherit");
    case OBJ_PROTECT_CLOSE | OBJ_INHERIT:	return tr("Protected, Inherit");
    }
	return "";
}

STATUS CWinHandle::SetAttribute(quint32 Attribute, bool bSet)
{
	QWriteLocker Locker(&m_Mutex);

    if (!KphCommsIsConnected())
		return ERR(tr("KProcessHacker is not available"));

	if(bSet)
		m_Attributes |= Attribute;
	else
		m_Attributes ^= Attribute;

	NTSTATUS status;
    HANDLE processHandle;
    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION, (HANDLE)m_ProcessId)))
    {
        OBJECT_HANDLE_FLAG_INFORMATION handleFlagInfo;

        handleFlagInfo.Inherit = !!(m_Attributes & OBJ_INHERIT);
        handleFlagInfo.ProtectFromClose = !!(m_Attributes & OBJ_PROTECT_CLOSE);

        status = KphSetInformationObject(processHandle, (HANDLE)m_HandleId, KphObjectHandleFlagInformation, &handleFlagInfo, sizeof(OBJECT_HANDLE_FLAG_INFORMATION));

        NtClose(processHandle);
    }

    if (!NT_SUCCESS(status))
    {
		return ERR(tr("Failed to set handle attribute"));
    }

    return OK;
}

bool CWinHandle::IsProtected() const
{
	return (GetAttributes() & OBJ_PROTECT_CLOSE) != 0;
}

STATUS CWinHandle::SetProtected(bool bSet) 
{
	return SetAttribute(OBJ_PROTECT_CLOSE, bSet);
}

bool CWinHandle::IsInherited() const
{
	return (GetAttributes() & OBJ_INHERIT) != 0;
}

STATUS CWinHandle::SetInherited(bool bSet)
{
	return SetAttribute(OBJ_INHERIT, bSet);
}

QString CWinHandle::GetFileShareAccessString() const
{
	QReadLocker Locker(&m_Mutex);

	QString Str = "---";
    if (m_FileFlags & PH_HANDLE_FILE_SHARED_MASK)
    {
		if (m_FileFlags & PH_HANDLE_FILE_SHARED_READ)
			Str[0] = 'R';
        if (m_FileFlags & PH_HANDLE_FILE_SHARED_WRITE)
            Str[1] = 'W';
        if (m_FileFlags & PH_HANDLE_FILE_SHARED_DELETE)
            Str[2] = 'D';
    }
	return Str;
}

QString CWinHandle::GetTypeString() const
{ 
	QReadLocker Locker(&m_Mutex); 
	if (m_SubTypeName.isEmpty())
		return m_TypeName;
	return m_TypeName + " (" + m_SubTypeName + ")"; 
}

QString CWinHandle::GetGrantedAccessString() const
{
	QReadLocker Locker(&m_Mutex);

	PPH_STRING GrantedAccessSymbolicText = NULL;
	PPH_ACCESS_ENTRY accessEntries;
	ULONG numberOfAccessEntries;
	PPH_STRING TypeName = CastQString(m_TypeName);
	if (PhGetAccessEntries(PhGetStringOrEmpty(TypeName), &accessEntries, &numberOfAccessEntries))
	{
		GrantedAccessSymbolicText = PhGetAccessString(m_GrantedAccess, accessEntries, numberOfAccessEntries);
		PhFree(accessEntries);
	}
	if(TypeName)
		PhDereferenceObject(TypeName);

	return CastPhString(GrantedAccessSymbolicText);
}

#define PH_FILEMODE_ASYNC 0x01000000
#define PhFileModeUpdAsyncFlag(mode) (mode & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT) ? mode &~ PH_FILEMODE_ASYNC: mode | PH_FILEMODE_ASYNC)

PH_ACCESS_ENTRY FileModeAccessEntries[6] = 
{
    { L"FILE_FLAG_OVERLAPPED", PH_FILEMODE_ASYNC, FALSE, FALSE, L"Asynchronous" },
    { L"FILE_FLAG_WRITE_THROUGH", FILE_WRITE_THROUGH, FALSE, FALSE, L"Write through" },
    { L"FILE_FLAG_SEQUENTIAL_SCAN", FILE_SEQUENTIAL_ONLY, FALSE, FALSE, L"Sequental" },
    { L"FILE_FLAG_NO_BUFFERING", FILE_NO_INTERMEDIATE_BUFFERING, FALSE, FALSE, L"No buffering" },
    { L"FILE_SYNCHRONOUS_IO_ALERT", FILE_SYNCHRONOUS_IO_ALERT, FALSE, FALSE, L"Synchronous alert" },
    { L"FILE_SYNCHRONOUS_IO_NONALERT", FILE_SYNCHRONOUS_IO_NONALERT, FALSE, FALSE, L"Synchronous non-alert" },
};

QString CWinHandle::GetFileAccessMode(quint32 Mode)
{
	// Since FILE_MODE_INFORMATION has no flag for asynchronous I/O we should use our own flag and set
	// it only if none of synchronous flags are present. That's why we need PhFileModeUpdAsyncFlag.
	PPH_STRING fileModeAccessStr = PhGetAccessString(PhFileModeUpdAsyncFlag(Mode), FileModeAccessEntries, RTL_NUMBER_OF(FileModeAccessEntries) );

	return QString("0x%1 (%2)").arg(Mode, 0, 16).arg(CastPhString(fileModeAccessStr));
}

QString CWinHandle::GetSectionType(quint32 Attribs)
{
    if (Attribs & SEC_COMMIT)
        return tr("Commit");
    else if (Attribs & SEC_FILE)
        return tr("File");
    else if (Attribs & SEC_IMAGE)
        return tr("Module");
    else if (Attribs & SEC_RESERVE)
        return tr("Reserve");
	return tr("Unknown");
};

VOID PhLoadSymbolProviderOptions(_Inout_ PPH_SYMBOL_PROVIDER SymbolProvider);

BOOLEAN NTAPI EnumGenericModulesCallback(_In_ PPH_MODULE_INFO Module, _In_opt_ PVOID Context)
{
	if (Module->Type == PH_MODULE_TYPE_MODULE || Module->Type == PH_MODULE_TYPE_WOW64_MODULE) {
		PPH_STRING Name = PhCreateString(Module->FileName->Buffer);
		PhLoadModuleSymbolProvider((PPH_SYMBOL_PROVIDER)Context, Name, (ULONG64)Module->BaseAddress, Module->Size);
		PhDereferenceObject(Name);
	}
    return TRUE;
}


QVariantMap CWinHandle::GetHandleInfo() const
{
	QReadLocker Locker(&m_Mutex); 

	QVariantMap HandleInfo;

    HANDLE processHandle;
	if (!NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_DUP_HANDLE, (HANDLE)m_ProcessId)))
		return HandleInfo;
	
	OBJECT_BASIC_INFORMATION basicInfo;
	if (NT_SUCCESS(PhGetHandleInformation(processHandle, (HANDLE)m_HandleId, ULONG_MAX, &basicInfo, NULL, NULL, NULL)))
	{
		HandleInfo["References"] = (quint64)basicInfo.PointerCount;
		HandleInfo["Handles"] = (quint64)basicInfo.HandleCount;

		HandleInfo["Paged"] = (quint64)basicInfo.PagedPoolCharge;
		HandleInfo["VirtualSize"] = (quint64)basicInfo.NonPagedPoolCharge;
	}


	if(m_TypeName == "ALPC Port")
	{
        //
        // TODO this path doesn't use all the ALPC info returned yet
        // see: KPH_ALPC_BASIC_INFORMATION.State
        //
        KPH_ALPC_BASIC_INFORMATION kphAlpcInfo;
        if (KphCommsIsConnected() && NT_SUCCESS(KphAlpcQueryInformation(processHandle, (HANDLE)m_HandleId, KphAlpcBasicInformation, &kphAlpcInfo, sizeof(kphAlpcInfo), NULL)))
        {
			HandleInfo["Flags"] = (quint32)kphAlpcInfo.Flags;
			HandleInfo["SeqNumber"] = (quint64)kphAlpcInfo.SequenceNo;
			HandleInfo["Context"] = (quint64)kphAlpcInfo.PortContext;

			PKPH_ALPC_COMMUNICATION_NAMES_INFORMATION connectionNames;
			if (!NT_SUCCESS(KphAlpcQueryComminicationsNamesInfo(processHandle, (HANDLE)m_HandleId, &connectionNames)))
			{
				connectionNames = NULL;
			}

			KPH_ALPC_COMMUNICATION_INFORMATION connectionInfo;
			if (NT_SUCCESS(KphAlpcQueryInformation(processHandle, (HANDLE)m_HandleId, KphAlpcCommunicationInformation, &connectionInfo, sizeof(connectionInfo), NULL)))
			{
				if (connectionInfo.ConnectionPort.OwnerProcessId)
				{
					HandleInfo["ConnectionPID"] = (quint64)connectionInfo.ConnectionPort.OwnerProcessId;

					if (connectionNames && connectionNames->ConnectionPort.Length > 0)
						HandleInfo["ConnectionPort"] = QString::fromWCharArray(connectionNames->ConnectionPort.Buffer, connectionNames->ConnectionPort.Length / sizeof(WCHAR));
				}

				if (connectionInfo.ServerCommunicationPort.OwnerProcessId)
				{
					HandleInfo["ServerComPID"] = (quint64)connectionInfo.ServerCommunicationPort.OwnerProcessId;

					if (connectionNames && connectionNames->ServerCommunicationPort.Length > 0)
						HandleInfo["ServerComPort"] = QString::fromWCharArray(connectionNames->ServerCommunicationPort.Buffer, connectionNames->ServerCommunicationPort.Length / sizeof(WCHAR));
				}

				if (connectionInfo.ClientCommunicationPort.OwnerProcessId)
				{
					HandleInfo["ClientComPID"] = (quint64)connectionInfo.ClientCommunicationPort.OwnerProcessId;

					if (connectionNames && connectionNames->ClientCommunicationPort.Length > 0)
						HandleInfo["ClientComPort"] = QString::fromWCharArray(connectionNames->ClientCommunicationPort.Buffer, connectionNames->ClientCommunicationPort.Length / sizeof(WCHAR));
				}

				if (connectionNames)
					PhFree(connectionNames);
			}
        }
		else
		{
			HANDLE alpcPortHandle;
			if (NT_SUCCESS(NtDuplicateObject(processHandle, (HANDLE)m_HandleId, NtCurrentProcess(), &alpcPortHandle, READ_CONTROL, 0, 0)))
			{
				ALPC_BASIC_INFORMATION alpcInfo;
				if (NT_SUCCESS(NtAlpcQueryInformation(alpcPortHandle, AlpcBasicInformation, &alpcInfo, sizeof(ALPC_BASIC_INFORMATION), NULL)))
				{
					HandleInfo["Flags"] = (quint32)alpcInfo.Flags;
					HandleInfo["SeqNumber"] = (quint64)alpcInfo.SequenceNo;
					HandleInfo["Context"] = (quint64)alpcInfo.PortContext;
				}

				//if (WindowsVersion >= WINDOWS_10_19H2)
				//{
				//	ALPC_SERVER_SESSION_INFORMATION serverInfo;
				//
				//	if (NT_SUCCESS(NtAlpcQueryInformation(alpcPortHandle, AlpcServerSessionInformation, &serverInfo, sizeof(ALPC_SERVER_SESSION_INFORMATION), NULL)))
				//	{
				//		HandleInfo["SessionId"] = (quint64)serverInfo.SessionId;
				//		HandleInfo["ProcessId"] = (quint64)serverInfo.ProcessId;
				//	}
				//}

				NtClose(alpcPortHandle);
			}
		}
	}
	else if(m_TypeName == "File")
	{
		QString SubTypeName;
		quint64 FileSize = 0;
		quint64 FilePosition = 0;
		quint32 FileMode = 0;
		int FileType = 0;

		CWinHandle__UpdateFileData(processHandle, (HANDLE)m_HandleId, SubTypeName, FileSize, FilePosition, &FileMode, &FileType);
		
		HandleInfo["Mode"] = FileMode;

		if (FileType != 0)
		{
			HandleInfo["IsDir"] = FileType == 2;
			HandleInfo["Size"] = FileSize;
			HandleInfo["Position"] = FilePosition;
		}

		KPH_FILE_OBJECT_DRIVER fileObjectDriver;
		if (KphCommsIsConnected() && NT_SUCCESS(KphQueryInformationObject(processHandle, (HANDLE)m_HandleId, KphObjectFileObjectDriver, &fileObjectDriver, sizeof(fileObjectDriver), NULL)))
		{
			PPH_STRING string;

			if (NT_SUCCESS(PhGetDriverName(fileObjectDriver.DriverHandle, &string)))
				HandleInfo["DrvDevice"] = CastPhString(string);

			if (NT_SUCCESS(PhGetDriverImageFileName(fileObjectDriver.DriverHandle, &string)))
				HandleInfo["DrvImage"] = CastPhString(string);

			NtClose(fileObjectDriver.DriverHandle);
		}
	}
	else if(m_TypeName == "Section")
	{
		HANDLE sectionHandle;
		NTSTATUS status = NtDuplicateObject(processHandle, (HANDLE)m_HandleId, NtCurrentProcess(), &sectionHandle, SECTION_QUERY | SECTION_MAP_READ, 0, 0 );
		if (!NT_SUCCESS(status))
			status = NtDuplicateObject(processHandle, (HANDLE)m_HandleId, NtCurrentProcess(), &sectionHandle, SECTION_QUERY, 0, 0);

		if (NT_SUCCESS(status))
		{
			SECTION_BASIC_INFORMATION sectionInfo;
            
			if (NT_SUCCESS(PhGetSectionBasicInformation(sectionHandle, &sectionInfo)))
			{
				HandleInfo["Attribs"] = (quint32)sectionInfo.AllocationAttributes;
				HandleInfo["Size"] = (quint64)sectionInfo.MaximumSize.QuadPart;
			}

			PPH_STRING fileName = NULL;
			if (NT_SUCCESS(PhGetSectionFileName(sectionHandle, &fileName)))
			{
				PPH_STRING newFileName;

				if (newFileName = PhResolveDevicePrefix(&fileName->sr)) 
				{
					PhDereferenceObject(fileName);
					fileName = newFileName;
				}
			}

			HandleInfo["File"] = CastPhString(fileName);

			NtClose(sectionHandle);
		}
	}
	else if(m_TypeName == "Mutant")
	{
		HANDLE mutantHandle;
		if (NT_SUCCESS(NtDuplicateObject(processHandle, (HANDLE)m_HandleId, NtCurrentProcess(), &mutantHandle, SEMAPHORE_QUERY_STATE, 0, 0)))
		{
			MUTANT_BASIC_INFORMATION mutantInfo;
			MUTANT_OWNER_INFORMATION ownerInfo;

			if (NT_SUCCESS(PhGetMutantBasicInformation(mutantHandle, &mutantInfo)))
			{
				HandleInfo["Count"] = mutantInfo.CurrentCount;
				HandleInfo["Abandoned"] = mutantInfo.AbandonedState;
			}

			if (NT_SUCCESS(PhGetMutantOwnerInformation(mutantHandle, &ownerInfo)))
			{
				HandleInfo["OwnerPID"] = (quint64)ownerInfo.ClientId.UniqueProcess;
				HandleInfo["OwnerTID"] = (quint64)ownerInfo.ClientId.UniqueThread;
			}

			NtClose(mutantHandle);
		}
	}
	else if(m_TypeName == "Process")
	{
		HANDLE dupHandle;
		if (NT_SUCCESS(NtDuplicateObject(processHandle, (HANDLE)m_HandleId, NtCurrentProcess(), &dupHandle, PROCESS_QUERY_LIMITED_INFORMATION, 0, 0)))
		{
			/*PPH_STRING fileName;
			if (NT_SUCCESS(PhGetProcessImageFileName(dupHandle, &fileName)))
			{
					= CastPhString(fileName);
			}*/

			NTSTATUS exitStatus = STATUS_PENDING;
			PROCESS_BASIC_INFORMATION procInfo;
			if (NT_SUCCESS(PhGetProcessBasicInformation(dupHandle, &procInfo)))
			{
				HandleInfo["PID"] = (quint64)procInfo.UniqueProcessId;

				HandleInfo["ExitStatus"] = procInfo.ExitStatus;
			}

			KERNEL_USER_TIMES times;
			if (NT_SUCCESS(PhGetProcessTimes(dupHandle, &times)))
			{
				HandleInfo["Created"] = FILETIME2time(times.CreateTime.QuadPart);
				if (exitStatus != STATUS_PENDING)
					HandleInfo["Exited"] = FILETIME2time(times.ExitTime.QuadPart);
			}

			NtClose(dupHandle);
		}
	}
	else if(m_TypeName == "Thread")
	{
		HANDLE dupHandle;
		if (NT_SUCCESS(NtDuplicateObject(processHandle, (HANDLE)m_HandleId, NtCurrentProcess(), &dupHandle, THREAD_QUERY_LIMITED_INFORMATION, 0, 0)))
		{
			/*PPH_STRING name;
			if (NT_SUCCESS(PhGetThreadName(dupHandle, &name)))
			{
				= CastPhString(name);
			}*/

			NTSTATUS exitStatus = STATUS_PENDING;
			THREAD_BASIC_INFORMATION threadInfo;
			if (NT_SUCCESS(PhGetThreadBasicInformation(dupHandle, &threadInfo)))
			{
				HandleInfo["PID"] = (quint64)threadInfo.ClientId.UniqueProcess;
				HandleInfo["TID"] = (quint64)threadInfo.ClientId.UniqueThread;

				HandleInfo["ExitStatus"] = threadInfo.ExitStatus;

				//if (NT_SUCCESS(PhOpenProcess(
				//    &processHandle,
				//    PROCESS_QUERY_LIMITED_INFORMATION,
				//    threadInfo.ClientId.UniqueProcess
				//    )))
				//{
				//    if (NT_SUCCESS(PhGetProcessModuleFileName(processHandle, &fileName)))
				//    {
				//        PhMoveReference(&fileName, PhGetFileName(fileName));
				//        PhSetListViewSubItem(Context->ListViewHandle, Context->ListViewRowCache[PH_HANDLE_GENERAL_INDEX_PROCESSTHREADNAME], 1, PhGetStringOrEmpty(fileName));
				//        PhDereferenceObject(fileName);
				//    }
				//
				//    NtClose(processHandle);
				//}
			}

			KERNEL_USER_TIMES times;
			if (NT_SUCCESS(PhGetThreadTimes(dupHandle, &times)))
			{
				HandleInfo["Created"] = FILETIME2time(times.CreateTime.QuadPart);
				if (exitStatus != STATUS_PENDING)
					HandleInfo["Exited"] = FILETIME2time(times.ExitTime.QuadPart);
			}

			NtClose(dupHandle);
		}
	}
	else if(m_TypeName == "Timer")
	{
		HANDLE timerHandle;
		if (NT_SUCCESS(NtDuplicateObject(processHandle, (HANDLE)m_HandleId, NtCurrentProcess(), &timerHandle, TIMER_QUERY_STATE, 0, 0)))
		{
			TIMER_BASIC_INFORMATION basicInfo;
			if (NT_SUCCESS(PhGetTimerBasicInformation(timerHandle, &basicInfo)))
			{
				HandleInfo["Remaining"] = basicInfo.RemainingTime.QuadPart;
				HandleInfo["Signaled"] = basicInfo.TimerState;
			}

			NtClose(timerHandle);
		}
	}
	/*else if(m_TypeName == "TpWorkerFactory") // ToDo
	{
		HANDLE workerFactoryHandle;
		if (NT_SUCCESS(NtDuplicateObject(processHandle, (HANDLE)m_HandleId, NtCurrentProcess(), &workerFactoryHandle, WORKER_FACTORY_QUERY_INFORMATION, 0, 0)))
		{
			WORKER_FACTORY_BASIC_INFORMATION basicInfo;
            if (NT_SUCCESS(NtQueryInformationWorkerFactory(workerFactoryHandle, WorkerFactoryBasicInformation, &basicInfo, sizeof(WORKER_FACTORY_BASIC_INFORMATION), NULL)))
            {
                PPH_SYMBOL_PROVIDER symbolProvider;
                PPH_STRING symbol = NULL;

                symbolProvider = PhCreateSymbolProvider(basicInfo.ProcessId);
                PhLoadSymbolProviderOptions(symbolProvider);

                if (symbolProvider->IsRealHandle)
                {
                    PhEnumGenericModules(basicInfo.ProcessId, symbolProvider->ProcessHandle, 0, EnumGenericModulesCallback, symbolProvider);

                    symbol = PhGetSymbolFromAddress(symbolProvider, (ULONG64)basicInfo.StartRoutine, NULL, NULL, NULL, NULL);
                }

                PhDereferenceObject(symbolProvider);

                if (symbol)
                {
                    //PhaFormatString(L"Worker Thread Start: %s", symbol->Buffer)->Buffer
                    PhDereferenceObject(symbol);
                }
                else
                {
                    //PhaFormatString(L"Worker Thread Start: 0x%Ix", basicInfo.StartRoutine)->Buffer;
                }
                //PhaFormatString(L"Worker Thread Context: 0x%Ix", basicInfo.StartParameter)->Buffer);
            }

			NtClose(workerFactoryHandle);
		}
	}*/

	NtClose(processHandle);

	return HandleInfo;
}

STATUS CWinHandle::Close(bool bForce)
{
	QWriteLocker Locker(&m_Mutex); 

	NTSTATUS status;
    HANDLE processHandle;
	if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, (HANDLE)m_ProcessId)))
    {
#ifndef SAFE_MODE // in safe mode always check and fail
		if (!bForce)
#endif
		{
			BOOLEAN critical = FALSE;
			BOOLEAN strict = FALSE;

			if (WindowsVersion >= WINDOWS_10)
			{
				BOOLEAN breakOnTermination;
				if (NT_SUCCESS(PhGetProcessBreakOnTermination(processHandle, &breakOnTermination)))
				{
					if (breakOnTermination)
					{
						critical = TRUE;
					}
				}

				PROCESS_MITIGATION_POLICY_INFORMATION policyInfo;
				policyInfo.Policy = ProcessStrictHandleCheckPolicy;
				policyInfo.StrictHandleCheckPolicy.Flags = 0;
				if (NT_SUCCESS(NtQueryInformationProcess(processHandle, ProcessMitigationPolicy, &policyInfo, sizeof(PROCESS_MITIGATION_POLICY_INFORMATION), NULL)))
				{
					if (policyInfo.StrictHandleCheckPolicy.Flags != 0)
					{
						strict = TRUE;
					}
				}
			}

			if (critical && strict)
			{
				NtClose(processHandle);
				return ERR(tr("You are about to close one or more handles for a critical process with strict handle checks enabled. This will shut down the operating system immediately!"), ERROR_CONFIRM);
			}
		}

        status = NtDuplicateObject(processHandle, (HANDLE)m_HandleId, NULL, NULL, 0, 0, DUPLICATE_CLOSE_SOURCE);

		NtClose(processHandle);

        if (!NT_SUCCESS(status))
        {
			return ERR(tr("Failed To close Handle"), status);
        }
    }
    else
    {
        return ERR(tr("Unable to open the process"), status);
    }

	return OK;
}

static NTSTATUS PhpDuplicateHandleFromProcess(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, HANDLE ProcessId, HANDLE HandleId)
{
    NTSTATUS status;
    HANDLE processHandle;

    if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_DUP_HANDLE, ProcessId )))
        return status;

    status = NtDuplicateObject(processHandle, HandleId, NtCurrentProcess(), Handle, DesiredAccess, 0, 0 );

    NtClose(processHandle);

    return status;
}

STATUS CWinHandle::DoHandleAction(EHandleAction Action)
{
	QWriteLocker Locker(&m_Mutex); 

	ACCESS_MASK DesiredAccess;
	switch (Action)
	{
		case eSemaphoreAcquire:	DesiredAccess = SYNCHRONIZE; break;
		case eSemaphoreRelease: DesiredAccess = SEMAPHORE_MODIFY_STATE; break;

		case eSetLow:
		case eSetHigh:			DesiredAccess = EVENT_PAIR_ALL_ACCESS; break;

		case eCancelTimer:		DesiredAccess = TIMER_MODIFY_STATE; break;

		default: /*eEvent...*/  DesiredAccess = EVENT_MODIFY_STATE; break;
	}

    NTSTATUS status;

	HANDLE processHandle;
	if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_DUP_HANDLE, (HANDLE)m_ProcessId )))
        return ERR(tr("Unable to open process handle"), status);

	HANDLE dupDandle;
	status = NtDuplicateObject(processHandle, (HANDLE)m_HandleId, NtCurrentProcess(), &dupDandle, DesiredAccess, 0, 0 );

    NtClose(processHandle);

	if (!NT_SUCCESS(status))
		return ERR(tr("Unable to open duplicate handle"), status);


    switch (Action)
    {
    case eSemaphoreAcquire:
        {
            LARGE_INTEGER timeout;

            timeout.QuadPart = 0;
            NtWaitForSingleObject(dupDandle, FALSE, &timeout);
        }
        break;
    case eSemaphoreRelease:
        NtReleaseSemaphore(dupDandle, 1, NULL);
        break;

    case eEventSet:
        NtSetEvent(dupDandle, NULL);
        break;
    case eEventReset:
        NtResetEvent(dupDandle, NULL);
        break;
    case eEventPulse:
        NtPulseEvent(dupDandle, NULL);
        break;

	case eSetLow:
		NtSetLowEventPair(dupDandle);
        break;
	case eSetHigh:
        NtSetHighEventPair(dupDandle);
        break;

	case eCancelTimer:
		NtCancelTimer(dupDandle, NULL);
		break;
    }

    NtClose(dupDandle);

	return OK;
}

NTSTATUS NTAPI CWinHandle__DuplicateHandle(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess,_In_opt_ PVOID Context)
{
	QPair<HANDLE, HANDLE>* pPair = (QPair<HANDLE, HANDLE>*)Context;
	return PhpDuplicateHandleFromProcess(Handle, DesiredAccess, pPair->first, pPair->second);
}

NTSTATUS NTAPI CWinHandle__cbPermissionsClosed(_In_opt_ PVOID Context)
{
	QPair<HANDLE, HANDLE>* pPair = (QPair<HANDLE, HANDLE>*)Context;
	delete pPair;

	return STATUS_SUCCESS;
}

void CWinHandle::OpenPermissions()
{
	QReadLocker Locker(&m_Mutex); 
	QPair<HANDLE, HANDLE>* pPair = new QPair<HANDLE, HANDLE>((HANDLE)m_ProcessId, (HANDLE)m_HandleId);
	Locker.unlock();
    PhEditSecurity(NULL, (wchar_t*)m_FileName.toStdWString().c_str(), L"Handle", CWinHandle__DuplicateHandle, CWinHandle__cbPermissionsClosed, pPair);
}
