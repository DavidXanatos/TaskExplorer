#include "stdafx.h"
#include "DriverWindow.h"
#include "../Common/Settings.h"
#include "../API/Windows/ProcessHacker.h"
#include "../API/Windows/WindowsAPI.h"

CDriverWindow::CDriverWindow(QWidget *parent)
	: QMainWindow(parent)
{
	QWidget* centralWidget = new QWidget();
	ui.setupUi(centralWidget);
	this->setCentralWidget(centralWidget);
	this->setWindowTitle("Task Explorer - Driver Options");

	connect(ui.btnStart, SIGNAL(pressed()), this, SLOT(OnStop()));
	connect(ui.btnConnect, SIGNAL(pressed()), this, SLOT(OnConnect()));
	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	ui.chkUseDriver->setChecked(theConf->GetBool("Options/UseDriver", true));
	ui.securityLevel->setCurrentIndex(theConf->GetInt("Options/DriverSecurityLevel", KphSecurityPrivilegeCheck));

	ui.fileName->addItem(tr("(Auto Selection)"), "");
	QDir dir(QApplication::applicationDirPath());
	foreach(const QString& FileName, dir.entryList(QStringList() << "*.sys", QDir::Files))
		ui.fileName->addItem(FileName, FileName);

	if (theConf->GetString("Options/DriverFile").isEmpty())
	{
		ui.autoSelection->setVisible(true);
		ui.deviceName->setReadOnly(true);

		QPair<QString, QString> Driver = ((CWindowsAPI*)theAPI)->SellectDriver();
		ui.fileName->setCurrentIndex(ui.fileName->findData(Driver.second));
		ui.deviceName->setText(Driver.first);
	}
	else
	{
		ui.autoSelection->setVisible(false);
		ui.deviceName->setReadOnly(false);

		ui.fileName->setCurrentIndex(ui.fileName->findData(((CWindowsAPI*)theAPI)->GetDriverFileName()));
		ui.deviceName->setText(((CWindowsAPI*)theAPI)->GetDriverDeviceName());
	}

	m_HoldValues = false;
	connect(ui.fileName, SIGNAL(currentIndexChanged(int)), this, SLOT(OnDriverFile()));

	Refresh();

	ui.signingPolicy->setText(((CWindowsAPI*)theAPI)->IsTestSigning() ? tr("Test Signing Enabled") : tr("Signature Required"));

	restoreGeometry(theConf->GetBlob("DriverWindow/Window_Geometry"));

	m_TimerId = startTimer(1000);
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
	theConf->SetValue("Options/UseDriver", ui.chkUseDriver->isChecked());
	theConf->SetValue("Options/DriverSecurityLevel", ui.securityLevel->currentIndex());

	if (ui.autoSelection->isVisible())
	{
		theConf->SetValue("Options/DriverFile", "");
		theConf->SetValue("Options/DriverDevice", "");
	}
	else
	{
		theConf->SetValue("Options/DriverFile", ui.fileName->currentText());
		theConf->SetValue("Options/DriverDevice", ui.deviceName->text());
	}

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

void CDriverWindow::OnDriverFile()
{
	if (m_HoldValues)
		return;

	QString FileName = ui.fileName->currentData().toString();
	if (FileName.isEmpty())
	{
		ui.autoSelection->setVisible(true);
		ui.deviceName->setReadOnly(true);

		m_HoldValues = true;
		QPair<QString, QString> Driver = ((CWindowsAPI*)theAPI)->SellectDriver();
		ui.fileName->setCurrentIndex(ui.fileName->findData(Driver.second));
		ui.deviceName->setText(Driver.first);
		m_HoldValues = false;
	}
	else
	{
		ui.autoSelection->setVisible(false);
		ui.deviceName->setReadOnly(false);

		if (FileName.contains("XProcessHacker", Qt::CaseInsensitive))
			ui.deviceName->setText("XProcessHacker3");
		else if (FileName.contains("KProcessHacker", Qt::CaseInsensitive))
			ui.deviceName->setText("KProcessHacker3");
		else
			ui.deviceName->setText("");
	}
}

void CDriverWindow::Refresh()
{
	CServicePtr pService = theAPI->GetService(ui.deviceName->text());
	if (pService.isNull())
	{
		ui.btnStart->setEnabled(false);
		ui.btnStart->setText(tr("Start"));
		ui.driverStatus->setText(tr("Not installed"));
		ui.driverStatus->setToolTip("");
	}
	else
	{
		ui.driverStatus->setText(pService->GetStateString());
		ui.driverStatus->setToolTip(pService->GetFileName());
		if (pService->IsStopped())
		{
			ui.btnStart->setText(tr("Start"));
			ui.btnStart->setEnabled(true); // theAPI->RootAvaiable()
		}
		else
		{
			ui.btnStart->setText(tr("Stop"));
			if (pService->IsRunning(true))
				ui.btnStart->setEnabled(true); // theAPI->RootAvaiable()
			else
				ui.btnStart->setEnabled(false);
		}
	}

	if (KphIsConnected())
	{
		ui.btnConnect->setEnabled(true);
		ui.btnConnect->setText(tr("Disconnect"));

		ui.connection->setText(tr("Connected"));

		ui.verification->setText(KphIsVerified() ? tr("Verified") : tr("NOT Verified"));

		ULONG Features = 0;
		if (NT_SUCCESS(KphGetFeatures(&Features)))
			ui.features->setText(tr("0x%1").arg(Features, 8, 16, QChar('0')));
		else
			ui.features->setText(tr("N/A"));
	}
	else
	{
		ui.btnConnect->setEnabled((!pService.isNull() || !ui.fileName->currentText().isEmpty()) && !ui.deviceName->text().isEmpty()); // theAPI->RootAvaiable()
		ui.btnConnect->setText(pService.isNull() ? tr("Install") : tr("Connect"));

		if (quint32 Status = ((CWindowsAPI*)theAPI)->GetDriverStatus())
			ui.connection->setText(tr("Error: 0x%1").arg(Status, 8, 16, QChar('0')));
		else
			ui.connection->setText(tr("Disconnected"));

		ui.verification->setText(tr("N/A"));
		ui.features->setText(tr("N/A"));
	}
}

void CDriverWindow::OnConnect()
{
	if (KphIsConnected())
	{
		KphDisconnect();
	}
	else
	{
		STATUS Status = ((CWindowsAPI*)theAPI)->InitDriver(ui.deviceName->text(), ui.fileName->currentText(), ui.securityLevel->currentIndex());
		if (Status.IsError())
			QMessageBox::critical(this, "Task Explorer", Status.GetText());
	}
}

void CDriverWindow::OnStop()
{
	CServicePtr pService = theAPI->GetService(ui.deviceName->text());
	if (!pService.isNull())
	{
		if (pService->IsRunning(true))
			pService->Stop();
		else if (pService->IsStopped())
			pService->Start();
	}
}