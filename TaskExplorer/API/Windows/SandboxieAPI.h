#pragma once

#include <QObject>

class CSandboxieAPI : public QObject
{
	Q_OBJECT
public:
	static QString FindSbieDll();

	CSandboxieAPI(const QString& dllPath, QObject* parent = NULL);
	virtual ~CSandboxieAPI();

	//bool UpdateSandboxes();

	QString GetSandBoxName(quint64 ProcessId);

	bool IsSandBoxed(quint64 ProcessId);

protected:
	/*struct SBoxedProcess
	{
		quint64		ProcessId;

		QString		BoxName;
	};
	QMap<quint64, SBoxedProcess*> m_BoxedProcesses;

	struct SBoxInfo
	{
		QString		BoxName;

		QString		FileRoot;
		QString		KeyRoot;
		QString		IpcRoot;
	};
	QMap<QString, SBoxInfo*> m_SandBoxes;

	mutable QReadWriteLock m_Mutex;*/

private:
	struct SSandboxieAPI* m;
};

