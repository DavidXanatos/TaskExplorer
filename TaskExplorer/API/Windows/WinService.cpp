/*
 * Task Explorer -
 *   qt wrapper and support functions based on svcsup.c
 *
 * Copyright (C) 2009-2015 wj32
 * Copyright (C) 2017 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 * 
 */


#include "stdafx.h"
#include "WinService.h"
#include "WinModule.h"
#include "WindowsAPI.h"
#include "../../SVC/TaskService.h"

#include "ProcessHacker.h"

struct SWinService
{
	SWinService() :
		BitFlags(NULL)
	{
	}

    union
    {
        BOOLEAN BitFlags;
        struct
        {
			BOOLEAN
            DelayedStart : 1,
            HasTriggers : 1,
            PendingProcess : 1,
            NeedsConfigUpdate : 1,
            JustProcessed : 1,
            Spare : 3;
        };
    };
};

CWinService::CWinService(QObject *parent) 
	: CServiceInfo(parent) 
{
	m_Type = 0;
	m_State = 0;
	m_ControlsAccepted = 0;
	m_Flags = 0;

	m_StartType = 0;
	m_ErrorControl = 0;

	m_Win32ExitCode = 0;
	m_ServiceSpecificExitCode = 0;

	m_KeyLastWriteTime = 0;

	m = new SWinService();
}

CWinService::CWinService(const QString& Name, QObject *parent)
	: CWinService(parent)
{
	m_SvcName = Name;
}

CWinService::~CWinService()
{
	delete m;
}

bool CWinService::InitStaticData(struct _ENUM_SERVICE_STATUS_PROCESSW* service)
{
	QWriteLocker Locker(&m_Mutex);

	m_SvcName = QString::fromStdWString(service->lpServiceName);
	m_DisplayName = QString::fromStdWString(service->lpDisplayName);

	m_CreateTimeStamp = GetTime() * 1000;

	m->NeedsConfigUpdate = TRUE;

	CWinModule* pModule = new CWinModule();
	m_pModuleInfo = CModulePtr(pModule);
	connect(pModule, SIGNAL(AsyncDataDone(bool, quint32, quint32)), this, SLOT(OnAsyncDataDone(bool, quint32, quint32)));

	return true;
}

bool CWinService::UpdatePID(struct _ENUM_SERVICE_STATUS_PROCESSW* service)
{
	QWriteLocker Locker(&m_Mutex);

	//bool IsActive = service->ServiceStatusProcess.dwCurrentState == SERVICE_RUNNING || service->ServiceStatusProcess.dwCurrentState == SERVICE_PAUSED;

	if (m_ProcessId == service->ServiceStatusProcess.dwProcessId)
		return false;
	
	m_ProcessId = service->ServiceStatusProcess.dwProcessId;

	return true;
}

bool CWinService::UpdateDynamicData(void* pscManagerHandle, struct _ENUM_SERVICE_STATUS_PROCESSW* service, bool bRefresh)
{
	QWriteLocker Locker(&m_Mutex);

	bool modified = FALSE;

	if (bRefresh || // we want to force a refresh to reload data whcih we normaly not re chack after the first load
		m_Type != service->ServiceStatusProcess.dwServiceType ||
		m_State != service->ServiceStatusProcess.dwCurrentState ||
		m_ControlsAccepted != service->ServiceStatusProcess.dwControlsAccepted ||
		m_Win32ExitCode != service->ServiceStatusProcess.dwWin32ExitCode ||
		m_ServiceSpecificExitCode != service->ServiceStatusProcess.dwServiceSpecificExitCode ||
		m_Flags != service->ServiceStatusProcess.dwServiceFlags ||
		m->NeedsConfigUpdate)
	{
		m->NeedsConfigUpdate = FALSE;

		m_Type = service->ServiceStatusProcess.dwServiceType;
		m_State = service->ServiceStatusProcess.dwCurrentState;
		m_ControlsAccepted = service->ServiceStatusProcess.dwControlsAccepted;
		m_Win32ExitCode = service->ServiceStatusProcess.dwWin32ExitCode;
		m_ServiceSpecificExitCode = service->ServiceStatusProcess.dwServiceSpecificExitCode;
		m_Flags = service->ServiceStatusProcess.dwServiceFlags;



		SC_HANDLE ScManagerHandle = *(SC_HANDLE*)pscManagerHandle;

		SC_HANDLE serviceHandle = OpenService(ScManagerHandle, service->lpServiceName, SERVICE_QUERY_CONFIG);

		if (serviceHandle)
		{
			PPH_STRING Name = PhCreateString(service->lpServiceName);

			LPQUERY_SERVICE_CONFIG config;
			if (NT_SUCCESS(PhGetServiceConfig(serviceHandle, &config)))
			{
				m_StartType = config->dwStartType;
				m_ErrorControl = config->dwErrorControl;

				m_BinaryPath = QString::fromWCharArray(config->lpBinaryPathName);
				m_GroupeName = QString::fromWCharArray(config->lpLoadOrderGroup);

				//PhGetServiceRelevantFileName 
				PPH_STRING fileName = NULL;
				PhGetServiceDllParameter(config->dwServiceType, &Name->sr, &fileName);

				if (!fileName)
				{
					PPH_STRING commandLine;

					if (config->lpBinaryPathName[0])
					{
						commandLine = PhCreateString(config->lpBinaryPathName);

						if (config->dwServiceType & SERVICE_WIN32)
						{
							PH_STRINGREF dummyFileName;
							PH_STRINGREF dummyArguments;

							PhParseCommandLineFuzzy(&commandLine->sr, &dummyFileName, &dummyArguments, &fileName);

							if (!fileName)
								PhSwapReference((PVOID*)&fileName, commandLine);
						}
						else
						{
							fileName = PhGetFileName(commandLine);
						}

						PhDereferenceObject(commandLine);
					}
				}
				QString FileName = CastPhString(fileName);
				//

				if (m_FileName != FileName)
				{
					m_FileName = FileName;
					if (!m_FileName.isEmpty() && !m_pModuleInfo.isNull()) 
					{
						qobject_cast<CWinModule*>(m_pModuleInfo)->InitStaticData(m_FileName);
						qobject_cast<CWinModule*>(m_pModuleInfo)->InitAsyncData();
					}
				}

				PhFree(config);
			}

			PPH_STRING description = PhGetServiceDescription(serviceHandle);
			if (description)
				m_Description = CastPhString(description);

			static PH_STRINGREF servicesKeyName = PH_STRINGREF_INIT(L"System\\CurrentControlSet\\Services\\");

			//PhpUpdateServiceNodeKey
			PPH_STRING keyName = PhConcatStringRef2(&servicesKeyName, &Name->sr);

			HANDLE keyHandle;
			if (NT_SUCCESS(PhOpenKey(&keyHandle, KEY_READ, PH_KEY_LOCAL_MACHINE, &keyName->sr, 0)))
			{
				PKEY_BASIC_INFORMATION basicInfo;
				if (NT_SUCCESS(PhQueryKey(keyHandle, KeyBasicInformation, (PVOID*)&basicInfo)))
				{
					m_KeyLastWriteTime = FILETIME2time(basicInfo->LastWriteTime.QuadPart);
					PhFree(basicInfo);
				}

				NtClose(keyHandle);
			}

			PhDereferenceObject(keyName);
			//

			PhDereferenceObject(Name);

			ULONG returnLength;
			SERVICE_DELAYED_AUTO_START_INFO delayedAutoStartInfo;
			if (QueryServiceConfig2(serviceHandle, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, (BYTE *)&delayedAutoStartInfo, sizeof(SERVICE_DELAYED_AUTO_START_INFO), &returnLength))
				m->DelayedStart = delayedAutoStartInfo.fDelayedAutostart;
			else
				m->DelayedStart = FALSE;

			PSERVICE_TRIGGER_INFO triggerInfo;
			if (NT_SUCCESS(PhQueryServiceVariableSize(serviceHandle, SERVICE_CONFIG_TRIGGER_INFO, (PVOID*)&triggerInfo)))
			{
				m->HasTriggers = triggerInfo->cTriggers != 0;
				PhFree(triggerInfo);
			}
			else
				m->HasTriggers = FALSE;

			CloseServiceHandle(serviceHandle);
		}

		modified = true;
	}

	return modified;
}

void CWinService::OnAsyncDataDone(bool IsPacked, quint32 ImportFunctions, quint32 ImportModules)
{
}

void CWinService::UnInit()
{

}

bool CWinService::IsDriver() const
{
	QReadLocker Locker(&m_Mutex);

	return (m_Type == SERVICE_KERNEL_DRIVER || m_Type == SERVICE_FILE_SYSTEM_DRIVER);
}

QString CWinService::GetTypeString() const
{
	QReadLocker Locker(&m_Mutex);

	switch (m_Type)
	{
		case SERVICE_KERNEL_DRIVER:				return tr("Driver");
		case SERVICE_FILE_SYSTEM_DRIVER:		return tr("FS driver");
		case SERVICE_WIN32_OWN_PROCESS:			return tr("Own process");
		case SERVICE_WIN32_SHARE_PROCESS:		return tr("Share process");
		case (SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS):
												return tr("Own interactive process");
		case (SERVICE_WIN32_SHARE_PROCESS | SERVICE_INTERACTIVE_PROCESS):
												return tr("Share interactive process");
		case SERVICE_USER_OWN_PROCESS:			return tr("User own process");
		case (SERVICE_USER_OWN_PROCESS | SERVICE_USERSERVICE_INSTANCE):
												return tr("User own process (instance)");
		case SERVICE_USER_SHARE_PROCESS:		return tr("User share process");
		case (SERVICE_USER_SHARE_PROCESS | SERVICE_USERSERVICE_INSTANCE):
												return tr("User share process (instance)");
		default: return tr("Unknown %1").arg(m_Type);
	}
}

bool CWinService::IsStopped() const
{
	QReadLocker Locker(&m_Mutex); // is it stoped (periode)?
	return m_State == SERVICE_STOPPED;
}

bool CWinService::IsRunning(bool bStrict) const
{
	QReadLocker Locker(&m_Mutex); // is it [still] running or about to start?
	return m_State == SERVICE_RUNNING || (!bStrict && (m_State == SERVICE_STOP_PENDING || m_State == SERVICE_START_PENDING));
}

bool CWinService::IsPaused() const
{
	QReadLocker Locker(&m_Mutex); // is it paused or about to be paused
	return m_State == SERVICE_PAUSE_PENDING || m_State == SERVICE_PAUSED || m_State == SERVICE_CONTINUE_PENDING;
}

QString CWinService::GetStateString() const
{
	QReadLocker Locker(&m_Mutex);

	switch (m_State)
	{
		case SERVICE_STOPPED:				return tr("Stopped");
		case SERVICE_START_PENDING:			return tr("Start pending");
		case SERVICE_STOP_PENDING:			return tr("Stop pending");
		case SERVICE_RUNNING:				return tr("Running");
		case SERVICE_CONTINUE_PENDING:		return tr("Continue pending");
		case SERVICE_PAUSE_PENDING:			return tr("Pause pending");
		case SERVICE_PAUSED:				return tr("Paused");
		default: return tr("Unknown %1").arg(m_State);
	}
}

QString CWinService::GetStartTypeString() const
{
	QReadLocker Locker(&m_Mutex);

	switch (m_StartType)
	{
		case SERVICE_DISABLED:				return tr("Disabled");
		case SERVICE_BOOT_START:			return tr("Boot start");
		case SERVICE_SYSTEM_START:			return tr("System start");
		case SERVICE_AUTO_START:			return tr("Auto start");
		case SERVICE_DEMAND_START:			return tr("Demand start");
		default: return tr("Unknown %1").arg(m_StartType);
	}
}

QString CWinService::GetErrorControlString() const
{
	QReadLocker Locker(&m_Mutex);

	switch (m_ErrorControl)
	{
		case SERVICE_ERROR_IGNORE:			return tr("Ignore");
		case SERVICE_ERROR_NORMAL:			return tr("Normal");
		case SERVICE_ERROR_SEVERE:			return tr("Severe");
		case SERVICE_ERROR_CRITICAL:		return tr("Critical");
		default: return tr("Unknown %1").arg(m_ErrorControl);
	}
}

STATUS CWinService::Start()
{
	QWriteLocker Locker(&m_Mutex);

	std::wstring SvcName = m_SvcName.toStdWString();
	BOOLEAN success = FALSE;

	SC_HANDLE serviceHandle;
    if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_START, (wchar_t*)SvcName.c_str())))
    {
        if (StartService(serviceHandle, 0, NULL))
            success = TRUE;

        CloseServiceHandle(serviceHandle);
    }

    if (!success)
    {
        NTSTATUS status = PhGetLastWin32ErrorAsNtStatus();

		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::ServiceAction(m_SvcName, "Start"))
				return OK;
		}

		return ERR(tr("Failed to start service"), status);
    }
	return OK;
}

STATUS CWinService::Pause()
{
	QWriteLocker Locker(&m_Mutex);

	std::wstring SvcName = m_SvcName.toStdWString();
	BOOLEAN success = FALSE;

	SC_HANDLE serviceHandle;
    if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_PAUSE_CONTINUE, (wchar_t*)SvcName.c_str())))
    {
		SERVICE_STATUS serviceStatus;

        if (ControlService(serviceHandle, SERVICE_CONTROL_PAUSE, &serviceStatus))
            success = TRUE;

        CloseServiceHandle(serviceHandle);
    }

    if (!success)
    {
        NTSTATUS status = PhGetLastWin32ErrorAsNtStatus();

		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::ServiceAction(m_SvcName, "Pause"))
				return OK;
		}

		return ERR(tr("Failed to pause service"), status);
    }
	return OK;
}

STATUS CWinService::Continue()
{
	QWriteLocker Locker(&m_Mutex);

	std::wstring SvcName = m_SvcName.toStdWString();
	BOOLEAN success = FALSE;

	SC_HANDLE serviceHandle;
    if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_PAUSE_CONTINUE, (wchar_t*)SvcName.c_str())))
    {
		SERVICE_STATUS serviceStatus;

        if (ControlService(serviceHandle, SERVICE_CONTROL_CONTINUE, &serviceStatus))
            success = TRUE;

        CloseServiceHandle(serviceHandle);
    }

    if (!success)
    {
        NTSTATUS status = PhGetLastWin32ErrorAsNtStatus();

		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::ServiceAction(m_SvcName, "Continue"))
				return OK;
		}

		return ERR(tr("Failed to continue service"), status);
    }
	return OK;
}

STATUS CWinService::Stop()
{
	QWriteLocker Locker(&m_Mutex);

	std::wstring SvcName = m_SvcName.toStdWString();
	BOOLEAN success = FALSE;

	SC_HANDLE serviceHandle;
    if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_STOP, (wchar_t*)SvcName.c_str())))
    {
		SERVICE_STATUS serviceStatus;

        if (ControlService(serviceHandle, SERVICE_CONTROL_STOP, &serviceStatus))
            success = TRUE;

        CloseServiceHandle(serviceHandle);
    }

    if (!success)
    {
        NTSTATUS status = PhGetLastWin32ErrorAsNtStatus();

		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::ServiceAction(m_SvcName, "Stop"))
				return OK;
		}

		return ERR(tr("Failed to stop service"), status);
    }
	return OK;
}

STATUS CWinService::Delete(bool bForce)
{
	QWriteLocker Locker(&m_Mutex);

	if (!bForce)
		return ERR(tr("Deleting a service can prevent the system from starting or functioning properly."), ERROR_CONFIRM);

#ifdef SAFE_MODE
	return OK;
#endif

	std::wstring SvcName = m_SvcName.toStdWString();
	BOOLEAN success = FALSE;

	SC_HANDLE serviceHandle;
    if (NT_SUCCESS(PhOpenService(&serviceHandle, DELETE, (wchar_t*)SvcName.c_str())))
    {
        if (DeleteService(serviceHandle))
            success = TRUE;

        CloseServiceHandle(serviceHandle);
    }

    if (!success)
    {
        NTSTATUS status = PhGetLastWin32ErrorAsNtStatus();

		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::ServiceAction(m_SvcName, "Delete"))
				return OK;
		}

		return ERR(tr("Failed to delete service"), status);
    }
	return OK;
}

NTSTATUS NTAPI CWinService__OpenService(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
	std::wstring* pName = ((std::wstring*)Context);

	SC_HANDLE serviceHandle;
	if (NT_SUCCESS(PhOpenService(&serviceHandle, DesiredAccess, (wchar_t*)pName->c_str())))
	{
		*Handle = serviceHandle;
		return STATUS_SUCCESS;
	}

	return PhGetLastWin32ErrorAsNtStatus();
}

NTSTATUS NTAPI CWinService__cbPermissionsClosed(_In_ HANDLE Handle, _In_ BOOLEAN Release, _In_opt_ PVOID Context)
{
	if (Release) {
		std::wstring* pName = ((std::wstring*)Context);
		delete pName;
	}

	return STATUS_SUCCESS;
}

void CWinService::OpenPermissions()
{

	std::wstring* pName = new std::wstring;
	*pName = GetName().toStdWString();

	PhEditSecurity(NULL, (wchar_t*)m_DisplayName.toStdWString().c_str(), L"Service", CWinService__OpenService, CWinService__cbPermissionsClosed, pName);
}