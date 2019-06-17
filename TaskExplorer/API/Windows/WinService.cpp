/*
 * Process Hacker -
 *   qt wrapper and support functions
 *
 * Copyright (C) 2009-2015 wj32
 * Copyright (C) 2017 dmex
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
#include "../../GUI/TaskExplorer.h"
#include "WinService.h"
#include "WinModule.h"

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
            BOOLEAN DelayedStart : 1;
            BOOLEAN HasTriggers : 1;
            BOOLEAN PendingProcess : 1;
            BOOLEAN NeedsConfigUpdate : 1;
            BOOLEAN JustProcessed : 1;
            BOOLEAN Spare : 3;
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
	m_ProcessId = 0;

	m_StartType = 0;
	m_ErrorControl = 0;

	m = new SWinService();

	m_pModuleInfo = CModulePtr(new CWinModule());
	connect(m_pModuleInfo.data(), SIGNAL(AsyncDataDone(bool, ulong, ulong)), this, SLOT(OnAsyncDataDone(bool, ulong, ulong)));
}

CWinService::~CWinService()
{
	delete m;
}

bool CWinService::InitStaticData(void* pscManagerHandle, struct _ENUM_SERVICE_STATUS_PROCESSW* service)
{
	QWriteLocker Locker(&m_Mutex);

	m_SvcName = QString::fromStdWString(service->lpServiceName);
	m_DisplayName = QString::fromStdWString(service->lpDisplayName);
    m_Flags = service->ServiceStatusProcess.dwServiceFlags;
    m_ProcessId = service->ServiceStatusProcess.dwProcessId;
	m_Type = service->ServiceStatusProcess.dwServiceType;
	m_State = service->ServiceStatusProcess.dwCurrentState;
	m_ControlsAccepted = service->ServiceStatusProcess.dwControlsAccepted;

	//SC_HANDLE ScManagerHandle = *(SC_HANDLE*)pscManagerHandle;

	m->NeedsConfigUpdate = TRUE;

	return true;
}

bool CWinService::UpdateDynamicData(void* pscManagerHandle, struct _ENUM_SERVICE_STATUS_PROCESSW* service)
{
	bool modified = FALSE;

	if (m_Type != service->ServiceStatusProcess.dwServiceType ||
		m_State != service->ServiceStatusProcess.dwCurrentState ||
		m_ControlsAccepted != service->ServiceStatusProcess.dwControlsAccepted ||
		m->NeedsConfigUpdate)
	{
		m->NeedsConfigUpdate = FALSE;

		m_Type = service->ServiceStatusProcess.dwServiceType;
		m_State = service->ServiceStatusProcess.dwCurrentState;
		m_ControlsAccepted = service->ServiceStatusProcess.dwControlsAccepted;

		SC_HANDLE ScManagerHandle = *(SC_HANDLE*)pscManagerHandle;

		SC_HANDLE serviceHandle = OpenService(ScManagerHandle, service->lpServiceName, SERVICE_QUERY_CONFIG);

		if (serviceHandle)
		{
			PPH_STRING Name = PhCreateString(service->lpServiceName);

			LPQUERY_SERVICE_CONFIG config = (LPQUERY_SERVICE_CONFIG)PhGetServiceConfig(serviceHandle);
			if (config)
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
				m_FileName = CastPhString(fileName);
				//

				PhFree(config);
			}

			static PH_STRINGREF servicesKeyName = PH_STRINGREF_INIT(L"System\\CurrentControlSet\\Services\\");

			//PhpUpdateServiceNodeKey
			PPH_STRING keyName = PhConcatStringRef2(&servicesKeyName, &Name->sr);

			HANDLE keyHandle;
			if (NT_SUCCESS(PhOpenKey(&keyHandle, KEY_READ, PH_KEY_LOCAL_MACHINE, &keyName->sr, 0)))
			{
				PKEY_BASIC_INFORMATION basicInfo;
				if (NT_SUCCESS(PhQueryKey(keyHandle, KeyBasicInformation, (PVOID*)&basicInfo)))
				{
					m_KeyLastWriteTime = QDateTime::fromTime_t((int64_t)basicInfo->LastWriteTime.QuadPart / 10000000ULL - 11644473600ULL);
					PhFree(basicInfo);
				}

				NtClose(keyHandle);
			}

			PhDereferenceObject(keyName);
			//

			PhDereferenceObject(Name);

			// get file info
			if (!m_FileName.isEmpty())
				qobject_cast<CWinModule*>(m_pModuleInfo)->InitAsyncData(m_FileName);

			ULONG returnLength;
			SERVICE_DELAYED_AUTO_START_INFO delayedAutoStartInfo;
			if (QueryServiceConfig2(serviceHandle, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, (BYTE *)&delayedAutoStartInfo, sizeof(SERVICE_DELAYED_AUTO_START_INFO), &returnLength))
				m->DelayedStart = delayedAutoStartInfo.fDelayedAutostart;
			else
				m->DelayedStart = FALSE;

			PSERVICE_TRIGGER_INFO triggerInfo;
			if (triggerInfo = (PSERVICE_TRIGGER_INFO)PhQueryServiceVariableSize(serviceHandle, SERVICE_CONFIG_TRIGGER_INFO))
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

	if (m_ProcessId != service->ServiceStatusProcess.dwProcessId)
	{
		m_ProcessId = service->ServiceStatusProcess.dwProcessId;

		// todo:

		modified = true;
	}


	/*                // Add the service to its process, if appropriate.
                if (
                    (
                    serviceItem->State == SERVICE_RUNNING ||
                    serviceItem->State == SERVICE_PAUSED
                    ) &&
                    serviceItem->ProcessId
                    )
                {*/

	return modified;
}

void CWinService::OnAsyncDataDone(bool IsPacked, ulong ImportFunctions, ulong ImportModules)
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
		default: return tr("Unknown");
	}
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
		default: return tr("Unknown");
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
		default: return tr("Unknown");
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
		default: return tr("Unknown");
	}
}
