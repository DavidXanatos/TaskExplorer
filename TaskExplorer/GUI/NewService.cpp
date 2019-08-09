#include "stdafx.h"
#include "NewService.h"
#ifdef WIN32
#include "../API/Windows/ProcessHacker/PhSvc.h"
#endif


CNewService::CNewService(QWidget *parent)
	: QMainWindow(parent)
{
	QWidget* centralWidget = new QWidget();
	this->setCentralWidget(centralWidget);
	ui.setupUi(centralWidget);

#ifdef WIN32
	for (int i = 0; i < 10; i++)
		ui.svcType->addItem((char*)PhpServiceTypePairs[i].Key, (quint64)PhpServiceTypePairs[i].Value);
	for (int i = 0; i < 5; i++)
		ui.startType->addItem((char*)PhpServiceStartTypePairs[i].Key, (quint64)PhpServiceStartTypePairs[i].Value);
	for (int i = 0; i < 4; i++)
		ui.errorControl->addItem((char*)PhpServiceErrorControlPairs[i].Key, (quint64)PhpServiceErrorControlPairs[i].Value);
#endif

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
	{
		QMessageBox::information(NULL, "TaskExplorer", tr("Successfully creted service: %1").arg(ui.scvName->text()));
		this->close();
	}
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
