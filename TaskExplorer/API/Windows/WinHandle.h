#pragma once
#include "../HandleInfo.h"

// begin_phapppub
#define PH_HANDLE_FILE_SHARED_READ 0x1
#define PH_HANDLE_FILE_SHARED_WRITE 0x2
#define PH_HANDLE_FILE_SHARED_DELETE 0x4
#define PH_HANDLE_FILE_SHARED_MASK 0x7

class CWinHandle : public CHandleInfo
{
	Q_OBJECT
public:
	CWinHandle(QObject *parent = nullptr);
	virtual ~CWinHandle();

	static quint64 MakeID(quint64 HandleValue, quint64 UniqueProcessId);

	virtual quint64 GetObjectAddress()	const		{ QReadLocker Locker(&m_Mutex); return m_Object; }
	
	virtual quint32 GetAttributes() const			{ QReadLocker Locker(&m_Mutex); return m_Attributes; }
	virtual QString GetAttributesString() const;
	virtual STATUS SetAttribute(quint32 Attribute, bool bSet = true);
	virtual bool IsProtected() const;
	virtual STATUS SetProtected(bool bSet);
	virtual bool IsInherited() const;
	virtual STATUS SetInherited(bool bSet);

	virtual quint32 GetGrantedAccess() const		{ QReadLocker Locker(&m_Mutex); return m_GrantedAccess; }
	virtual quint32 GetTypeIndex() const			{ QReadLocker Locker(&m_Mutex); return m_TypeIndex; }
	virtual quint32 GetFileFlags() const			{ QReadLocker Locker(&m_Mutex); return m_FileFlags; }

	virtual QString GetTypeName() const				{ QReadLocker Locker(&m_Mutex); return m_TypeName; }
	virtual QString GetOriginalName() const			{ QReadLocker Locker(&m_Mutex); return m_OriginalName; }

	virtual QString GetFileShareAccessString() const;
	virtual QString GetTypeString() const;
	virtual QString GetGrantedAccessString() const;

	static QString GetFileAccessMode(quint32 Mode);
	static QString GetSectionType(quint32 Attribs);
	virtual QVariantMap GetHandleInfo() const;

	virtual STATUS				Close(bool bForce = false);

	enum EHandleAction
	{
		eSemaphoreAcquire,
		eSemaphoreRelease,

		eEventSet,
		eEventReset,
		eEventPulse,

		eSetLow,
		eSetHigh,

		eCancelTimer
	};
	virtual STATUS				DoHandleAction(EHandleAction Action);

	
	virtual void OpenPermissions();

protected:
	friend class CWindowsAPI;
	friend class CWinProcess;
	friend class CWinHandleFinder;

	bool InitStaticData(struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* handle, quint64 TimeStamp);

	bool InitExtData(struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* handle, quint64 ProcessHandle) { return InitExtData(handle, ProcessHandle, true); }
	QFutureWatcher<bool>* InitExtDataAsync(struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* handle, quint64 ProcessHandle);

	bool UpdateDynamicData(struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* handle, quint64 ProcessHandle);

	quint64				m_Object;
	quint32				m_Attributes;
	quint32				m_GrantedAccess;
	quint32				m_TypeIndex;
	quint32				m_FileFlags;

	QString				m_TypeName;
    QString				m_OriginalName;

	QString				m_SubTypeName;

private:
	bool InitExtData(struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* handle, quint64 ProcessHandle, bool bFull);
	static bool InitExtDataAsync(CWinHandle* This, struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX* handle, quint64 ProcessHandle);

};

class CWinHandleEx : public CWinHandle
{
public:
	CWinHandleEx(QObject *parent = nullptr) : CWinHandle(parent) {}

	virtual QString GetProcessName() const { return m_ProcessName; }

protected:
	friend class CWindowsAPI;

	QString			m_ProcessName;
};