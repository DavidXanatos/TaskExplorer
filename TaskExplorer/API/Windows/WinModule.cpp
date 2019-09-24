#include "stdafx.h"
#include "WinModule.h"
#include "ProcessHacker.h"
#include "WindowsAPI.h"
#include "../../Common/Settings.h"

#include <QtWin>

CWinModule::CWinModule(quint64 ProcessId, bool IsSubsystemProcess, QObject *parent) 
	: CModuleInfo(parent) 
{
	m_ProcessId = ProcessId;
	m_IsSubsystemProcess = IsSubsystemProcess;
	m_EntryPoint = NULL;
    m_Flags = 0;
    m_Type = 0;
	m_LoadTime = 0;
    m_LoadReason = 0;
    m_LoadCount = 0;
	m_ImageTimeStamp = 0;
	m_ImageCharacteristics = 0;
	m_ImageDllCharacteristics = 0;

	m_VerifyResult = VrUnknown;

	m_IsPacked = false;
	m_ImportFunctions = 0;
	m_ImportModules = 0;
}

CWinModule::~CWinModule()
{
}

bool CWinModule::InitStaticData(struct _PH_MODULE_INFO* module, quint64 ProcessHandle)
{
	QWriteLocker Locker(&m_Mutex);

	m_IsLoaded = true;
	m_FileName = CastPhString(module->FileName, false);
	m_ModuleName = CastPhString(module->Name, false);

	m_BaseAddress = (quint64)module->BaseAddress;
	m_EntryPoint = (quint64)module->EntryPoint;
	m_Size = module->Size;
	m_Flags = module->Flags;
	m_Type = module->Type;
	m_LoadReason = module->LoadReason;
	m_LoadCount = module->LoadCount;
	m_LoadTime = FILETIME2time(module->LoadTime.QuadPart);
	m_ParentBaseAddress = (quint64)module->ParentBaseAddress;


	if (m_IsSubsystemProcess)
    {
        // HACK: Update the module type. (TO-DO: Move into PhEnumGenericModules) (dmex)
        m_Type = PH_MODULE_TYPE_ELF_MAPPED_IMAGE;
    }
    else
    {
        // Fix up the load count. If this is not an ordinary DLL or kernel module, set the load count to 0.
        if (m_Type != PH_MODULE_TYPE_MODULE &&
            m_Type != PH_MODULE_TYPE_WOW64_MODULE &&
            m_Type != PH_MODULE_TYPE_KERNEL_MODULE)
        {
            m_LoadCount = 0;
        }
    }

	if (m_Type == PH_MODULE_TYPE_MODULE ||
        m_Type == PH_MODULE_TYPE_WOW64_MODULE ||
        m_Type == PH_MODULE_TYPE_MAPPED_IMAGE ||
        m_Type == PH_MODULE_TYPE_KERNEL_MODULE)
    {
        PH_REMOTE_MAPPED_IMAGE remoteMappedImage;
        PPH_READ_VIRTUAL_MEMORY_CALLBACK readVirtualMemoryCallback;

        if (m_Type == PH_MODULE_TYPE_KERNEL_MODULE)
            readVirtualMemoryCallback = KphReadVirtualMemoryUnsafe;
        else
            readVirtualMemoryCallback = NtReadVirtualMemory;

        // Note:
        // On Windows 7 the LDRP_IMAGE_NOT_AT_BASE flag doesn't appear to be used
        // anymore. Instead we'll check ImageBase in the image headers. We read this in
        // from the process' memory because:
        //
        // 1. It (should be) faster than opening the file and mapping it in, and
        // 2. It contains the correct original image base relocated by ASLR, if present.

        m_Flags &= ~LDRP_IMAGE_NOT_AT_BASE;

        if (NT_SUCCESS(PhLoadRemoteMappedImageEx((HANDLE)ProcessHandle, &m_BaseAddress, readVirtualMemoryCallback, &remoteMappedImage)))
        {
            ULONG_PTR imageBase = 0;
            ULONG entryPoint = 0;

            m_ImageTimeStamp = remoteMappedImage.NtHeaders->FileHeader.TimeDateStamp;
            m_ImageCharacteristics = remoteMappedImage.NtHeaders->FileHeader.Characteristics;

            if (remoteMappedImage.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
            {
                PIMAGE_OPTIONAL_HEADER32 optionalHeader = (PIMAGE_OPTIONAL_HEADER32)&remoteMappedImage.NtHeaders->OptionalHeader;

                imageBase = (ULONG_PTR)optionalHeader->ImageBase;
                entryPoint = optionalHeader->AddressOfEntryPoint;
                m_ImageDllCharacteristics = optionalHeader->DllCharacteristics;
            }
            else if (remoteMappedImage.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            {
                PIMAGE_OPTIONAL_HEADER64 optionalHeader = (PIMAGE_OPTIONAL_HEADER64)&remoteMappedImage.NtHeaders->OptionalHeader;

                imageBase = (ULONG_PTR)optionalHeader->ImageBase;
                entryPoint = optionalHeader->AddressOfEntryPoint;
                m_ImageDllCharacteristics = optionalHeader->DllCharacteristics;
            }

            if (imageBase != (ULONG_PTR)m_BaseAddress)
                m_Flags |= LDRP_IMAGE_NOT_AT_BASE;

            if (entryPoint != 0)
                m_EntryPoint = (quint64)PTR_ADD_OFFSET(m_BaseAddress, entryPoint);

            PhUnloadRemoteMappedImage(&remoteMappedImage);
        }
    }

	InitFileInfo();

	return true;
}

bool CWinModule::InitStaticData(const QString& FileName)
{
	QWriteLocker Locker(&m_Mutex);

	m_IsLoaded = true;
	m_FileName = FileName;
	
	InitFileInfo();

	return true;
}

void CWinModule::InitFileInfo()
{
	FILE_NETWORK_OPEN_INFORMATION networkOpenInfo;
	if (NT_SUCCESS(PhQueryFullAttributesFileWin32((PWSTR)m_FileName.toStdWString().c_str(), &networkOpenInfo)))
	{
		m_ModificationTime = FILETIME2time(networkOpenInfo.LastWriteTime.QuadPart);
		m_FileSize = networkOpenInfo.EndOfFile.QuadPart;
	}
}

bool CWinModule::ResolveRefServices()
{
	QWriteLocker Locker(&m_Mutex);

	static PQUERY_TAG_INFORMATION I_QueryTagInformation = NULL;
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    if (PhBeginInitOnce(&initOnce))
    {
        I_QueryTagInformation = (PQUERY_TAG_INFORMATION)PhGetModuleProcAddress(L"advapi32.dll", "I_QueryTagInformation");
        PhEndInitOnce(&initOnce);
    }

	if (!I_QueryTagInformation)
		return false;
	
	wstring ModuleName = m_ModuleName.toStdWString();

	TAG_INFO_NAMES_REFERENCING_MODULE namesReferencingModule;
	memset(&namesReferencingModule, 0, sizeof(TAG_INFO_NAMES_REFERENCING_MODULE));
	namesReferencingModule.InParams.dwPid = m_ProcessId;
	namesReferencingModule.InParams.pszModule = (wchar_t*)ModuleName.c_str();

	ULONG win32Result = I_QueryTagInformation(NULL, eTagInfoLevelNamesReferencingModule, &namesReferencingModule);

	if (win32Result == ERROR_NO_MORE_ITEMS)
		win32Result = ERROR_SUCCESS;

	if (win32Result != ERROR_SUCCESS)
		return false;

	if (!namesReferencingModule.OutParams.pmszNames)
		return false;

	m_Services.clear();

	PWSTR serviceName = namesReferencingModule.OutParams.pmszNames;
	while (TRUE)
	{
		ULONG nameLength = (ULONG)PhCountStringZ(serviceName);
		if (nameLength == 0)
			break;

		m_Services.append(QString::fromWCharArray(serviceName, nameLength));

		serviceName += nameLength + 1;
	}

	LocalFree(namesReferencingModule.OutParams.pmszNames);
	
	return true;
}

bool CWinModule::InitStaticData(const QVariantMap& Module)
{
	QWriteLocker Locker(&m_Mutex);

	m_IsLoaded = false;
	//Module["Sequence"].toInt();
	m_ModuleName = Module["ImageName"].toString();
	m_BaseAddress = Module["BaseAddress"].toULongLong();
	m_Size = Module["Size"].toULongLong();
	m_LoadTime = Module["TimeStamp"].toULongLong();
	//Module["Checksum"].toByteArray();

	return true;
}

void CWinModule::ClearControlFlowGuardEnabled()
{
	QReadLocker Locker(&m_Mutex);
	m_ImageDllCharacteristics &= ~IMAGE_DLLCHARACTERISTICS_GUARD_CF;
}

bool CWinModule::UpdateDynamicData(struct _PH_MODULE_INFO* module)
{
	QReadLocker Locker(&m_Mutex);

	BOOLEAN modified = FALSE;

	/*
        if (m_JustProcessed)
            modified = TRUE;

        m_JustProcessed = FALSE;

        if (m_LoadCount != module->LoadCount)
        {
            m_LoadCount = module->LoadCount;
            modified = TRUE;
        }
	*/
	return modified;
}

void CWinModule::InitAsyncData(const QString& PackageFullName)
{
	QReadLocker Locker(&m_Mutex);

	QVariantMap Params;
	Params["FileName"] = m_FileName;
	Params["PackageFullName"] = PackageFullName;
	Params["IsSubsystemProcess"] = m_IsSubsystemProcess;

	// Note: this instance of CWinModule may be deleted before the async proces finishes,
	// so to make things simple and avoid emmory leaks we pass all params and results as a QVariantMap
	// its not the most eficient way but its simple and reliable.

	QFutureWatcher<QVariantMap>* pWatcher = new QFutureWatcher<QVariantMap>(this); // Note: the job will be canceled if the file will be deleted :D
	connect(pWatcher, SIGNAL(resultReadyAt(int)), this, SLOT(OnInitAsyncData(int)));
	connect(pWatcher, SIGNAL(finished()), pWatcher, SLOT(deleteLater()));
	pWatcher->setFuture(QtConcurrent::run(CWinModule::InitAsyncData, Params));
}

// Note: PhInitializeModuleVersionInfoCached does not look thread safe, so we have to guard it.
QMutex g_ModuleVersionInfoCachedMutex;

QVariantMap CWinModule::InitAsyncData(QVariantMap Params)
{
	QVariantMap Result;

	PPH_STRING FileName = CastQString(Params["FileName"].toString());
	PPH_STRING PackageFullName = CastQString(Params["PackageFullName"].toString());
	BOOLEAN IsSubsystemProcess = Params["IsSubsystemProcess"].toBool();

	PH_IMAGE_VERSION_INFO VersionInfo = { NULL, NULL, NULL, NULL };

	// PhpProcessQueryStage1 Begin
	NTSTATUS status;

	if (FileName && !IsSubsystemProcess)
	{
		HICON SmallIcon;
		HICON LargeIcon;
		if (!PhExtractIcon(FileName->Buffer, &LargeIcon, &SmallIcon))
		{
			LargeIcon = NULL;
			SmallIcon = NULL;
		}

		if (SmallIcon)
		{
			Result["SmallIcon"] = QtWin::fromHICON(SmallIcon);
			DestroyIcon(SmallIcon);
		}

		if (LargeIcon)
		{
			Result["LargeIcon"] = QtWin::fromHICON(LargeIcon);
			DestroyIcon(LargeIcon);
		}

		// Version info.
		QMutexLocker Lock(&g_ModuleVersionInfoCachedMutex);
		PhInitializeImageVersionInfoCached(&VersionInfo, FileName, FALSE);
	}
	// PhpProcessQueryStage1 End

	// PhpProcessQueryStage2 Begin
	if (FileName && !IsSubsystemProcess)
	{
		NTSTATUS status;

		VERIFY_RESULT VerifyResult = VERIFY_RESULT(0); //VrUnknown
		PPH_STRING VerifySignerName = NULL;
		if(theConf->GetBool("Options/VerifySignatures", true))
			VerifyResult = PhVerifyFileCached(FileName, PackageFullName, &VerifySignerName, FALSE);

		BOOLEAN IsPacked;
		ulong ImportFunctions;
		ulong ImportModules;
		status = PhIsExecutablePacked(FileName->Buffer, &IsPacked, &ImportModules, &ImportFunctions);

		// If we got an Module-related error, the Module is packed.
		if (status == STATUS_INVALID_IMAGE_NOT_MZ || status == STATUS_INVALID_IMAGE_FORMAT || status == STATUS_ACCESS_VIOLATION)
		{
			IsPacked = TRUE;
			ImportModules = ULONG_MAX;
			ImportFunctions = ULONG_MAX;
		}

		Result["VerifyResult"] = (int)VerifyResult;
		Result["VerifySignerName"] = CastPhString(VerifySignerName);
		Result["IsPacked"] = IsPacked;
		Result["ImportFunctions"] = (quint32)ImportFunctions;
		Result["ImportModules"] = (quint32)ImportModules;
	}

	if (/*PhEnableLinuxSubsystemSupport &&*/ FileName && IsSubsystemProcess)
	{
		QMutexLocker Lock(&g_ModuleVersionInfoCachedMutex);
		PhInitializeImageVersionInfoCached(&VersionInfo, FileName, TRUE);
	}
	// PhpProcessQueryStage2 End

	QVariantMap Infos;
	Infos["CompanyName"] = CastPhString(VersionInfo.CompanyName);
	Infos["Description"] = CastPhString(VersionInfo.FileDescription);
	Infos["FileVersion"] = CastPhString(VersionInfo.FileVersion);
	Infos["ProductName"] = CastPhString(VersionInfo.ProductName);

	Result["Infos"] = Infos;

	PhDereferenceObject(FileName);
	PhDereferenceObject(PackageFullName);

	return Result;
}

void CWinModule::OnInitAsyncData(int Index)
{
	QFutureWatcher<QVariantMap>* pWatcher = (QFutureWatcher<QVariantMap>*)sender();

	QVariantMap Result = pWatcher->resultAt(Index);

	QWriteLocker Locker(&m_Mutex);

	m_SmallIcon = Result["SmallIcon"].value<QPixmap>();
	m_LargeIcon = Result["LargeIcon"].value<QPixmap>();

	m_FileDetails.clear();
	QVariantMap Infos = Result["Infos"].toMap();
	foreach(const QString& Key, Infos.keys())
		m_FileDetails[Key] = Infos[Key].toString();

	m_VerifyResult = (EVerifyResult)Result["VerifyResult"].toInt();
	m_VerifySignerName = Result["VerifySignerName"].toString();

	m_IsPacked = Result["IsPacked"].toBool();
	m_ImportFunctions = Result["ImportFunctions"].toUInt();
	m_ImportModules = Result["ImportModules"].toUInt();

	emit AsyncDataDone(Result["IsPacked"].toBool(), Result["ImportFunctions"].toUInt(), Result["ImportModules"].toUInt());
}

QString CWinModule::GetTypeString() const
{
	QReadLocker Locker(&m_Mutex);

    switch (m_Type)
    {
    case PH_MODULE_TYPE_MODULE:
        return tr("DLL");
    case PH_MODULE_TYPE_MAPPED_FILE:
        return tr("Mapped file");
    case PH_MODULE_TYPE_MAPPED_IMAGE:
    case PH_MODULE_TYPE_ELF_MAPPED_IMAGE:
        return tr("Mapped image");
    case PH_MODULE_TYPE_WOW64_MODULE:
        return tr("WOW64 DLL");
    case PH_MODULE_TYPE_KERNEL_MODULE:
        return tr("Kernel module");
    default:	
		return tr("Unknown");
    }
}

QString CWinModule::GetVerifyResultString() const
{
	QReadLocker Locker(&m_Mutex);
	
	switch (m_VerifyResult)
	{
	case VrTrusted:			return tr("Trusted");
	case VrNoSignature:		return tr("Un signed");
	case VrExpired:
	case VrRevoked:
	case VrDistrust:
	case VrBadSignature:	return tr("Not trusted");
	default:				return tr("Unknown");
	}
}

QString CWinModule::GetASLRString() const
{
	QReadLocker Locker(&m_Mutex);

	if (m_ImageDllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE)
		return tr("ASLR");
	return QString();
}

QString CWinModule::GetCFGuardString() const
{
	QReadLocker Locker(&m_Mutex);

	if (m_ImageDllCharacteristics & IMAGE_DLLCHARACTERISTICS_GUARD_CF)
		return tr("CF Guard");
	return QString();
}

QString CWinModule::GetLoadReasonString() const
{
	QReadLocker Locker(&m_Mutex);

	if (m_Type == PH_MODULE_TYPE_KERNEL_MODULE)
    {
        return tr("Dynamic");
    }
    else if (m_Type == PH_MODULE_TYPE_MODULE || m_Type == PH_MODULE_TYPE_WOW64_MODULE)
    {
        switch ((_LDR_DLL_LOAD_REASON)m_LoadReason)
        {
        case LoadReasonStaticDependency:			return tr("Static dependency");
        case LoadReasonStaticForwarderDependency:	return tr("Static forwarder dependency");
        case LoadReasonDynamicForwarderDependency:	return tr("Dynamic forwarder dependency");
        case LoadReasonDelayloadDependency:			return tr("Delay load dependency");
        case LoadReasonDynamicLoad:					return tr("Dynamic");
        case LoadReasonAsImageLoad:					return tr("As image");
        case LoadReasonAsDataLoad:					return tr("As data");
        case LoadReasonEnclavePrimary:				return tr("Enclave");
        case LoadReasonEnclaveDependency:			return tr("Enclave dependency");
        default:
			if (WindowsVersion >= WINDOWS_8)
				return tr("Unknown");
			else
				return tr("N/A");
        }
    }
	return QString();
}

STATUS CWinModule::Unload(bool bForce)
{
	HANDLE ProcessId = (HANDLE)m_ProcessId;

	NTSTATUS status;
    HANDLE processHandle;

    switch (m_Type)
    {
    case PH_MODULE_TYPE_MODULE:
    case PH_MODULE_TYPE_WOW64_MODULE:
		//if(!bForce)
		//	return ERR(tr("Unloading a module may cause the process to crash."), ERROR_CONFIRM);

        if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, ProcessId)))
        {
            LARGE_INTEGER timeout;

            timeout.QuadPart = -(LONGLONG)UInt32x32To64(5, PH_TIMEOUT_SEC);
            status = PhUnloadDllProcess(processHandle, (PVOID)m_BaseAddress, &timeout);

            NtClose(processHandle);
        }

        if (status == STATUS_DLL_NOT_FOUND)
        {
            return ERR(tr("Unable to find the module to unload."));
        }

        if (!NT_SUCCESS(status))
        {
            return ERR(tr("Unable to unload the module."));
        }

        break;

    case PH_MODULE_TYPE_KERNEL_MODULE:
		if(!bForce)
			return ERR(tr("Unloading a driver may cause system instability."), ERROR_CONFIRM);

		{
			wstring Name = m_ModuleName.toStdWString();
			status = PhUnloadDriver((PVOID)m_BaseAddress, (wchar_t*)Name.c_str());
		}

        if (!NT_SUCCESS(status))
        {
			return ERR(tr("Unable to unload driver."), status);
        }

        break;

    case PH_MODULE_TYPE_MAPPED_FILE:
    case PH_MODULE_TYPE_MAPPED_IMAGE:
		//if(!bForce)
		//	return ERR(tr("Unmapping a section view may cause the process to crash."), ERROR_CONFIRM);

        if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_VM_OPERATION, ProcessId)))
        {
            status = NtUnmapViewOfSection(processHandle, (PVOID)m_BaseAddress);
            NtClose(processHandle);
        }

        if (!NT_SUCCESS(status))
        {
			return ERR(tr("Unable to unmap the section view at 0x").arg(QString::number(m_BaseAddress, 16)));
        }

        break;

    default:
        return ERR(tr("Unknown module type!"));
    }

    return OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CWinMainModule 

bool CWinMainModule::InitStaticData(quint64 ProcessId, const QString& FileName, bool IsSubsystemProcess, bool IsWow64)
{
	QWriteLocker Locker(&m_Mutex);

	m_IsLoaded = true;
	m_ProcessId = ProcessId;
	m_FileName = FileName;
	m_IsSubsystemProcess = IsSubsystemProcess;

	// subsystem
	if (m_IsSubsystemProcess)
    {
        m_ImageSubsystem = IMAGE_SUBSYSTEM_POSIX_CUI;
    }
    else
    {
		HANDLE ProcessHandle = NULL;
		// Try to get a handle with query information + vm read access.
		if (!NT_SUCCESS(PhOpenProcess(&ProcessHandle, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, (HANDLE)ProcessId)))
		{
			// Try to get a handle with query limited information + vm read access.
			PhOpenProcess(&ProcessHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, (HANDLE)ProcessId);
		}

		if(ProcessHandle)
		{
			PROCESS_BASIC_INFORMATION basicInfo;
			if (NT_SUCCESS(PhGetProcessBasicInformation((HANDLE)ProcessHandle, &basicInfo)) && basicInfo.PebBaseAddress != 0)
			{
				m_PebBaseAddress = (quint64)basicInfo.PebBaseAddress;
				if (IsWow64)
				{
					PVOID peb32;
					PhGetProcessPeb32((HANDLE)ProcessHandle, &peb32);
					m_PebBaseAddress32 = (quint64)peb32;
				}

				PVOID imageBaseAddress;
				PH_REMOTE_MAPPED_IMAGE mappedImage;
                if (NT_SUCCESS(NtReadVirtualMemory((HANDLE)ProcessHandle, PTR_ADD_OFFSET(basicInfo.PebBaseAddress, FIELD_OFFSET(PEB, ImageBaseAddress)), &imageBaseAddress, sizeof(PVOID), NULL)))
                {
                    if (NT_SUCCESS(PhLoadRemoteMappedImage((HANDLE)ProcessHandle, imageBaseAddress, &mappedImage)))
                    {
                        m_ImageTimeStamp = mappedImage.NtHeaders->FileHeader.TimeDateStamp;
                        m_ImageCharacteristics = mappedImage.NtHeaders->FileHeader.Characteristics;

                        if (mappedImage.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
                        {
                            m_ImageSubsystem = ((PIMAGE_OPTIONAL_HEADER32)&mappedImage.NtHeaders->OptionalHeader)->Subsystem;
                            m_ImageDllCharacteristics = ((PIMAGE_OPTIONAL_HEADER32)&mappedImage.NtHeaders->OptionalHeader)->DllCharacteristics;
                        }
                        else if (mappedImage.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
                        {
                            m_ImageSubsystem = ((PIMAGE_OPTIONAL_HEADER64)&mappedImage.NtHeaders->OptionalHeader)->Subsystem;
                            m_ImageDllCharacteristics = ((PIMAGE_OPTIONAL_HEADER64)&mappedImage.NtHeaders->OptionalHeader)->DllCharacteristics;
                        }

                        PhUnloadRemoteMappedImage(&mappedImage);
                    }
                }
            }

			NtClose(ProcessHandle);
        }
    }

	InitFileInfo();

	return true;
}

quint64 CWinMainModule::GetPebBaseAddress(bool bWow64) const
{
	QReadLocker Locker(&m_Mutex); 
	return bWow64 ? m_PebBaseAddress32 : m_PebBaseAddress;
}
