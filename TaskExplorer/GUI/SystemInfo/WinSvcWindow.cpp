#include "stdafx.h"
#include "WinSvcWindow.h"
#include "../../API/Windows/ProcessHacker/PhSvc.h"	
#include "../../API/SystemAPI.h"
#include "../../GUI/TaskExplorer.h"
#include "WinSvcTrigger.h"
#include "WinSvcShutdown.h"
#include "ServiceListWidget.h"
#include "../Common/ComboInputDialog.h"


CWinSvcWindow::CWinSvcWindow(QSharedPointer<CWinService> pService, QWidget *parent)
	: QMainWindow(parent)
{
	m_pService = pService;

	m_GeneralChanged = false;
	m_OldDelayedStart = false;

	m_RecoveryChanged = false;
	m_NumberOfActions = 0;
	m_EnableFlagCheckBox = false;
	m_RebootAfter = 0;

	m_TriggersChanged = false;
	m_InitialNumberOfTriggers = 0;

	m_PreshutdownTimeoutValid = false;
	m_RequiredPrivilegesValid = false;
	m_SidTypeValid = false;
	m_LaunchProtectedValid = false;
	m_OriginalLaunchProtected = 0;

	QWidget* centralWidget = new QWidget();
	this->setCentralWidget(centralWidget);
	ui.setupUi(centralWidget);

	this->setWindowTitle(tr("Properties of %1").arg(m_pService->GetName()));
	m_pDependencies = new CServiceListWidget();
	ui.dependenciesLayout->addWidget(m_pDependencies, 0, 0);
	m_pDependencies->SetLabel(tr("This service depends on the following services:"));
	m_pDependants = new CServiceListWidget();
	ui.dependantsLayout->addWidget(m_pDependants, 0, 0);
	m_pDependants->SetLabel(tr("The following services depend on this service:"));
	ui.triggers->setHeaderLabels(tr("Trigger|Action").split("|"));
	ui.privilegs->setHeaderLabels(tr("Name|Display name").split("|"));


	connect(ui.browseBtn, SIGNAL(pressed()), this, SLOT(OnBrowse()));
	connect(ui.showPW, SIGNAL(stateChanged(int)), this, SLOT(OnShowPW(int)));
	connect(ui.permissionsBtn, SIGNAL(pressed()), this, SLOT(OnPermissions()));

	connect(ui.firstFailure, SIGNAL(currentTextChanged(const QString &)), this, SLOT(FixReciveryControls()));
	connect(ui.secondFailure, SIGNAL(currentTextChanged(const QString &)), this, SLOT(FixReciveryControls()));
	connect(ui.subsequentFailures, SIGNAL(currentTextChanged(const QString &)), this, SLOT(FixReciveryControls()));

	connect(ui.browseRun, SIGNAL(pressed()), this, SLOT(OnBrowseRun()));
	connect(ui.restartOptionsBtn, SIGNAL(pressed()), this, SLOT(OnRestartOptions()));

	connect(ui.svcType, SIGNAL(currentTextChanged(const QString &)), this, SLOT(OnGeneralChanged()));
	connect(ui.startType, SIGNAL(currentTextChanged(const QString &)), this, SLOT(OnGeneralChanged()));
	connect(ui.errorControl, SIGNAL(currentTextChanged(const QString &)), this, SLOT(OnGeneralChanged()));
	connect(ui.svcGroup, SIGNAL(textEdited(const QString &)), this, SLOT(OnGeneralChanged()));
	connect(ui.binaryPath, SIGNAL(textChanged(const QString &)), this, SLOT(OnGeneralChanged()));
	connect(ui.userName, SIGNAL(textChanged(const QString &)), this, SLOT(OnGeneralChanged()));
	connect(ui.userPW, SIGNAL(textChanged(const QString &)), this, SLOT(OnGeneralChanged()));
	connect(ui.delayedStart, SIGNAL(stateChanged(int)), this, SLOT(OnGeneralChanged()));

	connect(ui.firstFailure, SIGNAL(currentTextChanged(const QString &)), this, SLOT(OnRecoveryChanged()));
	connect(ui.secondFailure, SIGNAL(currentTextChanged(const QString &)), this, SLOT(OnRecoveryChanged()));
	connect(ui.subsequentFailures, SIGNAL(currentTextChanged(const QString &)), this, SLOT(OnRecoveryChanged()));
	connect(ui.resetFailCtr, SIGNAL(textEdited(const QString &)), this, SLOT(OnRecoveryChanged()));
	connect(ui.restartService, SIGNAL(textEdited(const QString &)), this, SLOT(OnRecoveryChanged()));
	connect(ui.actionsForError, SIGNAL(stateChanged(int)), this, SLOT(OnRecoveryChanged()));
	connect(ui.programRun, SIGNAL(textEdited(const QString &)), this, SLOT(OnRecoveryChanged()));


	connect(ui.triggers, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnTrigger(QTreeWidgetItem*, int)));
	connect(ui.newBtn, SIGNAL(pressed()), this, SLOT(OnNewTrigger()));
	connect(ui.editBtn, SIGNAL(pressed()), this, SLOT(OnEditTrigger()));
	connect(ui.deleteBtn, SIGNAL(pressed()), this, SLOT(OnDeleteTrigger()));

	connect(ui.preShutdown, SIGNAL(textEdited(const QString &)), this, SLOT(OnPreShutdown()));
	connect(ui.sidType, SIGNAL(currentTextChanged(const QString &)), this, SLOT(OnSidType()));
	connect(ui.protectionType, SIGNAL(currentTextChanged(const QString &)), this, SLOT(OnProtectionType()));

	connect(ui.addBtn, SIGNAL(pressed()), this, SLOT(OnAddPrivilege()));
	connect(ui.removeBtn, SIGNAL(pressed()), this, SLOT(OnRemovePrivilege()));

	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));


	// General Tab
	{
		for (int i = 0; i < 10; i++)
			ui.svcType->addItem((char*)PhpServiceTypePairs[i].Key, (quint64)PhpServiceTypePairs[i].Value);
		for (int i = 0; i < 5; i++)
			ui.startType->addItem((char*)PhpServiceStartTypePairs[i].Key, (quint64)PhpServiceStartTypePairs[i].Value);
		for (int i = 0; i < 4; i++)
			ui.errorControl->addItem((char*)PhpServiceErrorControlPairs[i].Key, (quint64)PhpServiceErrorControlPairs[i].Value);

		SC_HANDLE serviceHandle;
		ULONG startType;
		ULONG errorControl;
		PPH_STRING serviceDll;

		ui.description->setPlainText(pService->GetDisplayName()); // temporary value we will try to retrive the proper one firther below

		ui.svcType->setCurrentIndex(ui.svcType->findData((quint64)pService->GetType()));

		startType = pService->GetStartType();
		errorControl = pService->GetErrorControl();
		serviceHandle = PhOpenService((wchar_t*)pService->GetName().toStdWString().c_str(), SERVICE_QUERY_CONFIG);

		if (serviceHandle)
		{
			LPQUERY_SERVICE_CONFIG config;
			PPH_STRING description;
			BOOLEAN delayedStart;

			if (config = (LPQUERY_SERVICE_CONFIG)PhGetServiceConfig(serviceHandle))
			{
				ui.svcGroup->setText(QString::fromWCharArray(config->lpLoadOrderGroup));
				ui.binaryPath->setText(QString::fromWCharArray(config->lpBinaryPathName));
				ui.userName->setText(QString::fromWCharArray(config->lpServiceStartName));

				if (startType != config->dwStartType || errorControl != config->dwErrorControl)
				{
					startType = config->dwStartType;
					errorControl = config->dwErrorControl;
					//PhMarkNeedsConfigUpdateServiceItem(serviceItem);
				}

				PhFree(config);
			}

			if (description = PhGetServiceDescription(serviceHandle))
				ui.description->setPlainText(CastPhString(description));

			if (PhGetServiceDelayedAutoStart(serviceHandle, &delayedStart))
			{
				m_OldDelayedStart = delayedStart;

				ui.delayedStart->setChecked(delayedStart);
			}

			CloseServiceHandle(serviceHandle);
		}

		ui.startType->setCurrentIndex(ui.startType->findData((quint64)startType));
		ui.errorControl->setCurrentIndex(ui.errorControl->findData((quint64)errorControl));

		ui.userPW->setText(tr("password"));
		ui.showPW->setChecked(false);

		PH_STRINGREF svcName;
		svcName.Buffer = (wchar_t*)pService->GetName().toStdWString().c_str();
		svcName.Length = pService->GetName().length() * sizeof(wchar_t);
		if (NT_SUCCESS(PhGetServiceDllParameter(pService->GetType(), &svcName, &serviceDll)))
		{
			ui.dllPath->setText(QString::fromWCharArray(serviceDll->Buffer));
			PhDereferenceObject(serviceDll);
		}
		else
		{
			ui.dllPath->setText(tr("N/A"));
		}

		m_GeneralChanged = false;
	}

	// Recovery Tab
	if ((pService->GetFlags() & SERVICE_RUNS_IN_SYSTEM_PROCESS) != 0)
	{
		// Services which run in system processes don't support failure actions.
        // todo: Create a different page with a message saying this.
		ui.recoveryTab->setEnabled(false);
	}
	else
	{
		for (int i = 0; i < 4; i++) {
			ui.firstFailure->addItem((char*)ServiceActionPairs[i].Key, (quint64)ServiceActionPairs[i].Value);
			ui.secondFailure->addItem((char*)ServiceActionPairs[i].Key, (quint64)ServiceActionPairs[i].Value);
			ui.subsequentFailures->addItem((char*)ServiceActionPairs[i].Key, (quint64)ServiceActionPairs[i].Value);
		}

		NTSTATUS status = STATUS_SUCCESS;
		SC_HANDLE serviceHandle;
		LPSERVICE_FAILURE_ACTIONS failureActions;
		SERVICE_FAILURE_ACTIONS_FLAG failureActionsFlag;
		SC_ACTION_TYPE lastType;
		ULONG returnLength;
		ULONG i;

		if (serviceHandle = PhOpenService((wchar_t*)pService->GetName().toStdWString().c_str(), SERVICE_QUERY_CONFIG))
		{
			if (failureActions = (LPSERVICE_FAILURE_ACTIONS)PhQueryServiceVariableSize(serviceHandle, SERVICE_CONFIG_FAILURE_ACTIONS))
			{
				// Failure action types

				m_NumberOfActions = failureActions->cActions;

				if (failureActions->cActions != 0 && failureActions->cActions != 3)
					status = STATUS_SOME_NOT_MAPPED;

				// If failure actions are not defined for a particular fail count, the
				// last failure action is used. Here we duplicate this behaviour when there
				// are fewer than 3 failure actions.
				lastType = SC_ACTION_NONE;

				ui.firstFailure->setCurrentIndex(ui.firstFailure->findData(failureActions->cActions >= 1 ? (lastType = failureActions->lpsaActions[0].Type) : lastType));
				ui.secondFailure->setCurrentIndex(ui.secondFailure->findData(failureActions->cActions >= 2 ? (lastType = failureActions->lpsaActions[1].Type) : lastType));
				ui.subsequentFailures->setCurrentIndex(ui.subsequentFailures->findData(failureActions->cActions >= 3 ? (lastType = failureActions->lpsaActions[2].Type) : lastType));

				// Reset fail count after

				ui.resetFailCtr->setText(QString::number(failureActions->dwResetPeriod / (60 * 60 * 24))); // s to days

				// Restart service after

				ui.restartService->setText("1");

				for (i = 0; i < failureActions->cActions; i++)
				{
					if (failureActions->lpsaActions[i].Type == SC_ACTION_RESTART)
					{
						if (failureActions->lpsaActions[i].Delay != 0)
						{
							ui.restartService->setText(QString::number(failureActions->lpsaActions[i].Delay / (1000 * 60))); // ms to min
						}

						break;
					}
				}

				// Enable actions for stops with errors

				if (QueryServiceConfig2(
					serviceHandle,
					SERVICE_CONFIG_FAILURE_ACTIONS_FLAG,
					(BYTE *)&failureActionsFlag,
					sizeof(SERVICE_FAILURE_ACTIONS_FLAG),
					&returnLength
				))
				{
					ui.actionsForError->setChecked(failureActionsFlag.fFailureActionsOnNonCrashFailures);
					m_EnableFlagCheckBox = true;
				}
				else
				{
					m_EnableFlagCheckBox = false;
				}

				// Restart computer options

				m_RebootAfter = 1 * 1000 * 60;

				for (i = 0; i < failureActions->cActions; i++)
				{
					if (failureActions->lpsaActions[i].Type == SC_ACTION_REBOOT)
					{
						if (failureActions->lpsaActions[i].Delay != 0)
							m_RebootAfter = failureActions->lpsaActions[i].Delay;

						break;
					}
				}

				if (failureActions->lpRebootMsg && failureActions->lpRebootMsg[0] != 0)
					m_RebootMessage = QString::fromWCharArray(failureActions->lpRebootMsg);
				else
					m_RebootMessage = QString();

				// Run program

				ui.programRun->setText(QString::fromWCharArray(failureActions->lpCommand));

				PhFree(failureActions);
			}
			else
			{
				//return NTSTATUS_FROM_WIN32(GetLastError());
			}

			CloseServiceHandle(serviceHandle);

			//status = EspLoadRecoveryInfo(hwndDlg, context);

			if (status == STATUS_SOME_NOT_MAPPED)
			{
				if (m_NumberOfActions > 3)
				{
					QMessageBox::warning(NULL, "TaskExplorer", tr("The service has %1 failure actions configured, but this program only supports editing 3.\r\n"
						"If you save the recovery information using this program, the additional failure actions will be lost.").arg(m_NumberOfActions));
				}
			}
			else if (!NT_SUCCESS(status))
			{
				// error:
			}

			FixReciveryControls();

			m_RecoveryChanged = false;
		}
		else
		{
			//return NTSTATUS_FROM_WIN32(GetLastError());
		}
	}

	// Dependencies tab
	{
		HWND serviceListHandle;
		PPH_LIST serviceList;
		SC_HANDLE serviceHandle;
		ULONG win32Result = 0;
		BOOLEAN success = FALSE;

		if (serviceHandle = PhOpenService((wchar_t*)pService->GetName().toStdWString().c_str(), SERVICE_QUERY_CONFIG))
		{
			LPQUERY_SERVICE_CONFIG serviceConfig;

			if (serviceConfig =  (LPQUERY_SERVICE_CONFIG)PhGetServiceConfig(serviceHandle))
			{
				PWSTR dependency;
		
				dependency = serviceConfig->lpDependencies;
				QStringList serviceList;
				success = TRUE;

				if (dependency)
				{
					ULONG dependencyLength;

					while (TRUE)
					{
						dependencyLength = (ULONG)PhCountStringZ(dependency);

						if (dependencyLength == 0)
							break;

						if (dependency[0] == SC_GROUP_IDENTIFIER)
							goto ContinueLoop;

						serviceList.append(QString::fromWCharArray(dependency));

					ContinueLoop:
						dependency += dependencyLength + 1;
					}
				}

				m_pDependencies->SetServices(serviceList);

				PhFree(serviceConfig);
			}
			else
			{
				win32Result = GetLastError();
			}

			CloseServiceHandle(serviceHandle);
		}
		else
		{
			win32Result = GetLastError();
		}

		if (!success)
		{
			/*PhSetDialogItemText(hwndDlg, IDC_SERVICES_LAYOUT, PhaConcatStrings2(L"Unable to enumerate dependencies: ",
				((PPH_STRING)PH_AUTO(PhGetWin32Message(win32Result)))->Buffer)->Buffer);
			ShowWindow(GetDlgItem(hwndDlg, IDC_SERVICES_LAYOUT), SW_SHOW);*/
		}
	}

	// dependants Tab
	{
		HWND serviceListHandle;
		PPH_LIST serviceList;
		SC_HANDLE serviceHandle;
		ULONG win32Result = 0;
		BOOLEAN success = FALSE;

		if (serviceHandle = PhOpenService((wchar_t*)pService->GetName().toStdWString().c_str(), SERVICE_ENUMERATE_DEPENDENTS))
		{
			LPENUM_SERVICE_STATUS dependentServices;
			ULONG numberOfDependentServices;

			if (dependentServices = (LPENUM_SERVICE_STATUS)EsEnumDependentServices(serviceHandle, 0, &numberOfDependentServices))
			{
				ULONG i;
				QStringList serviceList;
				success = TRUE;

				for (i = 0; i < numberOfDependentServices; i++)
				{
					serviceList.append(QString::fromWCharArray(dependentServices[i].lpServiceName));
				}

				m_pDependants->SetServices(serviceList);

				PhFree(dependentServices);
			}
			else
			{
				win32Result = GetLastError();
			}

			CloseServiceHandle(serviceHandle);
		}
		else
		{
			win32Result = GetLastError();
		}

		if (!success)
		{
			/*PhSetDialogItemText(hwndDlg, IDC_SERVICES_LAYOUT, PhaConcatStrings2(L"Unable to enumerate dependents: ",
				((PPH_STRING)PH_AUTO(PhGetWin32Message(win32Result)))->Buffer)->Buffer);
			ShowWindow(GetDlgItem(hwndDlg, IDC_SERVICES_LAYOUT), SW_SHOW);*/
		}
	}

	// Triggers Tab
	{
		NTSTATUS status = STATUS_SUCCESS;
		SC_HANDLE serviceHandle;

		if (serviceHandle = PhOpenService((wchar_t*)pService->GetName().toStdWString().c_str(), SERVICE_QUERY_CONFIG))
		{
			PSERVICE_TRIGGER_INFO triggerInfo;
			ULONG i;

			if (triggerInfo = (PSERVICE_TRIGGER_INFO)PhQueryServiceVariableSize(serviceHandle, SERVICE_CONFIG_TRIGGER_INFO))
			{
				for (i = 0; i < triggerInfo->cTriggers; i++)
				{
					PSERVICE_TRIGGER trigger = &triggerInfo->pTriggers[i];

					PES_TRIGGER_INFO info = EspCreateTriggerInfo(trigger);

					AddTrigger(info);
				}

				PhFree(triggerInfo);
			}

			m_InitialNumberOfTriggers = m_TriggerInfos.size();

			CloseServiceHandle(serviceHandle);
		}
		else
		{
			//return NTSTATUS_FROM_WIN32(GetLastError());
		}

		m_TriggersChanged = false;
	}

	// Other Tab
	{
		for (int i = 0; i < 3; i++)
			ui.sidType->addItem((char*)EspServiceSidTypePairs[i].Key, (quint64)EspServiceSidTypePairs[i].Value);
		for (int i = 0; i < 4; i++)
			ui.protectionType->addItem((char*)EspServiceLaunchProtectedPairs[i].Key, (quint64)EspServiceLaunchProtectedPairs[i].Value);

		ULONG Type = pService->GetType();

        if (Type == SERVICE_KERNEL_DRIVER || Type == SERVICE_FILE_SYSTEM_DRIVER)
        {
            // Drivers don't support required privileges.
			ui.addBtn->setEnabled(false);
			ui.removeBtn->setEnabled(false);
        }

        if (WindowsVersion < WINDOWS_8_1)
			ui.protectionType->setEnabled(false);

		PH_STRINGREF svcName;
		svcName.Buffer = (wchar_t*)pService->GetName().toStdWString().c_str();
		svcName.Length = pService->GetName().length() * sizeof(wchar_t);
		PPH_STRING Sid = EspGetServiceSidString(&svcName);
		ui.serviceSID->setText(Sid ? CastPhString(Sid) : tr("N/A"));
        

		// EspLoadOtherInfo
		NTSTATUS status = STATUS_SUCCESS;
		SC_HANDLE serviceHandle;
		ULONG returnLength;
		SERVICE_PRESHUTDOWN_INFO preshutdownInfo;
		LPSERVICE_REQUIRED_PRIVILEGES_INFO requiredPrivilegesInfo;
		SERVICE_SID_INFO sidInfo;
		SERVICE_LAUNCH_PROTECTED_INFO launchProtectedInfo;

		if (serviceHandle = PhOpenService((wchar_t*)pService->GetName().toStdWString().c_str(), SERVICE_QUERY_CONFIG))
		{
			// Preshutdown timeout
			if (QueryServiceConfig2(serviceHandle,
				SERVICE_CONFIG_PRESHUTDOWN_INFO,
				(PBYTE)&preshutdownInfo,
				sizeof(SERVICE_PRESHUTDOWN_INFO),
				&returnLength
				))
			{
				ui.preShutdown->setText(QString::number(preshutdownInfo.dwPreshutdownTimeout));
				m_PreshutdownTimeoutValid = true;
			}

			// Required privileges

			if (requiredPrivilegesInfo = (LPSERVICE_REQUIRED_PRIVILEGES_INFO)PhQueryServiceVariableSize(serviceHandle, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO))
			{
				PWSTR privilege;
				ULONG privilegeLength;

				privilege = requiredPrivilegesInfo->pmszRequiredPrivileges;

				if (privilege)
				{
					while (TRUE)
					{
						privilegeLength = (ULONG)PhCountStringZ(privilege);

						if (privilegeLength == 0)
							break;

						AddPrivilege(QString::fromWCharArray(privilege, privilegeLength));

						privilege += privilegeLength + 1;
					}
				}

				PhFree(requiredPrivilegesInfo);
				m_RequiredPrivilegesValid = true;
			}

			// SID type

			if (QueryServiceConfig2(serviceHandle,
				SERVICE_CONFIG_SERVICE_SID_INFO,
				(PBYTE)&sidInfo,
				sizeof(SERVICE_SID_INFO),
				&returnLength
				))
			{
				ui.sidType->setCurrentIndex(ui.sidType->findData((quint32)sidInfo.dwServiceSidType));
				m_SidTypeValid = true;
			}

			// Launch protected

			if (QueryServiceConfig2(serviceHandle,
				SERVICE_CONFIG_LAUNCH_PROTECTED,
				(PBYTE)&launchProtectedInfo,
				sizeof(SERVICE_LAUNCH_PROTECTED_INFO),
				&returnLength
				))
			{
				ui.protectionType->setCurrentIndex(ui.protectionType->findData((quint32)launchProtectedInfo.dwLaunchProtected));

				m_LaunchProtectedValid = true;
				m_OriginalLaunchProtected = launchProtectedInfo.dwLaunchProtected;
			}

			CloseServiceHandle(serviceHandle);
		}
		else
		{
			//return NTSTATUS_FROM_WIN32(GetLastError());
		}
	}
}

CWinSvcWindow::~CWinSvcWindow()
{
	foreach(void* pTriggerInfo, m_TriggerInfos)
		EspDestroyTriggerInfo((PES_TRIGGER_INFO)pTriggerInfo);
}

void CWinSvcWindow::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CWinSvcWindow::accept()
{
	// General Tab
	if(m_GeneralChanged)
	{
		m_GeneralChanged = false;

		NTSTATUS status;
		SC_HANDLE serviceHandle;

		ULONG newServiceType = ui.svcType->currentData().toULongLong();
		ULONG newServiceStartType = ui.startType->currentData().toULongLong();
		ULONG newServiceErrorControl = ui.errorControl->currentData().toULongLong();

		QString newServiceGroup = ui.svcGroup->text();
		QString newServiceBinaryPath = ui.binaryPath->text();
		QString newServiceUserAccount = ui.userName->text();
		QString newServicePassword;
		if (ui.showPW->isChecked())
			newServicePassword = ui.userPW->text();

		serviceHandle = PhOpenService((wchar_t*)m_pService->GetName().toStdWString().c_str(), SERVICE_CHANGE_CONFIG);

		if (serviceHandle)
		{
			if (ChangeServiceConfig(
				serviceHandle,
				newServiceType,
				newServiceStartType,
				newServiceErrorControl,
				newServiceBinaryPath.toStdWString().c_str(),
				newServiceGroup.toStdWString().c_str(),
				NULL,
				NULL,
				newServiceUserAccount.toStdWString().c_str(),
				newServicePassword.toStdWString().c_str(),
				NULL
			))
			{
				bool newDelayedStart = ui.delayedStart->isChecked();

				if (newDelayedStart != m_OldDelayedStart)
				{
					PhSetServiceDelayedAutoStart(serviceHandle, newDelayedStart);
				}

				//PhMarkNeedsConfigUpdateServiceItem(serviceItem);

				CloseServiceHandle(serviceHandle);
			}
			else
			{
				CloseServiceHandle(serviceHandle);
				
				QMessageBox::warning(NULL, "TaskExplorer", tr("Unable to change service configuration, error: %1").arg(GetLastError()));
			}
		}
		else
		{
			/*if (GetLastError() == ERROR_ACCESS_DENIED && !PhGetOwnTokenAttributes().Elevated)
			{
				// todo; restart as service and retry
			}
			else*/
			{
				QMessageBox::warning(NULL, "TaskExplorer", tr("Unable to change service configuration, error: %1").arg(GetLastError()));
			}
		}
	}

	// Recovery Tab
	if (m_RecoveryChanged)
	{
		m_RecoveryChanged = false;

		NTSTATUS status;
        SC_HANDLE serviceHandle;
        ULONG restartServiceAfter;
        SERVICE_FAILURE_ACTIONS failureActions;
        SC_ACTION actions[3];
        ULONG i;
        BOOLEAN enableRestart = FALSE;

        
        // Build the failure actions structure.

        failureActions.dwResetPeriod = ui.resetFailCtr->text().toULongLong() * 60 * 60 * 24;
		wstring RebootMsg = m_RebootMessage.toStdWString();
        failureActions.lpRebootMsg = (wchar_t*)RebootMsg.c_str();
		wstring RrogramRun = ui.programRun->text().toStdWString();
        failureActions.lpCommand = (wchar_t*)RrogramRun.c_str();
        failureActions.cActions = 3;
        failureActions.lpsaActions = actions;

		actions[0].Type = (SC_ACTION_TYPE)ui.firstFailure->currentData().toULongLong();
        actions[1].Type = (SC_ACTION_TYPE)ui.secondFailure->currentData().toULongLong();
        actions[2].Type = (SC_ACTION_TYPE)ui.subsequentFailures->currentData().toULongLong();
		
        restartServiceAfter = ui.restartService->text().toULongLong() * 1000 * 60;

        for (i = 0; i < 3; i++)
        {
            switch (actions[i].Type)
            {
            case SC_ACTION_RESTART:
                actions[i].Delay = restartServiceAfter;
                enableRestart = TRUE;
                break;
            case SC_ACTION_REBOOT:
                actions[i].Delay = m_RebootAfter;
                break;
            case SC_ACTION_RUN_COMMAND:
                actions[i].Delay = 0;
                break;
            }
        }

        // Try to save the changes.

		serviceHandle = PhOpenService((wchar_t*)m_pService->GetName().toStdWString().c_str(), SERVICE_CHANGE_CONFIG | (enableRestart ? SERVICE_START : 0)); // SC_ACTION_RESTART requires SERVICE_START

        if (serviceHandle)
        {
            if (ChangeServiceConfig2(
                serviceHandle,
                SERVICE_CONFIG_FAILURE_ACTIONS,
                &failureActions
                ))
            {
                if (m_EnableFlagCheckBox)
                {
                    SERVICE_FAILURE_ACTIONS_FLAG failureActionsFlag;

					failureActionsFlag.fFailureActionsOnNonCrashFailures = ui.actionsForError->isChecked();

                    ChangeServiceConfig2(
                        serviceHandle,
                        SERVICE_CONFIG_FAILURE_ACTIONS_FLAG,
                        &failureActionsFlag
                        );
                }

                CloseServiceHandle(serviceHandle);
            }
            else
            {
                CloseServiceHandle(serviceHandle);
                
				QMessageBox::warning(NULL, "TaskExplorer", tr("Unable to change service recovery information: %1").arg(GetLastError()));
				// ((PPH_STRING)PH_AUTO(PhGetWin32Message(GetLastError())))->Buffer
            }
        }
        else
        {
            if (GetLastError() == ERROR_ACCESS_DENIED && !PhGetOwnTokenAttributes().Elevated)
            {
                // todo; restart as service and retry
            }
            //else
            {
                QMessageBox::warning(NULL, "TaskExplorer", tr("Unable to change service recovery information: %1").arg(GetLastError()));
				// ((PPH_STRING)PH_AUTO(PhGetWin32Message(GetLastError())))->Buffer
            }
        }

	}

	// Dependencies tab
	{
		// NOTHING
	}

	// dependants Tab
	{
		// NOTHING
	}

	// Triggers Tab
	// Do not try to change trigger information if we didn't have any triggers before and we don't
	// have any now. ChangeServiceConfig2 returns an error in this situation.
	if (m_TriggersChanged && m_InitialNumberOfTriggers == 0 && m_TriggerInfos.size() == 0)
	{
		m_TriggersChanged = false;

		PH_AUTO_POOL autoPool;
		SC_HANDLE serviceHandle;
		SERVICE_TRIGGER_INFO triggerInfo;
		ULONG i;
		ULONG j;

		PhInitializeAutoPool(&autoPool);

		memset(&triggerInfo, 0, sizeof(SERVICE_TRIGGER_INFO));
		triggerInfo.cTriggers = m_TriggerInfos.size();

		// pTriggers needs to be NULL when there are no triggers.
		if (triggerInfo.cTriggers != 0)
		{
			triggerInfo.pTriggers = (PSERVICE_TRIGGER)PH_AUTO(PhCreateAlloc(triggerInfo.cTriggers * sizeof(SERVICE_TRIGGER)));
			memset(triggerInfo.pTriggers, 0, triggerInfo.cTriggers * sizeof(SERVICE_TRIGGER));

			for (i = 0; i < triggerInfo.cTriggers; i++)
			{
				PSERVICE_TRIGGER trigger = &triggerInfo.pTriggers[i];
				PES_TRIGGER_INFO info = (PES_TRIGGER_INFO)m_TriggerInfos[i];

				trigger->dwTriggerType = info->Type;
				trigger->dwAction = info->Action;
				trigger->pTriggerSubtype = info->Subtype;

				if (info->DataList && info->DataList->Count != 0)
				{
					trigger->cDataItems = info->DataList->Count;
					trigger->pDataItems = (PSERVICE_TRIGGER_SPECIFIC_DATA_ITEM)PH_AUTO(PhCreateAlloc(info->DataList->Count * sizeof(SERVICE_TRIGGER_SPECIFIC_DATA_ITEM)));

					for (j = 0; j < info->DataList->Count; j++)
					{
						PSERVICE_TRIGGER_SPECIFIC_DATA_ITEM dataItem = &trigger->pDataItems[j];
						PES_TRIGGER_DATA data = (PES_TRIGGER_DATA)info->DataList->Items[j];

						dataItem->dwDataType = data->Type;

						if (data->Type == SERVICE_TRIGGER_DATA_TYPE_STRING)
						{
							dataItem->cbData = (ULONG)data->String->Length + 2; // include null terminator
							dataItem->pData = (PBYTE)data->String->Buffer;
						}
						else if (data->Type == SERVICE_TRIGGER_DATA_TYPE_BINARY)
						{
							dataItem->cbData = data->BinaryLength;
							dataItem->pData = (PBYTE)data->Binary;
						}
						else if (data->Type == SERVICE_TRIGGER_DATA_TYPE_LEVEL)
						{
							dataItem->cbData = sizeof(UCHAR);
							dataItem->pData = (PBYTE)&data->Byte;
						}
						else if (data->Type == SERVICE_TRIGGER_DATA_TYPE_KEYWORD_ANY || data->Type == SERVICE_TRIGGER_DATA_TYPE_KEYWORD_ALL)
						{
							dataItem->cbData = sizeof(ULONG64);
							dataItem->pData = (PBYTE)&data->UInt64;
						}
					}
				}
			}
		}

		BOOLEAN result = TRUE;
		ULONG win32Result = 0;
		if (serviceHandle = PhOpenService((wchar_t*)m_pService->GetName().toStdWString().c_str(), SERVICE_CHANGE_CONFIG))
		{
			if (!ChangeServiceConfig2(serviceHandle, SERVICE_CONFIG_TRIGGER_INFO, &triggerInfo))
			{
				result = FALSE;
				win32Result = GetLastError();
			}

			CloseServiceHandle(serviceHandle);
		}
		else
		{
			result = FALSE;
			win32Result = GetLastError();

			if (win32Result == ERROR_ACCESS_DENIED && !PhGetOwnTokenAttributes().Elevated)
			{
				// todo; restart as service and retry
			}
		}

		PhDeleteAutoPool(&autoPool);
	}

	// Other Tab
	{
		SC_HANDLE serviceHandle = NULL;
        ULONG win32Result = 0;
        BOOLEAN connectedToPhSvc = FALSE;
        ULONG launchProtected;

		launchProtected = ui.protectionType->currentData().toUInt();

        if (m_LaunchProtectedValid && launchProtected != 0 && launchProtected != m_OriginalLaunchProtected)
        {
			if (QMessageBox("TaskExplorer", tr("Setting service protection will prevent the service from being controlled, modified, or deleted. Do you want to continue?"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
				return;
        }

        if (m_GeneralChanged)
        {
			m_GeneralChanged = false;

            SERVICE_PRESHUTDOWN_INFO preshutdownInfo;
            SERVICE_REQUIRED_PRIVILEGES_INFO requiredPrivilegesInfo;
            SERVICE_SID_INFO sidInfo;
            SERVICE_LAUNCH_PROTECTED_INFO launchProtectedInfo;

            if (serviceHandle = PhOpenService((wchar_t*)m_pService->GetName().toStdWString().c_str(), SERVICE_CHANGE_CONFIG))
            {
                win32Result = GetLastError();

				if (win32Result == ERROR_ACCESS_DENIED && !PhGetOwnTokenAttributes().Elevated)
				{
					// todo; restart as service and retry
				}
				// else
				{
                    goto Done;
                }
            }

            if (m_PreshutdownTimeoutValid)
            {
				preshutdownInfo.dwPreshutdownTimeout = ui.preShutdown->text().toULong();

                if (!ChangeServiceConfig2(serviceHandle, SERVICE_CONFIG_PRESHUTDOWN_INFO, &preshutdownInfo))
                {
                    win32Result = GetLastError();
                }
            }

            if (m_RequiredPrivilegesValid)
            {
                wstring sb;
                
                for (int i = 0; i < ui.privilegs->topLevelItemCount(); i++)
                {
					QTreeWidgetItem *item = ui.privilegs->topLevelItem(i);

					sb.append(item->text(0).toStdWString());
					sb.append('\0');
                }

                requiredPrivilegesInfo.pmszRequiredPrivileges = (wchar_t*)sb.c_str();

                if (win32Result == 0 && !ChangeServiceConfig2(serviceHandle, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, &requiredPrivilegesInfo))
                {
                    win32Result = GetLastError();
                }
            }

            if (m_SidTypeValid)
            {
                sidInfo.dwServiceSidType = EspGetServiceSidTypeInteger((wchar_t*)ui.sidType->currentText().toStdWString().c_str());

                if (win32Result == 0 && !ChangeServiceConfig2(serviceHandle, SERVICE_CONFIG_SERVICE_SID_INFO, &sidInfo))
                {
                    win32Result = GetLastError();
                }
            }

            if (m_LaunchProtectedValid)
            {
                launchProtectedInfo.dwLaunchProtected = launchProtected;

                if (!ChangeServiceConfig2(serviceHandle, SERVICE_CONFIG_LAUNCH_PROTECTED, &launchProtectedInfo))
                {
                    // For now, ignore errors here.
                    // win32Result = GetLastError();
                }
            }

Done:
            if (serviceHandle)
                CloseServiceHandle(serviceHandle);

            if (win32Result != 0)
            {
				QMessageBox::warning(NULL, "TaskExplorer", tr("Unable to change service information, error: %1").arg(win32Result));
				// ((PPH_STRING)PH_AUTO(PhGetWin32Message(win32Result)))->Buffer
            }
        }
	}
}

void CWinSvcWindow::reject()
{
	this->close();
}

void CWinSvcWindow::OnBrowse()
{
	QStringList FilePaths = QFileDialog::getOpenFileNames(0, tr("Select binary"), "", tr("All files (*.*)"));
	if (FilePaths.isEmpty())
		return;

	ui.binaryPath->setText(FilePaths.first());
}

void CWinSvcWindow::OnShowPW(int state)
{
	ui.userPW->setEchoMode(state == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password);
}

void CWinSvcWindow::OnPermissions()
{
	m_pService->OpenPermissions();
}

void CWinSvcWindow::OnBrowseRun()
{
	QStringList FilePaths = QFileDialog::getOpenFileNames(0, tr("Select program"), "", tr("All files (*.*)"));
	if (FilePaths.isEmpty())
		return;

	ui.programRun->setText(FilePaths.first());
}

void CWinSvcWindow::OnRestartOptions()
{
	CWinSvcShutdown dialog;
	dialog.SetRestartTime(m_RebootAfter);
	dialog.SetMessageText(m_RebootMessage);
	
	if (!dialog.exec())
		return;

	m_RecoveryChanged = true;
	m_RebootAfter = dialog.GetRestartTime();
	m_RebootMessage = dialog.GetMessageText();
}

void CWinSvcWindow::FixReciveryControls()
{
    SC_ACTION_TYPE action1;
    SC_ACTION_TYPE action2;
    SC_ACTION_TYPE actionS;
    BOOLEAN enableRestart;
    BOOLEAN enableReboot;
    BOOLEAN enableCommand;

	action1 = (SC_ACTION_TYPE)ui.firstFailure->currentData().toULongLong();
    action2 = (SC_ACTION_TYPE)ui.secondFailure->currentData().toULongLong();
    actionS = (SC_ACTION_TYPE)ui.subsequentFailures->currentData().toULongLong();

	ui.actionsForError->setEnabled(m_EnableFlagCheckBox);

    enableRestart = action1 == SC_ACTION_RESTART || action2 == SC_ACTION_RESTART || actionS == SC_ACTION_RESTART;
    enableReboot = action1 == SC_ACTION_REBOOT || action2 == SC_ACTION_REBOOT || actionS == SC_ACTION_REBOOT;
    enableCommand = action1 == SC_ACTION_RUN_COMMAND || action2 == SC_ACTION_RUN_COMMAND || actionS == SC_ACTION_RUN_COMMAND;

	ui.restartService->setEnabled(enableRestart);

	ui.restartOptionsBtn->setEnabled(enableReboot);

	ui.runGroup->setEnabled(enableCommand);
}

void CWinSvcWindow::OnTrigger(QTreeWidgetItem *item, int column)
{
	quint32 index = item->data(0, Qt::UserRole).toUInt();
	if (index >= m_TriggerInfos.size())
		return;

	CWinSvcTrigger WinSvcTrigger(this);
	WinSvcTrigger.SetInfo(m_TriggerInfos[index]);
	if (WinSvcTrigger.exec())
	{
		m_TriggersChanged = true;

		PES_TRIGGER_INFO info = EspCloneTriggerInfo((PES_TRIGGER_INFO)WinSvcTrigger.GetInfo());

		PES_TRIGGER_INFO old_info = (PES_TRIGGER_INFO)m_TriggerInfos[index];
		EspDestroyTriggerInfo(old_info);

		m_TriggerInfos[index] = info;

		UpdateTrigger(item, info);
	}
}

void CWinSvcWindow::OnNewTrigger()
{
	CWinSvcTrigger WinSvcTrigger(this);
	WinSvcTrigger.InitInfo();
	if (WinSvcTrigger.exec())
	{
		m_TriggersChanged = true;

		PES_TRIGGER_INFO info = EspCloneTriggerInfo((PES_TRIGGER_INFO)WinSvcTrigger.GetInfo());
		
		AddTrigger(info);
	}
}

void CWinSvcWindow::OnEditTrigger()
{
	QTreeWidgetItem *item = ui.triggers->currentItem();
	if (!item)
		return;
	OnTrigger(item, 0);
}

void CWinSvcWindow::OnDeleteTrigger()
{
	QTreeWidgetItem *item = ui.triggers->currentItem();
	if (!item)
		return;

	quint32 index = item->data(0, Qt::UserRole).toUInt();
	if (index >= m_TriggerInfos.size())
		return;

	if(QMessageBox("TaskExplorer", tr("Do you want to delete the sellected trigger"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	m_TriggersChanged = true;

	PES_TRIGGER_INFO info = (PES_TRIGGER_INFO)m_TriggerInfos[index];
	EspDestroyTriggerInfo(info);

	m_TriggerInfos.remove(index);

	delete item;
}

void CWinSvcWindow::AddTrigger(void* pInfo)
{
	QTreeWidgetItem* pItem = new QTreeWidgetItem();
	pItem->setData(0, Qt::UserRole, m_TriggerInfos.size());
	m_TriggerInfos.append(pInfo);
	ui.triggers->addTopLevelItem(pItem);

	UpdateTrigger(pItem, pInfo);
}

void CWinSvcWindow::UpdateTrigger(QTreeWidgetItem* pItem, void* pInfo)
{
	PES_TRIGGER_INFO info = (PES_TRIGGER_INFO)pInfo;
	PWSTR triggerString;
	PWSTR actionString;
	PPH_STRING stringUsed;
	INT lvItemIndex;

	EspFormatTriggerInfo(info, &triggerString, &actionString, &stringUsed);

	pItem->setText(0, QString::fromWCharArray(triggerString));
	pItem->setText(1, QString::fromWCharArray(actionString));

	if (stringUsed)
		PhDereferenceObject(stringUsed);
}

void CWinSvcWindow::OnAddPrivilege()
{
	CComboInputDialog comboDalog(this);
	comboDalog.setText(tr("Sellect privilege to add:"));

	NTSTATUS status;
	LSA_HANDLE policyHandle;

	if (!NT_SUCCESS(status = PhOpenLsaPolicy(&policyHandle, POLICY_VIEW_LOCAL_INFORMATION, NULL)))
    {
		QMessageBox::critical(NULL, "TaskExplorer", tr("Unable to open LSA policy, error: %1").arg(status));
		return;
    }

	LSA_ENUMERATION_HANDLE enumContext = 0;
	PPOLICY_PRIVILEGE_DEFINITION buffer;
	ULONG count;

    while (TRUE)
    {
        status = LsaEnumeratePrivileges(
            policyHandle,
            &enumContext,
            (PVOID*)&buffer,
            0x100,
            &count
            );

        if (status == STATUS_NO_MORE_ENTRIES)
            break;
        if (!NT_SUCCESS(status))
            break;

        for (int i = 0; i < count; i++)
        {
			comboDalog.addItem(QString::fromWCharArray(buffer[i].Name.Buffer, buffer[i].Name.Length/sizeof(wchar_t)));
        }

        LsaFreeMemory(buffer);
    }

    LsaClose(policyHandle);

	if (!comboDalog.exec())
		return;

	QString Privilege = comboDalog.value();

	if (!ui.privilegs->findItems(Privilege, Qt::MatchFixedString, 0).isEmpty())
	{
		QMessageBox::warning(NULL, "TaskExplorer", tr("Privilege '%1' was already added.").arg(Privilege));
		return;
	}

	AddPrivilege(Privilege);

	m_RequiredPrivilegesValid = true;
}

void CWinSvcWindow::OnRemovePrivilege()
{
	QTreeWidgetItem *item = ui.privilegs->currentItem();
	if (!item)
		return;

	if(QMessageBox("TaskExplorer", tr("Do you want to delete the sellected privileg"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	delete item;
}

void CWinSvcWindow::AddPrivilege(const QString& Privilege)
{
	wstring wPrivilege = Privilege.toStdWString();

	wchar_t* privilege = (wchar_t*)wPrivilege.c_str();
	size_t privilegeLength = wPrivilege.length();

	QTreeWidgetItem* pItem = new QTreeWidgetItem();
	pItem->setData(0, Qt::UserRole, Privilege);
	ui.privilegs->addTopLevelItem(pItem);

	pItem->setText(0, Privilege);

	PH_STRINGREF privilegeSr;
	privilegeSr.Buffer = privilege;
	privilegeSr.Length = privilegeLength * sizeof(WCHAR);

	PPH_STRING displayName;
	if (PhLookupPrivilegeDisplayName(&privilegeSr, &displayName))
	{
		pItem->setText(1, CastPhString(displayName));
	}
}





