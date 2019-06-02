#pragma once
#include "../ProcessInfo.h"


class CWinProcessPrivate;

class CWinProcess : public CProcessInfo
{
	Q_OBJECT

public:
	CWinProcess(QObject *parent = nullptr);
	virtual ~CWinProcess();

	// Basic
	virtual quint64 GetSessionID() const			{ QReadLocker Locker(&m_Mutex); return m_SessionId; }

	// Flags
	virtual bool IsSubsystemProcess() const;

	// Security
	virtual int IntegrityLevel() const;
	virtual QString GetIntegrityString() const;

	// Dynamic
	virtual ulong GetNumberOfHandles() const		{ QReadLocker Locker(&m_Mutex); return m_NumberOfHandles; }
	virtual int GetBasePriority() const				{ QReadLocker Locker(&m_Mutex); return m_BasePriority; }

	// File
	virtual QString GetDescription() const;

	// GDI, USER handles
	virtual ulong GetGdiHandles() const				{ QReadLocker Locker(&m_Mutex); return m_GdiHandles; }
	virtual ulong GetUserHandles() const			{ QReadLocker Locker(&m_Mutex); return m_UserHandles; }

	// OS context
	virtual ulong GetOsContextVersion() const;
	virtual QString GetOsContextString() const;

public slots:
	virtual bool	UpdateThreads();
	virtual bool	UpdateHandles();

protected:
	friend class CWindowsAPI;

	bool InitStaticData(struct _SYSTEM_PROCESS_INFORMATION* process);
	bool UpdateDynamicData(struct _SYSTEM_PROCESS_INFORMATION* process);
	bool UpdateThreadData(struct _SYSTEM_PROCESS_INFORMATION* process);
	void UnInit();
	void InitAsyncData();

	// Basic
	quint64 m_SessionId;

	// Dynamic
	int m_BasePriority;
	ulong m_NumberOfHandles;

	// GDI, USER handles
    ulong m_GdiHandles;
    ulong m_UserHandles;

	// Threads
	//QMap<quint64, >

protected slots:
	void OnInitAsyncData(int Index);

private:
	static QVariantMap InitAsyncData(QVariantMap Params);

	struct SWinProcess* m;
};

// CWinProcess Helper Functions

QString GetSidFullNameCached(const vector<char>& Sid);