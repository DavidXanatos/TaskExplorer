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

	static QString RunService();
#ifdef WIN64
	static QString RunService32();
#endif
	static bool RunService(const QString& ServiceName, QString BinaryPath = "");

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

	void timerEvent(QTimerEvent *e);

	void start();

	void stop();

    //void pause() {}
    //void resume() {}

	int					m_TimerId;
	quint64				m_TimeOut;
	quint64				m_LastActivity;
	QLocalServer*		m_pServer;


private:
	static QString GetTempName() { QMutexLocker Locker(&m_TempMutex); return m_TempName; }

	static QMutex m_TempMutex;
	static QString m_TempName;
};
