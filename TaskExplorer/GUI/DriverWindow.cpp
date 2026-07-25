#include "stdafx.h"
#include "DriverWindow.h"
#include "../../MiscHelpers/Common/Settings.h"
#include "../API/Windows/ProcessHacker.h"
#include "../API/Windows/WindowsAPI.h"
#include "TaskExplorer.h"

CDriverWindow::CDriverWindow(QWidget *parent)
	: QMainWindow(parent)
{
	QWidget* centralWidget = new QWidget();
	ui.setupUi(centralWidget);
	this->setCentralWidget(centralWidget);
	this->setWindowTitle("Task Explorer - Driver Options");

	connect(ui.btnGetDynData, SIGNAL(clicked(bool)), this, SLOT(GetDynData()));

	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	ui.chkUseDriver->setChecked(theConf->GetBool("OptionsKSI/KsiEnable", true));
	ui.deviceName->setText(theConf->GetString("OptionsKSI/DeviceName", "KTaskExplorer"));

	Refresh();

	if(((CWindowsAPI*)theAPI)->IsTestSigning())
		ui.signingPolicy->setText(tr("Test Signing Enabled"));
	else if(((CWindowsAPI*)theAPI)->IsCKSEnabled())
		ui.signingPolicy->setText(tr("Signature Required (CKS Enabled)"));
	else
		ui.signingPolicy->setText(tr("Signature Required"));
	
	restoreGeometry(theConf->GetBlob("DriverWindow/Window_Geometry"));

	m_TimerId = startTimer(250);
}

CDriverWindow::~CDriverWindow()
{
	theConf->SetBlob("DriverWindow/Window_Geometry", saveGeometry());

	if(m_TimerId != -1)
		killTimer(m_TimerId);
}

void CDriverWindow::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CDriverWindow::accept()
{
	theConf->SetValue("OptionsKSI/KsiEnable", ui.chkUseDriver->isChecked());
	//theConf->SetValue("OptionsKSI/DeviceName", ui.deviceName->text());

	this->close();
}

void CDriverWindow::reject()
{
	this->close();
}

void CDriverWindow::timerEvent(QTimerEvent *e)
{
	if (e->timerId() != m_TimerId) 
	{
		QMainWindow::timerEvent(e);
		return;
	}

	Refresh();
}

void CDriverWindow::Refresh()
{
	if (CServicePtr pService = theAPI->GetService(ui.deviceName->text()))
	{
		ui.driverStatus->setText(pService->GetStateString());
		ui.driverStatus->setToolTip(pService->GetFileName());
	}
	else
	{
		ui.driverStatus->setText(tr("Not installed"));
		ui.driverStatus->setToolTip("");
	}

	if (KphCommsIsConnected())
	{
		ui.connection->setText(tr("Connected"));

		if(g_KsiDynDataLoaded)
			ui.dyn_data->setText(tr("DynData loaded"));
		else
			ui.dyn_data->setText(tr("DynData NOT loaded"));

		QString sLevel;
		KPH_LEVEL level = KphLevelEx(FALSE);
		switch (level)
		{
		case KphLevelNone: sLevel = tr("None"); break;
		case KphLevelMin: sLevel = tr("Minimal"); break;
		case KphLevelLow: sLevel = tr("Low"); break;
		case KphLevelMed: sLevel = tr("Medium"); break;
		case KphLevelHigh: sLevel = tr("High"); break;
		case KphLevelMax: sLevel = tr("Maximum"); break;
		}
		ui.verification->setText(sLevel);

		QStringList Info;
		KPH_PROCESS_STATE processState = KphGetCurrentProcessState();
		if ((processState != 0) && (processState & KPH_PROCESS_STATE_MAXIMUM) != KPH_PROCESS_STATE_MAXIMUM)
		{
			if (!BooleanFlagOn(processState, KPH_PROCESS_SECURELY_CREATED))
				Info.append("not securely created");
			if (!BooleanFlagOn(processState, KPH_PROCESS_VERIFIED_PROCESS))
				Info.append("unverified primary image");
			if (!BooleanFlagOn(processState, KPH_PROCESS_PROTECTED_PROCESS))
				Info.append("inactive protections");
			if (!BooleanFlagOn(processState, KPH_PROCESS_NO_UNTRUSTED_IMAGES))
				Info.append("unsigned images (likely an unsigned plugin)");
			if (!BooleanFlagOn(processState, KPH_PROCESS_NOT_BEING_DEBUGGED))
				Info.append("process is being debugged");
			if (!BooleanFlagOn(processState, KPH_PROCESS_NO_WRITABLE_FILE_OBJECT))
				Info.append("writable file object");
			if (!BooleanFlagOn(processState, KPH_PROCESS_CREATE_NOTIFICATION))
				Info.append("missing create notification");
			//if (!BooleanFlagOn(processState, KPH_PROCESS_NO_VERIFY_TIMEOUT))
			//	Info.append("verify time out");
			if ((processState & KPH_PROCESS_STATE_MINIMUM) != KPH_PROCESS_STATE_MINIMUM)
				Info.append("tampered primary image");
		}

		ui.verification->setToolTip(Info.join("\n"));
	}
	else
	{
		ui.connection->setText(tr("Disconnected"));

		ui.dyn_data->setText(tr("N/A"));

		ui.verification->setText(tr("N/A"));
		ui.verification->setToolTip("");
	}
}

bool IsOnARM64();
extern "C" NTSTATUS KsiGetDynData(const QString &Path, _Out_ PBYTE* DynData, _Out_ PULONG DynDataLength, _Out_ PBYTE* Signature, _Out_ PULONG SignatureLength);

void CDriverWindow::GetDynData()
{
	QString AppDir = QApplication::applicationDirPath().replace("/", "\\");
	STATUS Status = TryUpdateDynData(AppDir);
	if (Status)
	{
		QString FileName = theConf->GetString("OptionsKSI/FileName", "KTaskExplorer.sys");
		if (!FileName.contains("\\")) 
		{
			if (IsOnARM64())
				FileName = AppDir + "\\ARM64\\" + FileName;
			else
				FileName = AppDir + "\\AMD64\\" + FileName;
		}
		FileName = FileName.replace("/", "\\");

		PBYTE dynData = NULL;
		ULONG dynDataLength;
		PBYTE signature = NULL;
		ULONG signatureLength;

		NTSTATUS status = KsiGetDynData(Split2(FileName, "\\", true).first, &dynData, &dynDataLength, &signature, &signatureLength);
		if (!NT_SUCCESS(status))
			Status = ERR("Unsupported windows version.", STATUS_UNKNOWN_REVISION);
		else
		{
			status = KphActivateDynData(dynData, dynDataLength, signature, signatureLength);
			if (!NT_SUCCESS(status))
				Status = ERR("KphActivateDynData Failed.", status);
			else
				g_KsiDynDataLoaded = true;
		}

		if (signature)
			PhFree(signature);
		if (dynData)
			PhFree(dynData);
	}
	if (!Status)
		CTaskExplorer::CheckErrors(QList<STATUS>() << Status);
}