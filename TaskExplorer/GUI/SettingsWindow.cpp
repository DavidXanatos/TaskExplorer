#include "stdafx.h"
#include "SettingsWindow.h"
#include "TaskExplorer.h"

CSettingsWindow::CSettingsWindow(QWidget *parent)
	: QMainWindow(parent)
{
	QWidget* centralWidget = new QWidget();
	this->setCentralWidget(centralWidget);
	ui.setupUi(centralWidget);

	ui.chkLinuxStyle->setTristate(true);
	ui.chkLinuxStyle->setToolTip(tr("Linux CPU Usage shows 100% per core, i.e. if a process is using 2 cores to 100% it will show as 200% total cpu usage.\r\nPartiallyChecked state means apply only to thread list."));
	switch (theConf->GetInt("Options/LinuxStyleCPU", 2))
	{
	case 0:	ui.chkLinuxStyle->setCheckState(Qt::Unchecked); break;
	case 1:	ui.chkLinuxStyle->setCheckState(Qt::Checked); break;
	case 2:	ui.chkLinuxStyle->setCheckState(Qt::PartiallyChecked); break;
	}

	ui.chkShow32->setChecked(theConf->GetBool("Options/Show32", true));

	ui.refreshInterval->setValue(theConf->GetInt("Options/RefreshInterval", 1000));
	ui.graphLength->setValue(theConf->GetInt("Options/GraphLength", 300));

	ui.chkUdpCons->setChecked(theConf->GetBool("Options/UseUDPPseudoConnectins", false));

	ui.chkUndecorate->setChecked(theConf->GetBool("Options/DbgHelpUndecorate", true));
	ui.symbolPath->setText(theConf->GetString("Options/DbgHelpSearchPath", "SRV*C:\\Symbols*https://msdl.microsoft.com/download/symbols"));
	ui.chkSymbolPath->setChecked(theConf->GetBool("Options/DbgHelpSearch", true));


	ui.chkShowTray->setChecked(theConf->GetBool("SysTray/Show", true));

	ui.trayMode->addItem(tr("Show static Icon"), "Icon");
	ui.trayMode->addItem(tr("CPU plot"), "Cpu");
	ui.trayMode->addItem(tr("CPU plot and Memory bar"), "CpuMem");
	ui.trayMode->addItem(tr("CPU plot and RAM bar"), "CpuMem1");
	ui.trayMode->addItem(tr("CPU plot and RAM+Swap bars"), "CpuMem2");
	ui.trayMode->setCurrentIndex(ui.trayMode->findData(theConf->GetString("SysTray/GraphMode", "CpuMem")));

	ui.chkToTray->setChecked(theConf->GetBool("SysTray/CloseToTray", true));

	ui.useDriver->addItem(tr("No"));
	ui.useDriver->addItem(tr("Yes (when elevated)"));
	ui.useDriver->addItem(tr("Yes (always, unsecure)"));
	ui.useDriver->setCurrentIndex(theConf->GetInt("Options/UseDriver", 1));

	ui.chkSoftForce->setCheckState((Qt::CheckState)theConf->GetInt("Options/UseSoftForce", 2));

	ui.persistenceTime->setValue(theConf->GetUInt64("Options/PersistenceTime", 5000));

	ui.chkGetRefServices->setChecked(theConf->GetBool("Options/GetServicesRefModule", true));
	ui.chkTraceDLLs->setChecked(theConf->GetBool("Options/TraceUnloadedModules", false));
	ui.chkUseCycles->setChecked(theConf->GetBool("Options/EnableCycleCpuUsage", false));

	//ui.chkOpenFilePos->setChecked(theConf->GetBool("Options/OpenFileGetPosition", false));

	connect(ui.colorList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnChangeColor(QListWidgetItem*)));

	connect(ui.chkShowTray, SIGNAL(stateChanged(int)), this, SLOT(OnChange()));
	connect(ui.chkUseCycles, SIGNAL(stateChanged(int)), this, SLOT(OnChange()));

	struct SColor
	{
		QString Name;
		QString Description;
		QString Default;
	};

	QList<SColor> Colors;
	Colors.append(SColor { "GridColor", tr("List grid color"), "#808080"});
	Colors.append(SColor { "Background", tr("Default background"), "#FFFFFF"});
	
	// list colors:
#ifdef WIN32
	Colors.append(SColor { "DangerousProcess", tr("Dangerous process"), "#FF0000"});
#endif
	Colors.append(SColor { "NewlyCreated", tr("New items"), "#00FF7F"});
	Colors.append(SColor { "ToBeRemoved", tr("Removed items"), "#F08080"});
	
	Colors.append(SColor { "SystemProcess", tr("System processes"), "#AACCFF"});
	Colors.append(SColor { "UserProcess", tr("Current user processes"), "#FFFF80"});
	Colors.append(SColor { "ServiceProcess", tr("Service processes"), "#80FFFF"});
#ifdef WIN32
	Colors.append(SColor { "JobProcess", tr("Job processes"), "#D49C5C"});
	Colors.append(SColor { "PicoProcess", tr("Pico processes"), "#42A0FF"});
	Colors.append(SColor { "ImmersiveProcess", tr("Immersive processes"), "#FFE6FF"});
	Colors.append(SColor { "NetProcess", tr(".NET processes"), "#DCFF00"});
#endif
	Colors.append(SColor { "ElevatedProcess", tr("Elevated processes"), "#FFBB30"});
#ifdef WIN32
	Colors.append(SColor { "KernelServices", tr("Kernel Services (Driver)"), "#FFC880"});
	Colors.append(SColor { "GuiThread", tr("Gui threads"), "#AACCFF"});
	Colors.append(SColor { "IsInherited", tr("Inherited handles"), "#77FFFF"});
	Colors.append(SColor { "IsProtected", tr("Protected handles"), "#FF77FF"});
#endif

	Colors.append(SColor { "Executable", tr("Executable memory"), "#FF90E0"});
	//

	foreach(const SColor& Color, Colors)
	{
		QListWidgetItem* pItem = new QListWidgetItem(Color.Description);

		StrPair ColorUse = Split2(theConf->GetString("Colors/" + Color.Name, Color.Default), ";");

		// pItem->setFlags(pItem->flags() | Qt::ItemIsUserCheckable); // set checkable flag
		if (Color.Name == "GridColor")
			pItem->setCheckState(theConf->GetBool("Options/ShowGrid", true) ? Qt::Checked : Qt::Unchecked);
		else if (Color.Name != "Background")
		{
			bool bUse = ColorUse.second.isEmpty() || ColorUse.second.compare("true", Qt::CaseInsensitive) == 0 || ColorUse.second.toInt() != 0;
			pItem->setCheckState(bUse ? Qt::Checked : Qt::Unchecked);
		}

		pItem->setData(Qt::UserRole, Color.Name);
		pItem->setBackgroundColor(QColor(ColorUse.first));

		ui.colorList->addItem(pItem);
	}

	connect(ui.buttonBox->button(QDialogButtonBox::Ok), SIGNAL(pressed()), this, SLOT(accept()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(pressed()), this, SLOT(apply()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	OnChange();
}

void CSettingsWindow::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CSettingsWindow::apply()
{
	switch (ui.chkLinuxStyle->checkState())
	{
	case Qt::Unchecked: theConf->GetInt("Options/LinuxStyleCPU", 0); break;
	case Qt::Checked: theConf->GetInt("Options/LinuxStyleCPU", 1); break;
	case Qt::PartiallyChecked:theConf->GetInt("Options/LinuxStyleCPU", 2); break;
	}

	theConf->SetValue("Options/Show32", ui.chkShow32->isChecked());

	theConf->SetValue("Options/RefreshInterval", ui.refreshInterval->value());
	theConf->SetValue("Options/GraphLength", ui.graphLength->value());

	theConf->SetValue("Options/UseUDPPseudoConnectins", ui.chkUdpCons->isChecked());


	theConf->SetValue("Options/DbgHelpUndecorate", ui.chkUndecorate->isChecked());
	theConf->SetValue("Options/DbgHelpSearchPath", ui.symbolPath->text());
	theConf->SetValue("Options/DbgHelpSearch", ui.chkSymbolPath->isChecked());

	theConf->SetValue("SysTray/Show", ui.chkShowTray->isChecked());

	theConf->SetValue("SysTray/GraphMode", ui.trayMode->currentData());

	theConf->SetValue("SysTray/CloseToTray", ui.chkToTray->isChecked());

	theConf->SetValue("Options/UseDriver", ui.useDriver->currentIndex());

	theConf->SetValue("Options/UseSoftForce", (int)ui.chkSoftForce->checkState());


	theConf->SetValue("Options/PersistenceTime", ui.persistenceTime->value());
	
	theConf->SetValue("Options/GetServicesRefModule", ui.chkGetRefServices->isChecked());
	theConf->SetValue("Options/TraceUnloadedModules", ui.chkTraceDLLs->isChecked());
	theConf->SetValue("Options/EnableCycleCpuUsage", ui.chkUseCycles->isChecked());

	//theConf->SetValue("Options/OpenFileGetPosition", ui.chkOpenFilePos->isChecked());



	for (int i = 0; i < ui.colorList->count(); i++)
	{
		QListWidgetItem* pItem = ui.colorList->item(i);
		QString Name = pItem->data(Qt::UserRole).toString();

		QString ColorStr = pItem->backgroundColor().name();

		if (Name == "GridColor")
			theConf->SetValue("Options/ShowGrid", pItem->checkState() == Qt::Checked);
		else if (Name != "Background")
			ColorStr += ";" + QString((pItem->checkState() == Qt::Checked) ? "true" : "false");

		theConf->SetValue("Colors/" + Name, ColorStr);
	}

	emit OptionsChanged();
}

void CSettingsWindow::accept()
{
	apply();

	this->close();
}

void CSettingsWindow::reject()
{
	this->close();
}

void CSettingsWindow::OnChangeColor(QListWidgetItem* pItem)
{
	QColor color = QColorDialog::getColor(pItem->backgroundColor(), this, "Select color");
	if (color.isValid())
		pItem->setBackgroundColor(color);
}

void CSettingsWindow::OnChange()
{
	ui.chkLinuxStyle->setEnabled(!ui.chkUseCycles->isChecked());
	ui.chkToTray->setEnabled(ui.chkShowTray->isChecked());
	ui.trayMode->setEnabled(ui.chkShowTray->isChecked());
}