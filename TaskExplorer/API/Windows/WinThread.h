#pragma once
#include "../ThreadInfo.h"

class CWinThread : public CThreadInfo
{
	Q_OBJECT
public:
	CWinThread(QObject *parent = nullptr);
	virtual ~CWinThread();

	virtual quint64 GetStartAddress() const				{ QReadLocker Locker(&m_Mutex); return m_StartAddress; }
	virtual QString GetStartAddressString() const;

	virtual quint64 GetBasePriorityIncrement() const	{ QReadLocker Locker(&m_Mutex); return m_BasePriorityIncrement; }

	virtual QString GetStateString() const;
	virtual QString GetPriorityString() const;

	virtual void TraceStack();

private slots:
	void	OnSymbolFromAddress(quint64 ProcessId, quint64 Address, int ResolveLevel, const QString& StartAddressString/*, const QString& FileName, const QString& SymbolName*/);

protected:
	friend class CWinProcess;

	bool InitStaticData(struct _SYSTEM_THREAD_INFORMATION* thread);
	bool UpdateDynamicData(struct _SYSTEM_THREAD_INFORMATION* thread);
	bool UpdateExtendedData();
	void UnInit();

	quint64		m_StartAddress;
	QString		m_StartAddressString;

	long		m_BasePriorityIncrement;

	struct SWinThread*	m;
};