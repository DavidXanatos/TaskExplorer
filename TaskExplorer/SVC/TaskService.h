#pragma once
#include "../qtservice/src/qtservice.h"

#define TASK_SERVICE_NAME "TaskExplorerSvc"

class CTaskService : public QObject, public QtServiceBase
{
	Q_OBJECT

public:
	CTaskService(int argc, char **argv, const QString& svcName, int timeout = 0);
	virtual ~CTaskService();

#ifdef _DEBUG
	static QVariant SendCommand(const QString& socketName, const QVariant &Command, int timeout = 30000);
#else
	static QVariant SendCommand(const QString& socketName, const QVariant &Command, int timeout = 5000);
#endif
	static void Terminate(const QString& socketName) { CTaskService::SendCommand(socketName, "Quit", 500); }

#ifdef WIN64
	static QString RunWorker(bool bElevanted = true, bool b32Bit = false);
#else
	static QString RunWorker(bool bElevanted = true);
#endif

	static bool CheckStatus(long Status);

	static bool TaskAction(quint64 ProcessId, const QString& Action, const QVariant& Data = QVariant()) { return TaskAction(ProcessId, 0, Action, Data); }
	static bool TaskAction(quint64 ProcessId, quint64 ThreadId, const QString& Action, const QVariant& Data = QVariant());
	static bool ServiceAction(const QString& Name, const QString& Action, const QVariant& Data = QVariant());

	static QString RunService();
	static bool RunService(const QString& ServiceName, QString BinaryPath = "");

	static void TerminateWorkers();

	void start();

	void stop();

public slots:
	void		receiveConnection();

protected:
	static bool	SendVariant(QLocalSocket* pSocket, const QVariant& Data, int timeout);
	static QVariant	RecvVariant(QLocalSocket* pSocket, int timeout);

    virtual void createApplication(int &argc, char **argv) {
		// we already created one
	}

    virtual int executeApplication()
    { 
		return QCoreApplication::exec(); 
	}

	qint32 ExecTaskAction(quint64 ProcessId, const QString& Action, const QVariant& Data);
	qint32 ExecTaskAction(quint64 ProcessId, quint64 ThreadId, const QString& Action, const QVariant& Data);
	qint32 ExecServiceAction(const QString& Name, const QString& Action, const QVariant& Data);

	void timerEvent(QTimerEvent *e);

    //void pause() {}
    //void resume() {}

	int					m_TimerId;
	quint64				m_TimeOut;
	quint64				m_LastActivity;
	QLocalServer*		m_pServer;


private:
	static QMutex m_Mutex;
	static QString m_TempName;
	static QString m_TempSocket;
#ifdef WIN64
	static QString m_TempSocket32;
#endif
};
