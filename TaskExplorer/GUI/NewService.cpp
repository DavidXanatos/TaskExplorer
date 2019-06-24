#include "stdafx.h"
#include "NewService.h"
#ifdef WIN32
#include "../API/Windows/ProcessHacker.h"
#endif

#define SIP(String, Integer) { (String), (PVOID)(Integer) }

PH_KEY_VALUE_PAIR PhpServiceTypePairs[] =
{
    SIP("Driver", SERVICE_KERNEL_DRIVER),
    SIP("FS driver", SERVICE_FILE_SYSTEM_DRIVER),
    SIP("Own process", SERVICE_WIN32_OWN_PROCESS),
    SIP("Share process", SERVICE_WIN32_SHARE_PROCESS),
    SIP("Own interactive process", SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS),
    SIP("Share interactive process", SERVICE_WIN32_SHARE_PROCESS | SERVICE_INTERACTIVE_PROCESS),
    SIP("User own process", SERVICE_USER_OWN_PROCESS),
    SIP("User own process (instance)", SERVICE_USER_OWN_PROCESS | SERVICE_USERSERVICE_INSTANCE),
    SIP("User share process", SERVICE_USER_SHARE_PROCESS),
    SIP("User share process (instance)", SERVICE_USER_SHARE_PROCESS | SERVICE_USERSERVICE_INSTANCE)
};

PH_KEY_VALUE_PAIR PhpServiceStartTypePairs[] =
{
    SIP("Disabled", SERVICE_DISABLED),
    SIP("Boot start", SERVICE_BOOT_START),
    SIP("System start", SERVICE_SYSTEM_START),
    SIP("Auto start", SERVICE_AUTO_START),
    SIP("Demand start", SERVICE_DEMAND_START)
};

PH_KEY_VALUE_PAIR PhpServiceErrorControlPairs[] =
{
    SIP("Ignore", SERVICE_ERROR_IGNORE),
    SIP("Normal", SERVICE_ERROR_NORMAL),
    SIP("Severe", SERVICE_ERROR_SEVERE),
    SIP("Critical", SERVICE_ERROR_CRITICAL)
};

CNewService::CNewService(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	for (int i = 0; i < sizeof(PhpServiceTypePairs) / sizeof(PH_KEY_VALUE_PAIR); i++)
		ui.svcType->addItem((char*)PhpServiceTypePairs[i].Key, (quint64)PhpServiceTypePairs[i].Value);
	for (int i = 0; i < sizeof(PhpServiceStartTypePairs) / sizeof(PH_KEY_VALUE_PAIR); i++)
		ui.startType->addItem((char*)PhpServiceStartTypePairs[i].Key, (quint64)PhpServiceStartTypePairs[i].Value);
	for (int i = 0; i < sizeof(PhpServiceErrorControlPairs) / sizeof(PH_KEY_VALUE_PAIR); i++)
		ui.errorControl->addItem((char*)PhpServiceErrorControlPairs[i].Key, (quint64)PhpServiceErrorControlPairs[i].Value);
	
	ui.svcType->setCurrentIndex(2); // "Own Process"
	ui.startType->setCurrentIndex(4); // "Demand Start"
	ui.errorControl->setCurrentIndex(0); // "Ignore"

	connect(ui.browseBtn, SIGNAL(pressed()), this, SLOT(OnBrowse()));
	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

void CNewService::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CNewService::accept()
{
	wstring serviceName = ui.scvName->text().toStdWString();
	wstring serviceDisplayName = ui.displayName->text().toStdWString();
	wstring serviceBinaryPath = ui.binaryPath->text().replace("/","\\").toStdWString();

	quint64 serviceType = ui.svcType->currentData().toInt();
	quint64 serviceStartType = ui.startType->currentData().toInt();
	quint64 serviceErrorControl = ui.errorControl->currentData().toInt();

#ifdef WIN32
	NTSTATUS status = STATUS_SUCCESS;
	BOOLEAN success = FALSE;
	SC_HANDLE scManagerHandle;
	SC_HANDLE serviceHandle;

	ULONG win32Result = ERROR_SUCCESS;
	if (scManagerHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE))
    {
		serviceHandle = CreateService(scManagerHandle, serviceName.c_str(), serviceDisplayName.c_str(), SERVICE_CHANGE_CONFIG,
			serviceType, serviceStartType, serviceErrorControl, serviceBinaryPath.c_str(), NULL, NULL, NULL, NULL, L"");

        if(!serviceHandle)
        {
            win32Result = GetLastError();
        }

        CloseServiceHandle(scManagerHandle);
    }
    else
    {
        win32Result = GetLastError();
    }

	if(win32Result != ERROR_SUCCESS)
		QMessageBox::warning(NULL, "TaskExplorer", tr("Failed to create service, error: %1").arg(win32Result));
	else 
		this->close();
#endif
}

void CNewService::reject()
{
	this->close();
}

void CNewService::OnBrowse()
{
	QStringList FilePaths = QFileDialog::getOpenFileNames(0, tr("Select binary"), "", tr("All files (*.*)"));
	if (FilePaths.isEmpty())
		return;

	ui.binaryPath->setText(FilePaths.first());
}