#include "stdafx.h"
#include "OnlineUpdater.h"
#include "Version.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/OtherFunctions.h"
#include "TaskExplorer.h"
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include "../MiscHelpers/Common/CheckableMessageBox.h"
#include "../API/Windows/WinHelper.h"
#include "../API/Windows/WinAdmin.h"
#include <QMessageBox>
#include "../UpdUtil/UpdDefines.h"
#include <QCryptographicHash>
#include <windows.h>
#include <QRandomGenerator>

#define RELEASE_FILE			"release.json"

#ifdef QT_NO_SSL
#error Qt requires Open SSL support for the updater to work
#endif

#ifdef _DEBUG

// mess with a dummy installation when debugging

#undef VERSION_MJR
#define VERSION_MJR		1
#undef VERSION_MIN
#define VERSION_MIN 	6
#undef VERSION_REV
#define VERSION_REV 	0
#undef VERSION_UPD
#define VERSION_UPD 	0

#define DUMMY_PATH "C:\\Projects\\TaskExplorer\\Build\\Test_0.99.5"

#endif

COnlineUpdater::COnlineUpdater(QObject* parent) : QObject(parent)
{
	m_IgnoredUpdates = theConf->GetStringList("Options/IgnoredUpdates");

	LoadState();
}

void COnlineUpdater::StartNetworkJob(CNetworkJob* pJob, const QUrl& Url)
{
	if (!m_RequestManager) 
		m_RequestManager = new CNetworkAccessManager(30 * 1000, this);

	QNetworkRequest Request = QNetworkRequest(Url);
	//Request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
	Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	//Request.setRawHeader("Accept-Encoding", "gzip");
	pJob->SetReply(m_RequestManager->get(Request));
}

void COnlineUpdater::GetUpdates(QObject* receiver, const char* member, const QVariantMap& Params, const CProgressDialogPtr& pDialog)
{
	QUrlQuery Query;
	Query.addQueryItem("action", "update");
	Query.addQueryItem("software", "task-explorer");
	//Query.addQueryItem("version", QString::number(VERSION_MJR) + "." + QString::number(VERSION_MIN) + "." + QString::number(VERSION_REV) + "." + QString::number(VERSION_UPD));
	Query.addQueryItem("version", QString::number(VERSION_MJR) + "." + QString::number(VERSION_MIN) + "." + QString::number(VERSION_REV));
	Query.addQueryItem("system", "windows-" + QSysInfo::kernelVersion() + "-" + QSysInfo::currentCpuArchitecture());
	Query.addQueryItem("language", QLocale::system().name());
#ifdef _DEBUG
	//Query.addQueryItem("language", "zh_CN");
	Query.addQueryItem("debug", "1");
#endif

	if (Params.contains("channel")) 
		Query.addQueryItem("channel", Params["channel"].toString());
	else {
		QString ReleaseChannel = theConf->GetString("Options/ReleaseChannel", "stable");
		Query.addQueryItem("channel", ReleaseChannel);
	}

	Query.addQueryItem("auto", Params["manual"].toBool() ? "0" : "1");

	if (!Params["manual"].toBool()) {
		int UpdateInterval = theConf->GetInt("Options/UpdateInterval", UPDATE_INTERVAL); // in seconds
		Query.addQueryItem("interval", QString::number(UpdateInterval));
	}

#ifdef _DEBUG
	QString Test = Query.toString();
#endif

	QUrl Url("https://xanasoft.com/update.php");
	Url.setQuery(Query);

	CNetworkJob* pJob = new CGetUpdatesJob(Params, this);
	pJob->SetProgressDialog(pDialog);
	StartNetworkJob(pJob, Url);
	QObject::connect(pJob, SIGNAL(UpdateData(const QVariantMap&, const QVariantMap&)), receiver, member, Qt::QueuedConnection);
}

void CGetUpdatesJob::Finish(QNetworkReply* pReply)
{
	QVariantMap Data;

	auto err = pReply->error();
	if (err != QNetworkReply::NoError) 
	{
		//m_pProgress->Finish(ERR(OtherError, QVariantList() << tr("Updater Error: %1").arg(err), err));
		Data["error"] = true;
		if(err == 99)
			Data["errorMsg"] = tr("Network Error");
		else
			Data["errorMsg"] = tr("Code %1").arg(err);
	}
	else
	{
		QByteArray Reply = pReply->readAll();

		Data = QJsonDocument::fromJson(Reply).toVariant().toMap();
	}

	if(m_pProgress)
		m_pProgress->OnFinished();

	emit UpdateData(Data, m_Params);
}

void COnlineUpdater::DownloadFile(const QString& Url, QObject* receiver, const char* member, const QVariantMap& Params, const CProgressDialogPtr& pDialog)
{
	CNetworkJob* pJob = new CGetFileJob(Params, this);
	pJob->SetProgressDialog(pDialog);
	StartNetworkJob(pJob, Url);
	QObject::connect(pJob, SIGNAL(Download(const QString&, const QVariantMap&)), receiver, member, Qt::QueuedConnection);
}

void CGetFileJob::Finish(QNetworkReply* pReply)
{
	quint64 Size = pReply->bytesAvailable();

	if(m_pProgress)
		m_pProgress->ShowProgress("");

	QString FilePath = m_Params["path"].toString();
	if (FilePath.isEmpty()) {
		QString Name = pReply->request().url().fileName();
		if (Name.isEmpty())
			Name = "unnamed_download.tmp";
		FilePath = ((COnlineUpdater*)parent())->GetUpdateDir(true) + "/" + Name;
	}

	QFile File(FilePath);
	if (File.open(QFile::WriteOnly)) {
		while (pReply->bytesAvailable() > 0)
			File.write(pReply->read(4096));
		File.flush();
		QDateTime Date = m_Params["setDate"].toDateTime();
		if(Date.isValid())
			File.setFileTime(Date, QFileDevice::FileModificationTime);
		File.close();
	}

	if(m_pProgress)
		m_pProgress->OnFinished();

	if (File.size() != Size) {
		QMessageBox::critical(theGUI, "TaskExplorer", tr("Failed to download file from: %1").arg(pReply->request().url().toString()));
		return;
	}

	emit Download(FilePath, m_Params);
}

// Update Handling

QStringList COnlineUpdater::ScanUpdateFiles(const QVariantMap& Update)
{
	QString AppDir = QApplication::applicationDirPath();
#ifdef DUMMY_PATH
	AppDir = DUMMY_PATH;
#endif

	QStringList Files;

	foreach(const QVariant vFile, Update["files"].toList()) {
		QVariantMap File = vFile.toMap();
		QCryptographicHash qHash(QCryptographicHash::Sha256);
		QFile qFile(AppDir + "\\" + File["path"].toString());
		if (qFile.open(QFile::ReadOnly)) {
			qHash.addData(&qFile);
			qFile.close();
		}
		if (qHash.result() != QByteArray::fromHex(File["hash"].toByteArray()))
			Files.append(File["path"].toString());
	}

	return Files;
}

void COnlineUpdater::LoadState()
{
	m_UpdateVersion = ParseVersionStr(theConf->GetString("Updater/UpdateVersion"), &m_UpdateNumber);
	if (VersionToInt(m_UpdateVersion) < GetCurrentVersionInt() || (m_UpdateVersion == GetCurrentVersion() && m_UpdateNumber <= GetCurrentUpdate())) // it seems update has been applied or has been superseded
		ClearUpdate();
	else
	{
		QVariantMap UpdateData = QJsonDocument::fromJson(ReadFileAsString(GetUpdateDir() + "/" UPDATE_FILE).toUtf8()).toVariant().toMap();
		if (UpdateData.isEmpty() || UpdateData["version"].toString() != m_UpdateVersion || UpdateData["update"].toInt() != m_UpdateNumber)
			ClearUpdate(); // update data is missing or invalid
		else
		{
			QStringList ChangedFiles = ScanUpdateFiles(UpdateData);
			if (ChangedFiles.isEmpty()) // check if this update has already been applied
				ClearUpdate();
			else
			{
				m_UpdatePrepared = true;
				for (const QString& file : ChangedFiles) {
					if (!QFile::exists(GetUpdateDir() + "/" + file)) {
						m_UpdatePrepared = false;
						break;
					}
				}
			}
		}
	}

	m_InstallerVersion = ParseVersionStr(theConf->GetString("Updater/InstallerVersion"));
	m_InstallerPath = theConf->GetString("Updater/InstallerPath");
	if (!IsVersionNewer(m_InstallerVersion))
		ClearRelease();
	else
	{
		if (!QFile::exists(m_InstallerPath)) {
			m_InstallerPath.clear();
			theConf->DelValue("Updater/InstallerPath");
		}
	}

	UpdateState();
}

void COnlineUpdater::UpdateState()
{
	QString OnNewUpdate = GetOnNewUpdateOption();
	bool bCanApplyUpdate = (OnNewUpdate == "install");
	bool bCanDownloadUpdate = bCanApplyUpdate || (OnNewUpdate == "download");

	QString OnNewRelease = GetOnNewReleaseOption();
	bool bCanRunInstaller = (OnNewRelease == "install");
	bool bCanDownloadInstaller = bCanRunInstaller || (OnNewRelease == "download");

	if (!m_InstallerPath.isEmpty() && bCanRunInstaller)
		m_CheckState = eRunInstall;
	else if (m_UpdatePrepared && bCanApplyUpdate)
		m_CheckState = eApplyUpdate;

	else if (!m_InstallerVersion.isEmpty() && bCanDownloadInstaller)
		m_CheckState = eDownloadInstall;
	else if (!m_UpdateVersion.isEmpty() && bCanDownloadUpdate)
		m_CheckState = eDownloadUpdate;

	emit StateChanged();
}

void COnlineUpdater::ClearUpdate()
{
	QFile::remove(GetUpdateDir() + "/" UPDATE_FILE);

	m_UpdateVersion.clear();
	m_UpdateNumber = 0;
	m_UpdatePrepared = false;
	theConf->DelValue("Updater/UpdateVersion");
}

void COnlineUpdater::ClearRelease()
{
	if (!m_InstallerPath.isEmpty()) {
		QFile::remove(m_InstallerPath);
		QFile::remove(m_InstallerPath + ".sig");
	}
	QFile::remove(GetUpdateDir() + "/" RELEASE_FILE);

	m_InstallerVersion.clear();
	m_InstallerPath.clear();
	theConf->DelValue("Updater/InstallerVersion");
	theConf->DelValue("Updater/InstallerPath");
}

void COnlineUpdater::Process() 
{
	int UpdateInterval = theConf->GetInt("Options/UpdateInterval", UPDATE_INTERVAL); // in seconds
	QDateTime CurretnDate = QDateTime::currentDateTime();
	time_t NextUpdateCheck = theConf->GetUInt64("Options/NextCheckForUpdates", 0);
	if (NextUpdateCheck == 0 || NextUpdateCheck > CurretnDate.addDays(31).toSecsSinceEpoch()) { // no check made yet or invalid value
		NextUpdateCheck = CurretnDate.addSecs(UpdateInterval).toSecsSinceEpoch();
		theConf->SetValue("Options/NextCheckForUpdates", NextUpdateCheck);
	}

	int iCheckUpdates = theConf->GetInt("Options/CheckForUpdates", 2);
	//int iCheckTweaks = theConf->GetInt("Options/UpdateTweaks", 2);
	if (m_CheckState == eIdle && (iCheckUpdates != 0 /*|| iCheckTweaks != 0*/) && CheckInternet())
	{
		if(CurretnDate.toSecsSinceEpoch() >= NextUpdateCheck)
		{
			if (iCheckUpdates == 2)
			{
				bool bCheck = false;
				iCheckUpdates = CCheckableMessageBox::question(theGUI, "TaskExplorer", tr("Do you want to check if there is a new version of TaskExplorer?")
					, tr("Don't show this message again."), &bCheck, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes, QMessageBox::Information) == QDialogButtonBox::Ok ? 1 : 0;

				if (bCheck) {
					theConf->SetValue("Options/CheckForUpdates", iCheckUpdates);
				}
			}

			if (iCheckUpdates != 0 /*|| iCheckTweaks == 1*/)
			{
				// schedule next check in 12 h in case this one fails
				theConf->SetValue("Options/NextCheckForUpdates", CurretnDate.addSecs(12 * 60 * 60).toSecsSinceEpoch());
		
				CheckForUpdates(false);
			}
			else
				theConf->SetValue("Options/NextCheckForUpdates", CurretnDate.addSecs(UpdateInterval).toSecsSinceEpoch());
		}
	}

	if (m_CheckState == eDownloadInstall || m_CheckState == eDownloadUpdate)
	{
		if (CheckInternet())
		{
			if (m_CheckState == eDownloadInstall)
			{
				QVariantMap ReleaseData = QJsonDocument::fromJson(ReadFileAsString(GetUpdateDir() + "/" RELEASE_FILE).toUtf8()).toVariant().toMap();
				DownloadInstaller(ReleaseData);
			}
			else if (m_CheckState == eDownloadUpdate)
			{
				QVariantMap UpdateData = QJsonDocument::fromJson(ReadFileAsString(GetUpdateDir() + "/" UPDATE_FILE).toUtf8()).toVariant().toMap();
				QStringList ChangedFiles = ScanUpdateFiles(UpdateData);
				DownloadUpdate(UpdateData, ChangedFiles);
			}
			m_CheckState = eIdle;
		}
	}
	else if (m_CheckState == eApplyUpdate || m_CheckState == eRunInstall)
	{
		// When auto install/apply is active wait for the user to be idle
#ifndef _DEBUG
		LASTINPUTINFO lastInput;
		GetLastInputInfo(&lastInput);
		DWORD dwIdleTime= (GetTickCount() - lastInput.dwTime) / 1000;

		if(dwIdleTime > theConf->GetInt("Options/UpdateIdleTime", 30*60)) // default 30 minutes
#endif
		{
			if (m_CheckState == eApplyUpdate)
				ApplyUpdate(true);
			else if (m_CheckState == eRunInstall)
				RunInstaller(true);
			m_CheckState = eIdle;
		}
	}
}

void COnlineUpdater::CheckForUpdates(bool bManual)
{
#ifdef _DEBUG
	if (QApplication::keyboardModifiers() & Qt::ControlModifier)
		bManual = false;
#endif

	m_CheckState = bManual ? eManual : eAuto;

	CProgressDialogPtr pProgress;
	if (bManual) {
		pProgress = CProgressDialogPtr(new CProgressDialog(tr("Checking for updates..."), theGUI));
		pProgress->Show();
	}

	QVariantMap Params;
    GetUpdates(this, SLOT(OnUpdateData(const QVariantMap&, const QVariantMap&)), Params, pProgress);
}

void COnlineUpdater::OnUpdateData(const QVariantMap& Data, const QVariantMap& Params)
{
	if (Data.isEmpty() || Data["error"].toBool()) {
		QString Error = Data.isEmpty() ? tr("server not reachable") : Data["errorMsg"].toString();
		//theGUI->OnLogMessage(tr("Failed to check for updates, error: %1").arg(Error), m_CheckMode != eManual); // todo
		if (m_CheckState == eManual)
			QMessageBox::critical(theGUI, "TaskExplorer", tr("Failed to check for updates, error: %1").arg(Error));
		m_CheckState = eIdle;
		return;
	}

	bool bNothing = true;
	bool bAuto = m_CheckState != eManual;
	
	QString LabelMsg = Data["labelMsg"].toString();
	theConf->SetValue("Updater/LabelMessage", LabelMsg);

	if (HandleUserMessage(Data))
		bNothing = false;

	m_LastUpdate = QDateTime::currentDateTime();
	
	if (HandleUpdate(Data))
		bNothing = false;

	//int iCheckTweaks = theConf->GetInt("Options/UpdateTweaks", 2);
	//if (bNothing && iCheckTweaks != 0) 
	//{
	//	QVariantMap Tweaks = Data["tweaks"].toMap();
	//	uint32 uRevision = Tweaks["revision"].toUInt();
	//	
	//	if (uRevision > theCore->TweakManager()->GetRevision())
	//	{
	//		QString DownloadUrl = Tweaks["downloadUrl"].toString();

	//		CProgressDialogPtr pProgress;
	//		if (!bAuto) {
	//			pProgress = CProgressDialogPtr(new CProgressDialog(tr("Downloading installer..."), theGUI));
	//			pProgress->Show();
	//		}

	//		QVariantMap Params;
	//		DownloadFile(DownloadUrl, this, SLOT(OnTweaksDownload(const QString&, const QVariantMap&)), Params, pProgress);
	//	}
	//}

	if (bAuto) {
		int UpdateInterval = theConf->GetInt("Options/UpdateInterval", UPDATE_INTERVAL); // in seconds
		theConf->SetValue("Options/NextCheckForUpdates", QDateTime::currentDateTime().addSecs(UpdateInterval).toSecsSinceEpoch());
	}
	else if (bNothing)  {
		QMessageBox::information(theGUI, "TaskExplorer", tr("No new updates found, your TaskExplorer is up-to-date.\n"
			"\nNote: The update check is often behind the latest GitHub release to ensure that only tested updates are offered."));
	}

	emit StateChanged();
}

bool COnlineUpdater::HandleUpdate(const QVariantMap& Data)
{
	bool Ret = false;

	bool bManual = m_CheckState == eManual;

	QString OnNewUpdate = GetOnNewUpdateOption();
	bool bIgnoreUpdate = !bManual && OnNewUpdate == "ignore";
	bool bCanApplyUpdate = (OnNewUpdate == "install");
	bool bCanDownloadUpdate = bCanApplyUpdate || (OnNewUpdate == "download");

	QString OnNewRelease = GetOnNewReleaseOption();
	bool bIgnoreInstaller = !bManual && OnNewRelease == "ignore";
	bool bCanRunInstaller = (OnNewRelease == "install");
	bool bCanDownloadInstaller = bCanRunInstaller || (OnNewRelease == "download");

	QVariantMap ReleaseData = Data["release"].toMap();
	QString ReleaseStr = ReleaseData["version"].toString();
	if (VersionToInt(ReleaseStr) > VersionToInt(m_InstallerVersion))
		ClearRelease(); // current installer is outdated, clear it
	if (IsVersionNewer(ReleaseStr)) 
	{
		if (bManual || (!bIgnoreInstaller && !m_IgnoredUpdates.contains(ReleaseStr))) 
		{
			Ret = true;

			m_InstallerVersion = ReleaseStr;
			theConf->SetValue("Updater/InstallerVersion", ReleaseStr);

			QJsonDocument doc(QJsonValue::fromVariant(ReleaseData).toObject());
			WriteStringToFile(GetUpdateDir(true) + "/" RELEASE_FILE, doc.toJson());
		}
	}

	QVariantMap UpdateData = Data["update"].toMap();
	QString UpdateStr = UpdateData["version"].toString();
	int iUpdateNumber = UpdateData["update"].toInt();
	bool bNewerVersion;
	if (VersionToInt(UpdateStr) > VersionToInt(m_UpdateVersion) || (UpdateStr == m_UpdateVersion && iUpdateNumber > m_UpdateNumber))
		ClearUpdate(); // current update is outdated, clear it
	QStringList ChangedFiles;
	if ((bNewerVersion = IsVersionNewer(UpdateStr)) || (UpdateStr == GetCurrentVersion()))
	{
		if (iUpdateNumber) UpdateStr += QChar('a' + (iUpdateNumber - 1));
		if (bNewerVersion || iUpdateNumber > GetCurrentUpdate()) 
		{
			ChangedFiles = ScanUpdateFiles(UpdateData);
			if (ChangedFiles.isEmpty()) // check if this update has already been applied
				theConf->SetValue("Updater/CurrentUpdate", MakeVersionStr(UpdateData)); // cache result
			else if (!bIgnoreUpdate)
			{
				if (bManual || !m_IgnoredUpdates.contains(UpdateStr))
				{
					Ret = true;

					m_UpdateVersion = UpdateStr;
					theConf->SetValue("Updater/UpdateVersion", MakeVersionStr(UpdateData)); // cache result

					QJsonDocument doc(QJsonValue::fromVariant(UpdateData).toObject());
					WriteStringToFile(GetUpdateDir(true) + "/" UPDATE_FILE, doc.toJson());
				}
			}
		}
	}

	
	if (!m_InstallerVersion.isEmpty()) 
	{
		if (m_InstallerPath.isEmpty())
		{
			int Download = 1;
			if ((!bManual && bCanDownloadInstaller) || (Download = AskDownload(ReleaseData, true)))
			{
				if (Download == 1)
				{
					if (bManual)
						DownloadInstaller(ReleaseData);
					else
						m_CheckState = eDownloadInstall;
				}
				else if (Download == -1)
					ClearRelease();
			}
		}
		else
		{
			if (bManual) 
				RunInstaller(false);
			else if(bCanRunInstaller)
				m_CheckState = eRunInstall;
		}
	}
	else if (!m_UpdateVersion.isEmpty())
	{
		if (!m_UpdatePrepared)
		{
			int Download = 1;
			if ((!bManual && bCanDownloadUpdate) || (Download = AskDownload(UpdateData, true)))
			{
				if (Download == 1)
				{
					if (bManual)
						DownloadUpdate(UpdateData, ChangedFiles);
					else
						m_CheckState = eDownloadUpdate;
				}
				else if (Download == -1)
					ClearUpdate();
			}
		}
		else
		{
			if (bManual) 
				ApplyUpdate(ChangedFiles, false);
			else if (bCanApplyUpdate)
				m_CheckState = eApplyUpdate;
		}
	}

	emit StateChanged();
	return Ret;
}

int COnlineUpdater::AskDownload(const QVariantMap& Data, bool bAuto)
{
	bool bManual = m_CheckState == eManual;

	QString VersionStr = MakeVersionStr(Data);

	QString UpdateMsg = Data["infoMsg"].toString();
	QString UpdateUrl = Data["infoUrl"].toString();
	
	QString FullMessage = !UpdateMsg.isEmpty() ? UpdateMsg : 
		tr("<p>There is a new version of TaskExplorer available.<br /><font color='red'><b>New version:</b></font> <b>%1</b></p>").arg(VersionStr);

	QVariantMap Installer = Data["installer"].toMap();
	QString DownloadUrl = Installer["downloadUrl"].toString();

	enum EAction
	{
		eNone = 0,
		eDownload,
		eNotify,
	} Action = eNone;

	if (bAuto && !DownloadUrl.isEmpty()) {
		Action = eDownload;
		FullMessage += tr("<p>Do you want to download the installer?</p>");
	}
	else if (bAuto && Data.contains("files")) {
		Action = eDownload;
		FullMessage += tr("<p>Do you want to download the updates?</p>");
	}
	else if (!UpdateUrl.isEmpty()) {
		Action = eNotify;
		FullMessage += tr("<p>Do you want to go to the <a href=\"%1\">download page</a>?</p>").arg(UpdateUrl);
	}

	CCheckableMessageBox mb(theGUI);
	mb.setWindowTitle("TaskExplorer");
	QIcon ico(QLatin1String(":/TaskExplorer.png"));
	mb.setIconPixmap(ico.pixmap(64, 64));
	//mb.setTextFormat(Qt::RichText);
	mb.setText(FullMessage);
	mb.setCheckBoxText(tr("Don't show this update anymore."));
	mb.setCheckBoxVisible(!bManual);

	if (Action != eNone) {
		mb.setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel);
		mb.setDefaultButton(QDialogButtonBox::Yes);
	} else
		mb.setStandardButtons(QDialogButtonBox::Ok);

	mb.exec();

	if (mb.clickedStandardButton() == QDialogButtonBox::Yes)
	{
		if (Action == eDownload) 
			return 1; // download
		else
			QDesktopServices::openUrl(UpdateUrl);
	}
	else 
	{
		if (mb.isChecked())
		{
			theConf->SetValue("Options/IgnoredUpdates", m_IgnoredUpdates << VersionStr);
			return -1; // ignore and remember
		}

		if (mb.clickedStandardButton() == QDialogButtonBox::Cancel) 
			return -1; // ignore only once
	}

	return 0; // do nothing
}

bool COnlineUpdater::DownloadInstaller(const QVariantMap& Release)
{
	bool bManual = m_CheckState == eManual;

	QVariantMap Installer = Release["installer"].toMap();
	QString DownloadUrl = Installer["downloadUrl"].toString();
	if (DownloadUrl.isEmpty())
		return false;

	CProgressDialogPtr pProgress;
	if (bManual) {
		pProgress = CProgressDialogPtr(new CProgressDialog(tr("Downloading installer..."), theGUI));
		pProgress->Show();
	}

	QVariantMap Params;
	Params["version"] = MakeVersionStr(Release);
	Params["signature"] = Installer["signature"];
    DownloadFile(DownloadUrl, this, SLOT(OnInstallerDownload(const QString&, const QVariantMap&)), Params, pProgress);

	return true;
}

void COnlineUpdater::OnInstallerDownload(const QString& Path, const QVariantMap& Params)
{
	bool bManual = m_CheckState == eManual;

	QString VersionStr = Params["version"].toString();
	QByteArray Signature = Params["signature"].toByteArray();

	QFile SigFile(Path + ".sig");
	if (SigFile.open(QFile::WriteOnly)) {
		SigFile.write(QByteArray::fromBase64(Signature));
		SigFile.close();
	}

	m_InstallerPath = Path;
	theConf->SetValue("Updater/InstallerPath", Path);

	if (bManual)
		RunInstaller(false);
	else
	{
		QString OnNewRelease = GetOnNewReleaseOption();
		bool bCanRunInstaller = (OnNewRelease == "install");
		RunInstaller(bCanRunInstaller);
	}
}

//void COnlineUpdater::OnTweaksDownload(const QString& Path, const QVariantMap& Params)
//{
//	bool bManual = m_CheckState == eManual;
//
//	QFile File(Path);
//	if (File.open(QFile::ReadOnly)) 
//	{
//		QByteArray TweakIni = File.readAll();
//		File.close();
//
//		if (!TweakIni.isEmpty())
//		{
//			theCore->WriteConfigFile("\\Tweaks.ini", TweakIni);
//
//			if (bManual)
//				QMessageBox::information(theGUI, "TaskExplorer", tr("Tweaks have been updated."));
//		}
//	}
//	File.remove();
//}

bool COnlineUpdater::RunInstaller(bool bSilent)
{
	QString FilePath = theConf->GetString("Updater/InstallerPath");
	if (FilePath.isEmpty() || !QFile::exists(FilePath)) {
		theConf->DelValue("Updater/InstallerPath");
		theConf->DelValue("Updater/InstallerVersion");
		return false;
	}

	if (!bSilent) {
		QString Message = tr("<p>A new TaskExplorer installer has been downloaded to the following location:</p><p><a href=\"%2\">%1</a></p><p>Do you want to begin the installation? If any programs are running in secure enclaves, they will be terminated!</p>")
			.arg(FilePath).arg("File:///" + Split2(FilePath, "/", true).first);
		int Ret = QMessageBox("TaskExplorer", Message, QMessageBox::Information, QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape, QMessageBox::Cancel, theGUI).exec();
		if (Ret == QMessageBox::Cancel) {
			ClearRelease();
			UpdateState();
		}
		if (Ret != QMessageBox::Yes)
			return false;
	}

	if (RunInstaller2(FilePath, true)) {
		if (bSilent)
			theConf->DelValue("Updater/InstallerVersion");
		QApplication::quit();
		return true;
	}

	return false;
}

bool COnlineUpdater::RunInstaller2(const QString& FilePath, bool bSilent)
{
//	if (bSilent) 
//	{
//		QStringList Params;
//		Params.append("run_setup");
//		Params.append(QString(FilePath).replace("/", "\\"));
//#ifndef _DEBUG
//		Params.append("/embedded");
//#else
//		Params.append("/pause");
//#endif
//
//		RESULT(int) Result = theCore->RunUpdateUtility(Params, 1);
//		if (!Result.IsError())
//			return true;
//		// else fallback to ShellExecuteEx
//		if (theConf->GetBool("Options/UpdateNoFallback", false))
//			return false;
//	}

	std::wstring wFile = QString(FilePath).replace("/", "\\").toStdWString();
	std::wstring wParams;
#ifndef _DEBUG
	wParams = L"/SILENT";
#endif

	return RunElevated(wFile, wParams, 15*60*1000) == 0;
}

bool COnlineUpdater::HandleUserMessage(const QVariantMap& Data)
{
	QString UserMsg = Data["userMsg"].toString();
	if (UserMsg.isEmpty())
		return false;
	
	QString MsgHash = QCryptographicHash::hash(Data["userMsg"].toByteArray(), QCryptographicHash::Md5).toHex().left(8);
	if (!m_IgnoredUpdates.contains(MsgHash))
	{
		QString FullMessage = UserMsg;
		QString InfoUrl = Data["infoUrl"].toString();
		if (!InfoUrl.isEmpty())
			FullMessage += tr("<p>Do you want to go to the <a href=\"%1\">info page</a>?</p>").arg(InfoUrl);

		CCheckableMessageBox mb(theGUI);
		mb.setWindowTitle("TaskExplorer");
			
		QByteArray MsgIcon = QByteArray::fromBase64(Data["msgIcon"].toByteArray());
		if (!MsgIcon.isEmpty())
		{
			QPixmap pixmap;
			pixmap.loadFromData(MsgIcon, "PNG");
			mb.setIconPixmap(pixmap);
		}
		else
		{
			QIcon ico(QLatin1String(":/TaskExplorer.png"));
			mb.setIconPixmap(ico.pixmap(64, 64));
		}
			
		//mb.setTextFormat(Qt::RichText);
		mb.setText(UserMsg);
		mb.setCheckBoxText(tr("Don't show this announcement in the future."));
			
		if (!InfoUrl.isEmpty()) {
			mb.setStandardButtons(QDialogButtonBox::Yes | QDialogButtonBox::No);
			mb.setDefaultButton(QDialogButtonBox::Yes);
		}
		else
			mb.setStandardButtons(QDialogButtonBox::Ok);

		mb.exec();

		if (mb.isChecked())
			theConf->SetValue("Options/IgnoredUpdates", m_IgnoredUpdates << MsgHash);

		if (mb.clickedStandardButton() == QDialogButtonBox::Yes)
		{
			QDesktopServices::openUrl(InfoUrl);
		}

		return true;
	}
	
	return false;
}

// Incremental updating

bool COnlineUpdater::DownloadUpdate(const QVariantMap& Update, const QStringList& ChangedFiles)
{
	bool bManual = m_CheckState == eManual;

	QStringList Args;
	Args.append("update");
	Args.append("/step:prepare");
	if (!ChangedFiles.isEmpty())
		Args.append("/files:" + ChangedFiles.join(";").replace("/", "\\"));
#ifndef _DEBUG
	Args.append("/embedded");
#else
	Args.append("/pause");
#endif
	Args.append("/temp:" + GetUpdateDir().replace("/", "\\"));
#ifdef DUMMY_PATH
	Args.append("/path:" DUMMY_PATH);
#endif

	CProgressDialogPtr pProgress;
	if (bManual) {
		pProgress = CProgressDialogPtr(new CProgressDialog(tr("Downloading updates..."), theGUI));
		pProgress->Show();
	}

	QVariantMap Params;
	Params["files"] = ChangedFiles;
	Params["version"] = MakeVersionStr(Update);
	CUtilityJob* pJob = new CUtilityJob(Params, this);
	pJob->SetProgressDialog(pProgress);
	QObject::connect(pJob, &CUtilityJob::Finished, this, &COnlineUpdater::OnUpdateDownload, Qt::QueuedConnection);
	if(!pJob->Run(Args))
		return false;

	return true;
}

void COnlineUpdater::OnUpdateDownload(int exitCode, const QVariantMap& Params)
{
	bool bManual = m_CheckState == eManual;

	if (exitCode < 0) {
		QMessageBox::critical(theGUI, "TaskExplorer", tr("Failed to download updates from server, error %1").arg(GetUpdErrorStr(exitCode)));
		return; // failed
	}

	QStringList ChangedFiles = Params["files"].toStringList();
	m_UpdatePrepared = true;

	if (bManual)
		ApplyUpdate(ChangedFiles, false);
	else
	{
		QString OnNewRelease = GetOnNewReleaseOption();
		bool bCanRunInstaller = (OnNewRelease == "install");
		ApplyUpdate(ChangedFiles, bCanRunInstaller);
	}
}

QString COnlineUpdater::GetUpdErrorStr(int exitCode)
{
	switch (exitCode)
	{
	case UPD_ERROR_INVALID:		return tr("invalid parameter");
	case UPD_ERROR_GET:			return tr("failed to download updated information");
	case UPD_ERROR_LOAD:		return tr("failed to load updated json file");
	case UPD_ERROR_DOWNLOAD:	return tr("failed to download a particular file");
	case UPD_ERROR_SCAN:		return tr("failed to scan existing installation");
	case UPD_ERROR_SIGN:		return tr("updated signature is invalid !!!");
	case UPD_ERROR_HASH:		return tr("downloaded file is corrupted");
	case UPD_ERROR_INTERNAL:	return tr("internal error");
	default:					return tr("unknown error");
	}
}

bool COnlineUpdater::OnlyMetaDataChanged(const QStringList& Files)
{
#ifdef _DEBUG
	return true;
#endif
	if(Files.isEmpty())
		return false; // empty list means all files

	foreach(const QString& File, Files) {
		if (File.compare("Translations.7z", Qt::CaseInsensitive) != 0 &&
			File.compare("capslist.txt", Qt::CaseInsensitive) != 0 &&
			File.compare("etwguids.txt", Qt::CaseInsensitive) != 0 &&
			File.compare("pooltag.txt", Qt::CaseInsensitive) != 0 &&

			File.compare("AMD64\\ksidyn.bin", Qt::CaseInsensitive) != 0 &&
			File.compare("AMD64\\ksidyn.sig", Qt::CaseInsensitive) != 0 &&
			File.compare("ARM64\\ksidyn.bin", Qt::CaseInsensitive) != 0 &&
			File.compare("ARM64\\ksidyn.sig", Qt::CaseInsensitive) != 0 &&

			File.compare("License.txt", Qt::CaseInsensitive) != 0)
			return false;
	}
	return true;
}

bool COnlineUpdater::ApplyUpdate(bool bSilent)
{
	QVariantMap UpdateData = QJsonDocument::fromJson(ReadFileAsString(GetUpdateDir() + "/" UPDATE_FILE).toUtf8()).toVariant().toMap();
	QStringList ChangedFiles = ScanUpdateFiles(UpdateData);

	return ApplyUpdate(ChangedFiles, bSilent);
}

bool COnlineUpdater::ApplyUpdate(const QStringList& Files, bool bSilent)
{
	bool bIsMetaOnly = OnlyMetaDataChanged(Files);

	if (!bSilent)
	{
		QString Message = tr("<p>Updates for TaskExplorer have been downloaded.</p><p>Do you want to apply these updates? If any programs are running in secure enclaves, they will be terminated.</p>");
		int Ret = QMessageBox("TaskExplorer", Message, QMessageBox::Information, QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape, QMessageBox::Cancel, theGUI).exec();
		if (Ret == QMessageBox::Cancel) {
			ClearUpdate();
			UpdateState();
		}
		if (Ret != QMessageBox::Yes)
			return false;
	}

	//if(bIsMetaOnly)
	//	theAPI->TerminateAll();

	QStringList Params;
	Params.append("update");
	Params.append("/step:apply");
	if (!Files.isEmpty()) 
		Params.append("/files:" + Files.join(";").replace("/", "\\"));
#ifndef _DEBUG
	Params.append("/embedded");
#else
	Params.append("/pause");
#endif
	Params.append("/temp:" + GetUpdateDir().replace("/", "\\"));
#ifdef DUMMY_PATH
	Params.append("/path:" DUMMY_PATH);
#endif
	if (!bIsMetaOnly)
		Params.append("/restart");

	RESULT(int) status = RunUpdater(Params, true, bIsMetaOnly);
	if (!status.IsError()) {
		if (!bIsMetaOnly)
			QApplication::quit();
		return true;
	}
	return false;
}

RESULT(int) COnlineUpdater::RunUpdater(const QStringList& Params, bool bSilent, bool Wait)
{
	//if (bSilent) {
	//	RESULT(int) Result = theCore->RunUpdateUtility(Params, 2, Wait);
	//	if (!Result.IsError())
	//		return Result;
	//	// else fallback to ShellExecuteEx
	//	if (theConf->GetBool("Options/UpdateNoFallback", false))
	//		return Result;
	//}

	std::wstring wFile = QString(QApplication::applicationDirPath() + "/UpdUtil.exe").replace("/", "\\").toStdWString();
	std::wstring wParams;
	foreach(const QString & Param, Params) {
		if (!wParams.empty()) wParams.push_back(L' ');
		wParams += L"\"" + Param.toStdWString() + L"\"";
	}

	int ExitCode = RunElevated(wFile, wParams, Wait ? 15*60*1000 : 0);
	if (ExitCode == STATUS_PENDING && !Wait)
		ExitCode = 0;
	return CResult<int>(0, ExitCode);
}

// Helpers

QString COnlineUpdater::GetUpdateDir(bool bCreate)
{
	QString TempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	if (TempDir.right(1) != "/")
		TempDir += "/";
	TempDir += "TaskExplorer-Updater";
	// Note: must not end with a /
	if(bCreate)
		QDir().mkpath(TempDir);
	return TempDir;
}

QString COnlineUpdater::MakeVersionStr(const QVariantMap& Data)
{
	QString Str = Data["version"].toString();
	int iUpdate = Data["update"].toInt();
	if (iUpdate) Str += QChar('a' + (iUpdate - 1));
	return Str;
}

QString COnlineUpdater::ParseVersionStr(const QString& Str, int* pUpdate)
{
	int pos = Str.indexOf(QRegularExpression("[a-zA-Z]"));
	if (pos == -1)
		return Str;
	QString Ver = Str.left(pos);
	if (pUpdate) {
		QString Tmp = Str.mid(pos);
		*pUpdate = Tmp[0].toLatin1() - 'a' + 1;
	}
	return Ver;
}

QString COnlineUpdater::GetCurrentVersion()
{
	return QString::number(VERSION_MJR) + "." + QString::number(VERSION_MIN) + "." + QString::number(VERSION_REV);
}

int COnlineUpdater::GetCurrentUpdate()
{
#ifdef _DEBUG
	return VERSION_UPD;
#endif
	int iUpdate = 0;
	QString Version = ParseVersionStr(theConf->GetString("Updater/CurrentUpdate", 0), &iUpdate);
	if(Version != GetCurrentVersion() || iUpdate < VERSION_UPD)
		iUpdate = VERSION_UPD;
	return iUpdate;
}

quint32 COnlineUpdater::GetCurrentVersionInt()
{
	//quint8 myVersion[4] = { VERSION_UPD, VERSION_REV, VERSION_MIN, VERSION_MJR }; // ntohl
	quint8 myVersion[4] = { 0, VERSION_REV, VERSION_MIN, VERSION_MJR }; // ntohl
	quint32 MyVersion = *(quint32*)&myVersion;
	return MyVersion;
}

quint32 COnlineUpdater::VersionToInt(const QString& VersionStr)
{
	quint32 Version = 0;
	QStringList Nums = VersionStr.split(".");
	for (int i = 0, Bits = 24; i < Nums.count() && Bits >= 0; i++, Bits -= 8)
		Version |= (Nums[i].toInt() & 0xFF) << Bits;
	return Version;
}

bool COnlineUpdater::IsVersionNewer(const QString& VersionStr)
{
	if (VersionStr.isEmpty())
		return false;
	return VersionToInt(VersionStr) > GetCurrentVersionInt();
}

/////////////////////////////////////////////////////////////////////////////////////////
// Cert Stuff

QString COnlineUpdater::GetOnNewUpdateOption() const
{
	int iCheckUpdates = theConf->GetInt("Options/CheckForUpdates", 2);
	if(iCheckUpdates == 0)
		return "ignore"; // user disabled update checks

	QString OnNewUpdate = theConf->GetString("Options/OnNewUpdate", "ignore");

	return OnNewUpdate;
}

QString COnlineUpdater::GetOnNewReleaseOption() const
{
	int iCheckUpdates = theConf->GetInt("Options/CheckForUpdates", 2);
	if(iCheckUpdates == 0)
		return "ignore"; // user disabled update checks

	QString OnNewRelease = theConf->GetString("Options/OnNewRelease", "download");

	return OnNewRelease;
}