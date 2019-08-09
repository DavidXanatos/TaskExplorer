#pragma once
#include <qobject.h>
#include "../AbstractInfo.h"

class CWinGDI: public CAbstractInfoEx
{
	Q_OBJECT

public:
	CWinGDI(QObject *parent = nullptr);
	virtual ~CWinGDI();

	virtual ulong GetHandleId() const				{ QReadLocker Locker(&m_Mutex); return m_HandleId; }
	virtual QString GetTypeString() const;
	virtual quint64 GetObject() const				{ QReadLocker Locker(&m_Mutex); return m_Object; }
	virtual QString GetInformations() const			{ QReadLocker Locker(&m_Mutex); return m_Informations; }

protected:
	friend class CWinProcess;

	bool InitData(ulong index, struct _GDI_HANDLE_ENTRY* handle);

	ulong			m_HandleId;
	quint64			m_Object;
	QString			m_Informations;
};

typedef QSharedPointer<CWinGDI> CWinGDIPtr;
typedef QWeakPointer<CWinGDI> CWinGDIRef;