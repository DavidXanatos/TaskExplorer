#pragma once

#include "../ThreadInfo.h"

class CWinThread;
class CAbstractSymbolProviderJob;

class CSymbolProvider : public QThread
{
    Q_OBJECT

public:
	CSymbolProvider(QObject *parent = nullptr);
    virtual ~CSymbolProvider();

	bool		Init();

	bool		IsRunning() { return m_bRunning; }

	void		GetSymbolFromAddress(quint64 ProcessId, quint64 Address, QObject *receiver, const char *member);

	quint64		GetStackTrace(quint64 ProcessId, quint64 ThreadId, QObject *receiver, const char *member);

	void		CancelJob(quint64 JobID);

signals:
	void		StatusMessage(const QString& Message);

protected:
	void		UnInit();

	virtual void run();

	bool		m_bRunning;

	mutable QMutex				m_JobMutex;
	QQueue<CAbstractSymbolProviderJob*>	m_JobQueue;

private:
	QMap<quint64, struct SSymbolProvider*> mm;

	struct _PH_CALLBACK_REGISTRATION* m_SymbolProviderEventRegistration;
};

typedef QSharedPointer<CSymbolProvider> CSymbolProviderPtr;


class CAbstractSymbolProviderJob : public QObject
{
	Q_OBJECT

protected:
	friend class CSymbolProvider;

	CAbstractSymbolProviderJob(quint64 ProcessId, QObject *parent = nullptr) : QObject(parent) { m_ProcessId = ProcessId;  m = NULL; }
	virtual ~CAbstractSymbolProviderJob() {}

	virtual void Run(struct SSymbolProvider* m) = 0;

	quint64			m_ProcessId;

	struct SSymbolProvider* m;
};


class CSymbolProviderJob : public CAbstractSymbolProviderJob
{
	Q_OBJECT

protected:
	friend class CSymbolProvider;

	CSymbolProviderJob(quint64 ProcessId, quint64 Address, QObject *parent = nullptr) : CAbstractSymbolProviderJob(ProcessId, parent) { m_Address = Address; }
	virtual ~CSymbolProviderJob() {}

	virtual void Run(struct SSymbolProvider* m);

	quint64			m_Address;

signals:
	void		SymbolFromAddress(quint64 ProcessId, quint64 Address, int ResolveLevel, const QString& StartAddressString, const QString& FileName, const QString& SymbolName);
};

class CStackProviderJob : public CAbstractSymbolProviderJob
{
	Q_OBJECT

protected:
	friend class CSymbolProvider;

	CStackProviderJob(quint64 ProcessId, quint64 ThreadId, QObject *parent = nullptr) : CAbstractSymbolProviderJob(ProcessId, parent) { m_ThreadId = ThreadId; }
	virtual ~CStackProviderJob() {}

	virtual void Run(struct SSymbolProvider* m);

	quint64			m_ThreadId;

signals:
	void		StackTraced(const CStackTracePtr& StackTrace);

private:
	CStackTracePtr  m_StackTrace;

public:
	void		OnCallBack(struct _PH_THREAD_STACK_FRAME* StackFrame);
};

QVariant SvcApiPredictAddressesFromClrData(const QVariantMap& Parameters);
QVariant SvcApiGetRuntimeNameByAddressClrProcess(const QVariantMap& Parameters);
