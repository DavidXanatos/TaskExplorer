#include "stdafx.h"
#include "SettingsWindow.h"
#include "TaskExplorer.h"
#include "../../MiscHelpers/Common/Settings.h"
#include "../../MiscHelpers/Archive/ArchiveFS.h"
#include "OnlineUpdater.h"
#include <QFontDialog>
#include <QDesktopServices>

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

	FixTriStateBoxPallete(this);

	ui.tabWidget->setTabPosition(QTabWidget::West);
	ui.tabWidget->setTabIcon(0, QIcon(":/Actions/Design"));
	ui.tabWidget->setTabIcon(1, QIcon(":/Actions/MiscOptions"));
	ui.tabWidget->setTabIcon(2, QIcon(":/Actions/GUI"));
	ui.tabWidget->setTabIcon(3, QIcon(":/Actions/Settings"));
	ui.tabWidget->setTabIcon(4, QIcon(":/Actions/Support"));

	ui.tabWidget->setCurrentIndex(0);

	int size = 16.0;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	size *= (QApplication::desktop()->logicalDpiX() / 96.0); // todo Qt6
#endif

	{
		ui.uiLang->addItem(tr("Auto Detection"), "");
		ui.uiLang->addItem(tr("No Translation"), "native");

		QString langDir;
		C7zFileEngineHandler LangFS("lang", this);
		if (LangFS.Open(QApplication::applicationDirPath() + "/translations.7z"))
			langDir = LangFS.Prefix() + "/";
		else
			langDir = QApplication::applicationDirPath() + "/translations/";

		foreach(const QString & langFile, QDir(langDir).entryList(QStringList("taskexplorer_*.qm"), QDir::Files))
		{
			QString Code = langFile.mid(13, langFile.length() - 13 - 3);
			QLocale Locale(Code);
			QString Lang = Locale.nativeLanguageName();
			ui.uiLang->addItem(Lang, Code);
		}
		ui.uiLang->setCurrentIndex(ui.uiLang->findData(theConf->GetString("General/Language")));
	}

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
	ui.chkFusionTheme->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("MainWindow/UseFusionTheme", 2)));

	ui.cmbDPI->addItem(tr("None"), 0);
	ui.cmbDPI->addItem(tr("Native"), 1);
	ui.cmbDPI->addItem(tr("Qt"), 2);
	ui.cmbDPI->setCurrentIndex(theConf->GetInt("Options/DPIScaling", 1));

	int FontScales[] = { 75,100,125,150,175,200,225,250,275,300,350,400, 0 };
	for (int* pFontScales = FontScales; *pFontScales != 0; pFontScales++)
		ui.cmbFontScale->addItem(tr("%1").arg(*pFontScales), *pFontScales);
	//ui.cmbFontScale->setCurrentIndex(ui.cmbFontScale->findData(theConf->GetInt("Options/FontScaling", 100)));
	ui.cmbFontScale->setCurrentText(QString::number(theConf->GetInt("Options/FontScaling", 100)));

	// UI Font
	ui.btnSelectUiFont->setIcon(QPixmap(":/Actions/Font").scaled(size, size));
	ui.btnSelectUiFont->setToolTip(tr("Select font"));
	ui.btnResetUiFont->setIcon(QPixmap(":/Actions/ResetFont").scaled(size, size));
	ui.btnResetUiFont->setToolTip(tr("Reset font"));

	connect(ui.btnSelectUiFont, SIGNAL(clicked(bool)), this, SLOT(OnSelectUiFont()));
	connect(ui.btnResetUiFont, SIGNAL(clicked(bool)), this, SLOT(OnResetUiFont()));
	ui.lblUiFont->setText(QApplication::font().family());

	// Updater
	ui.cmbInterval->addItem(tr("Every Day"), 1 * 24 * 60 * 60);
	ui.cmbInterval->addItem(tr("Every Week"), 7 * 24 * 60 * 60);
	ui.cmbInterval->addItem(tr("Every 2 Weeks"), 14 * 24 * 60 * 60);
	ui.cmbInterval->addItem(tr("Every 30 days"), 30 * 24 * 60 * 60);

	ui.cmbUpdate->addItem(tr("Ignore"), "ignore");
	ui.cmbUpdate->addItem(tr("Notify"), "notify");
	ui.cmbUpdate->addItem(tr("Download & Notify"), "download");
	ui.cmbUpdate->addItem(tr("Download & Install"), "install");

	ui.cmbRelease->addItem(tr("Notify"), "notify");
	ui.cmbRelease->addItem(tr("Download & Notify"), "download");
	ui.cmbRelease->addItem(tr("Download & Install"), "install");

	ui.chkAutoUpdate->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/CheckForUpdates", 2)));

	int UpdateInterval = theConf->GetInt("Options/UpdateInterval", UPDATE_INTERVAL);
	int pos = ui.cmbInterval->findData(UpdateInterval);
	if (pos == -1)
		ui.cmbInterval->setCurrentText(QString::number(UpdateInterval));
	else
		ui.cmbInterval->setCurrentIndex(pos);

	QString ReleaseChannel = theConf->GetString("Options/ReleaseChannel", "stable");
	ui.radStable->setChecked(ReleaseChannel == "stable");
	ui.radPreview->setChecked(ReleaseChannel == "preview");

	UpdateUpdater();

	ui.cmbUpdate->setCurrentIndex(ui.cmbUpdate->findData(theConf->GetString("Options/OnNewUpdate", "ignore")));
	ui.cmbRelease->setCurrentIndex(ui.cmbRelease->findData(theConf->GetString("Options/OnNewRelease", "download")));

	connect(ui.lblCurrent, SIGNAL(linkActivated(const QString&)), this, SLOT(OnUpdate(const QString&)));
	connect(ui.lblStable, SIGNAL(linkActivated(const QString&)), this, SLOT(OnUpdate(const QString&)));
	connect(ui.lblPreview, SIGNAL(linkActivated(const QString&)), this, SLOT(OnUpdate(const QString&)));

	connect(ui.chkAutoUpdate, SIGNAL(toggled(bool)), this, SLOT(UpdateUpdater()));
	connect(ui.radStable, SIGNAL(toggled(bool)), this, SLOT(UpdateUpdater()));
	connect(ui.radPreview, SIGNAL(toggled(bool)), this, SLOT(UpdateUpdater()));
	//

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

	ui.chkParents->setChecked(theConf->GetBool("Options/EnableParentRetention", true));
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

	connect(ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(OnTab()));

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
	theConf->SetValue("MainWindow/UseFusionTheme", CSettingsWindow__Chk2Int(ui.chkFusionTheme->checkState()));


	theConf->SetValue("Options/UIFont", ui.lblUiFont->text());
	theConf->SetValue("Options/DPIScaling", ui.cmbDPI->currentData());
	int Scaling = ui.cmbFontScale->currentText().toInt();
	if (Scaling < 75)
		Scaling = 75;
	else if (Scaling > 500)
		Scaling = 500;
	theConf->SetValue("Options/FontScaling", Scaling);


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

	theConf->SetValue("Options/EnableParentRetention", ui.chkParents->isChecked());
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

	// Updater
	theConf->SetValue("Options/CheckForUpdates", CSettingsWindow__Chk2Int(ui.chkAutoUpdate->checkState()));

	int UpdateInterval = ui.cmbInterval->currentData().toInt();
	if (!UpdateInterval)
		UpdateInterval = ui.cmbInterval->currentText().toInt();
	if (!UpdateInterval)
		UpdateInterval = UPDATE_INTERVAL;
	theConf->SetValue("Options/UpdateInterval", UpdateInterval);

	QString ReleaseChannel;
	if (ui.radStable->isChecked())
		ReleaseChannel = "stable";
	else if (ui.radPreview->isChecked())
		ReleaseChannel = "preview";
	if(!ReleaseChannel.isEmpty()) theConf->SetValue("Options/ReleaseChannel", ReleaseChannel);

	theConf->SetValue("Options/OnNewUpdate", ui.cmbUpdate->currentData());
	theConf->SetValue("Options/OnNewRelease", ui.cmbRelease->currentData());
	//

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

void CSettingsWindow::OnSelectUiFont()
{
	bool ok;
	auto newFont = QFontDialog::getFont(&ok, QApplication::font(), this);
	if (!ok) return;
	ui.lblUiFont->setText(newFont.family());
}

void CSettingsWindow::OnResetUiFont()
{
	QFont defaultFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
	ui.lblUiFont->setText(defaultFont.family());
}

void CSettingsWindow::UpdateUpdater()
{
	if (!ui.chkAutoUpdate->isChecked())
	{
		ui.cmbInterval->setEnabled(false);
		ui.cmbUpdate->setEnabled(false);
		ui.cmbRelease->setEnabled(false);
		ui.lblRevision->setText(QString());
		ui.lblRelease->setText(QString());
	}
	else
	{
		ui.cmbInterval->setEnabled(true);
		ui.cmbUpdate->setEnabled(true);
		ui.cmbRelease->setEnabled(true);

		ui.lblRevision->setText(QString());
		ui.lblRelease->setText(QString());
	}
}

void CSettingsWindow::OnTab()
{
	// Check if we're on the updater tab (index 4)
	if (ui.tabWidget->currentIndex() == 4)
	{
		if (ui.lblCurrent->text().isEmpty()) {
			if (ui.chkAutoUpdate->checkState() == Qt::Checked)
				GetUpdates();
			else
				ui.lblCurrent->setText(tr("<a href=\"check\">Check Now</a>"));
		}
	}
}

void CSettingsWindow::GetUpdates()
{
	QVariantMap Params;
	Params["channel"] = "all";
	theGUI->GetOnlineUpdater()->GetUpdates(this, SLOT(OnUpdateData(const QVariantMap&, const QVariantMap&)), Params);
}

QString CSettingsWindow__MkVersion(const QString& Name, const QVariantMap& Releases)
{
	QVariantMap Release = Releases[Name].toMap();
	QString Version = Release.value("version").toString();
	int iUpdate = Release["update"].toInt();
	if(iUpdate) Version += QChar('a' + (iUpdate - 1));
	return QString("<a href=\"%1\">%2</a>").arg(Name, Version);
}

void CSettingsWindow::OnUpdateData(const QVariantMap& Data, const QVariantMap& Params)
{
	if (Data.isEmpty() || Data["error"].toBool())
		return;

	m_UpdateData = Data;
	QVariantMap Releases = m_UpdateData["releases"].toMap();
	ui.lblCurrent->setText(tr("%1 (Current)").arg(COnlineUpdater::GetCurrentVersion()));
	ui.lblStable->setText(CSettingsWindow__MkVersion("stable", Releases));
	ui.lblPreview->setText(CSettingsWindow__MkVersion("preview", Releases));
}

void CSettingsWindow::OnUpdate(const QString& Channel)
{
	if (Channel == "check") {
		GetUpdates();
		return;
	}

	QVariantMap Releases = m_UpdateData["releases"].toMap();
	QVariantMap Release = Releases[Channel].toMap();

	QString VersionStr = Release["version"].toString();
	if (VersionStr.isEmpty())
		return;

	QString InfoUrl = Release["infoUrl"].toString();
	if (InfoUrl.isEmpty())
		InfoUrl = "https://xanasoft.com/go.php?to=sbie-get";
	QDesktopServices::openUrl(InfoUrl);
}