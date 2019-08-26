#pragma once
#include <qobject.h>
#include "../AbstractInfo.h"

class CWinGDI: public CAbstractInfoEx
{
	Q_OBJECT

public:
	CWinGDI(QObject *parent = nullptr);
	virtual ~CWinGDI();

	bool InitData(ulong index, struct _GDI_HANDLE_ENTRY* handle, const QString& ProcessName);

	virtual QString GetProcessName() const			{ QReadLocker Locker(&m_Mutex); return m_ProcessName; }
	virtual quint64 GetProcessId() const			{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }
	virtual ulong GetHandleId() const				{ QReadLocker Locker(&m_Mutex); return m_HandleId; }
	virtual QString GetTypeString() const;
	virtual quint64 GetObject() const				{ QReadLocker Locker(&m_Mutex); return m_Object; }
	virtual QString GetInformations() const			{ QReadLocker Locker(&m_Mutex); return m_Informations; }

protected:
	QString			m_ProcessName;
	quint64			m_ProcessId;
	ulong			m_HandleId;
	quint64			m_Object;
	QString			m_Informations;
};

typedef QSharedPointer<CWinGDI> CWinGDIPtr;
typedef QWeakPointer<CWinGDI> CWinGDIRef;