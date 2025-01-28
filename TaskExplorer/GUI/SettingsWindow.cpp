#include "stdafx.h"
#include "SettingsWindow.h"
#include "TaskExplorer.h"
#include "../../MiscHelpers/Common/Settings.h"

int CSettingsWindow__Chk2Int(Qt::CheckState state)
{
	switch (state) {
	case Qt::Unchecked: return 0;
	case Qt::Checked: return 1;
	default:
	case Qt::PartiallyChecked: return 2;
	}
}

Qt::CheckState CSettingsWindow__Int2Chk(int state)
{
	switch (state) {
	case 0: return Qt::Unchecked;
	case 1: return Qt::Checked;
	default:
	case 2: return Qt::PartiallyChecked;
	}
}

CSettingsWindow::CSettingsWindow(QWidget *parent)
	: QMainWindow(parent)
{
	QWidget* centralWidget = new QWidget();
	ui.setupUi(centralWidget);
	this->setCentralWidget(centralWidget);
	this->setWindowTitle(tr("Task Explorer - Settings"));

	ui.uiLang->addItem("International English", "");
	QDir langDir(QApplication::applicationDirPath() + "/translations/");
	foreach(const QString& langFile, langDir.entryList(QStringList("taskexplorer_*.qm"), QDir::Files))
	{
		QString Code = langFile.mid(13, langFile.length() - 13 - 3);
		QLocale Locale(Code);
		QString Lang = Locale.nativeLanguageName();
		ui.uiLang->addItem(Lang, Code);
	}
	ui.uiLang->setCurrentIndex(ui.uiLang->findData(theConf->GetString("General/Language")));

	ui.chkUseCycles->setChecked(theConf->GetBool("Options/EnableCycleCpuUsage", true));
	ui.chkLinuxStyle->setTristate(true);
	ui.chkLinuxStyle->setToolTip(tr("Linux CPU Usage shows 100% per core, i.e. if a process is using 2 cores to 100% it will show as 200% total cpu usage.\r\nPartiallyChecked state means apply only to thread std::list."));
	switch (theConf->GetInt("Options/LinuxStyleCPU", 2))
	{
	case 0:	ui.chkLinuxStyle->setCheckState(Qt::Unchecked); break;
	case 1:	ui.chkLinuxStyle->setCheckState(Qt::Checked); break;
	case 2:	ui.chkLinuxStyle->setCheckState(Qt::PartiallyChecked); break;
	}
	ui.chkClearZeros->setChecked(theConf->GetBool("Options/ClearZeros", true));

	ui.chkMaxThread->setChecked(theConf->GetBool("Options/ShowMaxThread", false));

	ui.chkShow32->setChecked(theConf->GetBool("Options/Show32", true));

	ui.chkDarkTheme->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("MainWindow/DarkTheme", 2)));

	ui.highlightCount->setValue(theConf->GetInt("Options/HighLoadHighlightCount", 5));

	ui.refreshInterval->setValue(theConf->GetInt("Options/RefreshInterval", 1000));
	ui.graphLength->setValue(theConf->GetInt("Options/GraphLength", 300));

	ui.chkUdpCons->setChecked(theConf->GetBool("Options/UseUDPPseudoConnectins", false));
	ui.chkLanPlot->setChecked(theConf->GetBool("Options/ShowLanPlot", false));
	ui.chkSmartDns->setChecked(theConf->GetBool("Options/MonitorDnsCache", false));
	ui.chkReverseDns->setChecked(theConf->GetBool("Options/UserReverseDns", false));

	ui.chkUndecorate->setChecked(theConf->GetBool("Options/DbgHelpUndecorate", true));
	ui.symbolPath->setText(theConf->GetString("Options/DbgHelpSearchPath", "SRV*C:\\Symbols*https://msdl.microsoft.com/download/symbols"));

	int DbgHelpSearch = theConf->GetInt("Options/DbgHelpSearch", 2);
	if (DbgHelpSearch == 2) {
		ui.chkSymbolPath->setTristate(true);
		ui.chkSymbolPath->setCheckState(Qt::PartiallyChecked);
	} else
		ui.chkSymbolPath->setChecked(DbgHelpSearch == 1);

	ui.onClose->addItem(tr("Close to Tray"), "ToTray");
	ui.onClose->addItem(tr("Prompt before Close"), "Prompt");
	ui.onClose->addItem(tr("Close"), "Close");
	ui.onClose->setCurrentIndex(ui.onClose->findData(theConf->GetString("Options/OnClose", "ToTray")));


	ui.chkShowTray->setChecked(theConf->GetBool("SysTray/Show", true));

	ui.trayMode->addItem(tr("Show static Icon"), "Icon");
	ui.trayMode->addItem(tr("CPU plot"), "Cpu");
	ui.trayMode->addItem(tr("CPU plot and Memory bar"), "CpuMem");
	ui.trayMode->addItem(tr("CPU plot and RAM bar"), "CpuMem1");
	ui.trayMode->addItem(tr("CPU plot and RAM+Swap bars"), "CpuMem2");
	ui.trayMode->setCurrentIndex(ui.trayMode->findData(theConf->GetString("SysTray/GraphMode", "CpuMem")));

	ui.chkSandboxie->setChecked(theConf->GetBool("Options/UseSandboxie", false));

	ui.chkSoftForce->setCheckState((Qt::CheckState)theConf->GetInt("Options/UseSoftForce", 2));

	ui.highlightTime->setValue(theConf->GetUInt64("Options/HighlightTime", 2500));
	ui.persistenceTime->setValue(theConf->GetUInt64("Options/PersistenceTime", 5000));

	ui.processName->addItem(tr("Description (Binary name)"), 1);
	ui.processName->addItem(tr("Binary name (Description)"), 2);
	ui.processName->addItem(tr("Binary name only"), 0);
	ui.processName->setCurrentIndex(ui.processName->findData(theConf->GetInt("Options/ShowProcessDescr", 1)));

	ui.chkParents->setChecked(theConf->GetBool("Options/EnableParrentRetention", true));
	ui.chkGetRefServices->setChecked(theConf->GetBool("Options/GetServicesRefModule", true));
	ui.chkTraceDLLs->setChecked(theConf->GetBool("Options/TraceUnloadedModules", false));

	//ui.chkOpenFilePos->setChecked(theConf->GetBool("Options/OpenFileGetPosition", false));

	connect(ui.colorList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnChangeColor(QListWidgetItem*)));

	connect(ui.chkShowTray, SIGNAL(stateChanged(int)), this, SLOT(OnChange()));
	//connect(ui.chkUseCycles, SIGNAL(stateChanged(int)), this, SLOT(OnChange()));

	connect(ui.chkSimpleCopy, SIGNAL(stateChanged(int)), this, SLOT(OnChange()));

	ui.chkSimpleCopy->setChecked(theConf->GetBool("Options/PanelCopySimple", false));
	ui.maxCellWidth->setValue(theConf->GetInt("Options/PanelCopyMaxCellWidth", 0));
	ui.cellSeparator->setText(theConf->GetString("Options/PanelCopyCellSeparator", "\\t"));


	foreach(const CTaskExplorer::SColor& Color, theGUI->GetAllColors())
	{
		QListWidgetItem* pItem = new QListWidgetItem(Color.Description);

		StrPair ColorUse = Split2(theConf->GetString("Colors/" + Color.Name, Color.Default), ";");

		// pItem->setFlags(pItem->flags() | Qt::ItemIsUserCheckable); // set checkable flag
		if (Color.Name == "GridColor")
			pItem->setCheckState(theConf->GetBool("Options/ShowGrid", true) ? Qt::Checked : Qt::Unchecked);
		else if (Color.Name != "Background" 
		 && Color.Name != "GraphBack" && Color.Name != "GraphFront"
		 && Color.Name != "PlotBack" && Color.Name != "PlotFront" && Color.Name != "PlotGrid" 
		)
		{
			bool bUse = ColorUse.second.isEmpty() || ColorUse.second.compare("true", Qt::CaseInsensitive) == 0 || ColorUse.second.toInt() != 0;
			pItem->setCheckState(bUse ? Qt::Checked : Qt::Unchecked);
		}

		pItem->setData(Qt::UserRole, Color.Name);
		pItem->setBackground(QColor(ColorUse.first));

		ui.colorList->addItem(pItem);
	}

	connect(ui.buttonBox->button(QDialogButtonBox::Ok), SIGNAL(pressed()), this, SLOT(accept()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(pressed()), this, SLOT(apply()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	restoreGeometry(theConf->GetBlob("SettingsWindow/Window_Geometry"));

	OnChange();
}

CSettingsWindow::~CSettingsWindow()
{
	theConf->SetBlob("SettingsWindow/Window_Geometry",saveGeometry());
}

void CSettingsWindow::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CSettingsWindow::apply()
{
	theConf->SetValue("General/Language", ui.uiLang->currentData());

	theConf->SetValue("Options/EnableCycleCpuUsage", ui.chkUseCycles->isChecked());
	switch (ui.chkLinuxStyle->checkState())
	{
	case Qt::Unchecked: theConf->GetInt("Options/LinuxStyleCPU", 0); break;
	case Qt::Checked: theConf->GetInt("Options/LinuxStyleCPU", 1); break;
	case Qt::PartiallyChecked:theConf->GetInt("Options/LinuxStyleCPU", 2); break;
	}
	theConf->SetValue("Options/ClearZeros", ui.chkClearZeros->isChecked());

	theConf->SetValue("Options/ShowMaxThread", ui.chkMaxThread->isChecked());

	theConf->SetValue("Options/Show32", ui.chkShow32->isChecked());

	theConf->SetValue("MainWindow/DarkTheme", CSettingsWindow__Chk2Int(ui.chkDarkTheme->checkState()));

	theConf->SetValue("Options/HighLoadHighlightCount", ui.highlightCount->value());

	theConf->SetValue("Options/RefreshInterval", ui.refreshInterval->value());
	theConf->SetValue("Options/GraphLength", ui.graphLength->value());

	theConf->SetValue("Options/UseUDPPseudoConnectins", ui.chkUdpCons->isChecked());
	theConf->SetValue("Options/ShowLanPlot", ui.chkLanPlot->isChecked());
	theConf->SetValue("Options/MonitorDnsCache", ui.chkSmartDns->isChecked());
	theConf->SetValue("Options/UserReverseDns", ui.chkReverseDns->isChecked());


	theConf->SetValue("Options/DbgHelpUndecorate", ui.chkUndecorate->isChecked());
	theConf->SetValue("Options/DbgHelpSearchPath", ui.symbolPath->text());
	if(ui.chkSymbolPath->checkState() != Qt::PartiallyChecked)
		theConf->SetValue("Options/DbgHelpSearch", ui.chkSymbolPath->isChecked() ? 1 : 0);

	theConf->SetValue("SysTray/OnClose", ui.onClose->currentData());

	theConf->SetValue("SysTray/Show", ui.chkShowTray->isChecked());

	theConf->SetValue("SysTray/GraphMode", ui.trayMode->currentData());

	theConf->SetValue("Options/UseSandboxie", ui.chkSandboxie->isChecked());

	theConf->SetValue("Options/UseSoftForce", (int)ui.chkSoftForce->checkState());

	theConf->SetValue("Options/HighlightTime", ui.highlightTime->value());
	theConf->SetValue("Options/PersistenceTime", ui.persistenceTime->value());
	
	theConf->SetValue("Options/ShowProcessDescr", ui.processName->currentData());

	theConf->SetValue("Options/EnableParrentRetention", ui.chkParents->isChecked());
	theConf->SetValue("Options/GetServicesRefModule", ui.chkGetRefServices->isChecked());
	theConf->SetValue("Options/TraceUnloadedModules", ui.chkTraceDLLs->isChecked());

	//theConf->SetValue("Options/OpenFileGetPosition", ui.chkOpenFilePos->isChecked());

	theConf->SetValue("Options/PanelCopySimple", ui.chkSimpleCopy->isChecked());
	theConf->SetValue("Options/PanelCopyMaxCellWidth", ui.maxCellWidth->value());
	theConf->SetValue("Options/PanelCopyCellSeparator", ui.cellSeparator->text());


	for (int i = 0; i < ui.colorList->count(); i++)
	{
		QListWidgetItem* pItem = ui.colorList->item(i);
		QString Name = pItem->data(Qt::UserRole).toString();

		QString ColorStr = pItem->background().color().name();

		if (Name == "GridColor")
			theConf->SetValue("Options/ShowGrid", pItem->checkState() == Qt::Checked);
		else if (Name != "Background"
		 && Name != "GraphBack" && Name != "GraphFront"
		 && Name != "PlotBack" && Name != "PlotFront" && Name != "PlotGrid" 
		)
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
	QColor color = QColorDialog::getColor(pItem->background().color(), this, "Select color");
	if (color.isValid())
		pItem->setBackground(color);
}

void CSettingsWindow::OnChange()
{
	//ui.chkLinuxStyle->setEnabled(!ui.chkUseCycles->isChecked());

	QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui.onClose->model());
	QStandardItem *item = model->item(0);
	item->setFlags((!ui.chkShowTray->isChecked()) ? item->flags() & ~Qt::ItemIsEnabled : item->flags() | Qt::ItemIsEnabled);

	ui.trayMode->setEnabled(ui.chkShowTray->isChecked());

	ui.cellSeparator->setEnabled(ui.chkSimpleCopy->isChecked());
	ui.maxCellWidth->setEnabled(!ui.chkSimpleCopy->isChecked());
}