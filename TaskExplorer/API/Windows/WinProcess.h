#pragma once
#include "../ProcessInfo.h"


class CWinProcessPrivate;

class CWinProcess : public CProcessInfo
{
	Q_OBJECT

public:
	CWinProcess(QObject *parent = nullptr);
	virtual ~CWinProcess();

	virtual bool ValidateParent(const CProcessPtr& pParent) const;

	// Basic
	virtual quint64 GetSessionID() const;
	virtual quint64 GetRawCreateTime() const;

	// Flags
	virtual bool IsSubsystemProcess() const;

	// Security
	virtual int IntegrityLevel() const;
	virtual QString GetIntegrityString() const;

	// Dynamic
	virtual int GetBasePriority() const				{ QReadLocker Locker(&m_Mutex); return m_BasePriority; }

	// GDI, USER handles
	virtual ulong GetGdiHandles() const				{ QReadLocker Locker(&m_Mutex); return m_GdiHandles; }
	virtual ulong GetUserHandles() const			{ QReadLocker Locker(&m_Mutex); return m_UserHandles; }

	// OS context
	virtual ulong GetOsContextVersion() const;
	virtual QString GetOsContextString() const;

	// Other Fields
	virtual quint64 GetProcessSequenceNumber() const;

public slots:
	virtual bool	UpdateThreads();
	virtual bool	UpdateHandles();
	virtual bool	UpdateModules();

	void			OnAsyncDataDone(bool IsPacked, ulong ImportFunctions, ulong ImportModules);

protected:
	friend class CWindowsAPI;

	bool InitStaticData(struct _SYSTEM_PROCESS_INFORMATION* process);
	bool UpdateDynamicData(struct _SYSTEM_PROCESS_INFORMATION* process, quint64 sysTotalTime, quint64 sysTotalCycleTime);
	bool UpdateThreadData(struct _SYSTEM_PROCESS_INFORMATION* process, quint64 sysTotalTime, quint64 sysTotalCycleTime);
	void UnInit();

	void AddNetworkIO(int Type, ulong TransferSize);
	void AddDiskIO(int Type, ulong TransferSize);

	// Dynamic
	int m_BasePriority;

	// GDI, USER handles
    ulong m_GdiHandles;
    ulong m_UserHandles;

	// Threads
	//QMap<quint64, >

private:

	struct SWinProcess* m;
};

// CWinProcess Helper Functions

QString GetSidFullNameCached(const vector<char>& Sid);