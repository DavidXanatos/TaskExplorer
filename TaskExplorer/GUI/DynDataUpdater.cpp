/*
 * Task Explorer -
 *   fetching the kernel-version-specific offsets the driver needs.
 *
 * This drives a progress dialog and a download, so it belongs with the GUI; it
 * used to sit in API/Windows/ProcessHacker.cpp and reach back up into
 * CTaskExplorer, which was one of the two edges keeping the API layer from
 * being a library of its own.
 */

#include "stdafx.h"
#include "TaskExplorer.h"
#include "../API/SystemAPI.h"
#ifdef WIN32
#include "../API/Windows/ProcessHacker.h" // IsOnARM64
#endif
#include "../../MiscHelpers/Common/Settings.h"
#include "../../MiscHelpers/Archive/Archive.h"
#include "../../MiscHelpers/Common/ProgressDialog.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

STATUS CTaskExplorer::UpdateDynData(const QString& AppDir)
{
	STATUS Status;

	CProgressDialog Progress(tr("Updating DynData"));
	QNetworkAccessManager Manager;

	QString Folder = theConf->GetConfigDir() + "\\Temp";
	QDir().mkpath(Folder);

	auto FailWithMessage = [&](const QString& Message) {
		Status = ERR(TE_Message, QVariantList() << Message);
		Progress.ShowProgress(Message);
		QTimer::singleShot(3000, [&] {Progress.close();});
	};

	auto ApplyUpdate = [&](const QString& FileName){

		CArchive Archive(FileName);

		if (Archive.Open() != ERR_7Z_OK) {
			FailWithMessage(tr("Failed to open archive."));
			return;
		}

		bool IsBoxArchive = false;

		QMap<int, QIODevice*> Files;

		// theSystem may not exist yet: at startup UpdateDynData runs from the
		// driver-load loop in main(), before CTaskExplorer's constructor calls
		// InitLocalSystem() to create theSystem. Reaching through it here caused a
		// null-pointer crash. The architecture is a static property of the machine,
		// so read it directly rather than via theSystem->GetArchitecture().
#ifdef WIN32
		const bool bArm64 = IsOnARM64();
#else
		const bool bArm64 = theSystem && theSystem->GetArchitecture() == CSystemAPI::eArchArm64;
#endif

		int bin = Archive.FindByPath(bArm64 ? "arm64/ksidyn.bin" : "amd64/ksidyn.bin");
		int sig = Archive.FindByPath(bArm64 ? "arm64/ksidyn.sig" : "amd64/ksidyn.sig");

		if (bin == -1 || sig == -1) {
			FailWithMessage(tr("DynData not found in archive."));
//#ifndef _DEBUG
			QFile::remove(FileName);
//#endif
			return;
		}

		QString DrvPath;
		QString DrvFileName = theConf->GetString("OptionsKSI/FileName", "KTaskExplorer.sys");
		if (DrvFileName.contains("\\")) 
			DrvPath = Split2(DrvFileName, "\\", true).first;
		else if (bArm64)
			DrvPath = AppDir + "\\ARM64";
		else
			DrvPath = AppDir + "\\AMD64";

		Files.insert(bin, new QFile(DrvPath + "\\ksidyn.bin.tmp"));
		Files.insert(sig, new QFile(DrvPath + "\\ksidyn.sig.tmp"));

		if (!Archive.Extract(&Files)) {
			FailWithMessage(tr("Failed to extreact files."));
//#ifndef _DEBUG
			QFile::remove(FileName);
//#endif
			return;
		}

//#ifndef _DEBUG
		QFile::remove(FileName);
//#endif


		QFile::remove(DrvPath + "\\ksidyn.bin.bak");
		QFile::remove(DrvPath + "\\ksidyn.sig.bak");

		QFile::rename(DrvPath + "\\ksidyn.bin", DrvPath + "\\ksidyn.bin.bak");
		QFile::rename(DrvPath + "\\ksidyn.sig", DrvPath + "\\ksidyn.sig.bak");

		QFile::rename(DrvPath + "\\ksidyn.bin.tmp", DrvPath + "\\ksidyn.bin");
		QFile::rename(DrvPath + "\\ksidyn.sig.tmp", DrvPath + "\\ksidyn.sig");

		Archive.Close(); Progress.ShowProgress(tr("Updated DynData successfully"));
		QTimer::singleShot(3000, [&] {Progress.close(); });
	};

	QScopedPointer<QNetworkReply> DlReply;
	auto DownloadSI = [&](const QString& sUrl) {

		QUrl DlUrl(sUrl);

		QString FileName = Folder + "\\" + DlUrl.fileName();

		if (QFile::exists(FileName)) {
//#ifdef _DEBUG
//			Progress.OnProgressMessage(tr("Latest SI build already downloaded"));
//			ApplyUpdate(FileName);
//			return;
//#else
			QFile::remove(FileName);
//#endif
		}

		QNetworkRequest DlRequest(DlUrl);
		DlRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

		DlReply.reset(Manager.get(DlRequest));

		QObject::connect(DlReply.data(), &QNetworkReply::downloadProgress, &Manager, [&](qint64 bytes, qint64 bytesTotal) {
			if (bytesTotal != 0)
				Progress.ShowProgress(tr("Downloading latest SI build"), 100 * bytes / bytesTotal);
		});

		QObject::connect(DlReply.data(), &QNetworkReply::finished, &Manager, [&, FileName]() {

			if (DlReply->error() != QNetworkReply::NoError) {
				QString Error = DlReply->errorString();
				FailWithMessage(tr("Download Failed, Error: %1").arg(Error));
				return;
			}

			QFile File(FileName);
			if (!File.open(QIODevice::WriteOnly)) {
				FailWithMessage(tr("Failed to open file for writing."));
				return;
			}
			File.write(DlReply->readAll());
			File.close();

			Progress.ShowProgress(tr("Successfully Downloaded latest SI build"));
			ApplyUpdate(FileName);
		});
	};

	QScopedPointer<QNetworkReply> Reply;
	auto GetUpdate = [&](){

		QString sUrl = theConf->GetString("OptionsKSI/SIUpdateUrl", "https://systeminformer.dev/update?channel=canary");
		
		QUrl Url(sUrl);
		
		QNetworkRequest Request(Url);
		Request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

		Reply.reset(Manager.get(Request));

		QObject::connect(Reply.data(), &QNetworkReply::finished, &Manager, [&]() {

			if (Reply->error() != QNetworkReply::NoError) {
				QString Error = Reply->errorString();
				FailWithMessage(tr("Update Check Failed, Error: %1").arg(Error));
				return;
			}
			//QString Location = Reply->header(QNetworkRequest::LocationHeader).toString();

			QByteArray Json = Reply->readAll();
			QVariantMap Data = QJsonDocument::fromJson(Json).toVariant().toMap();
			QString BinUrl = Data["bin_url"].toString();
			if (BinUrl.isEmpty()) {
				FailWithMessage(tr("Update Check Failed, Error: Unrecognized Reply"));
				return;
			}

			DownloadSI(BinUrl);
		});
	};

	GetUpdate();

	Progress.exec();

	return Status;
}
