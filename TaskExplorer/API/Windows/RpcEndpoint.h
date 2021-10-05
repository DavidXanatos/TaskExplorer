#pragma once
#include <qobject.h>
#include "..\AbstractInfo.h"

class CRpcEndpoint: public CAbstractInfoEx
{
	Q_OBJECT

public:
	CRpcEndpoint(QObject *parent = nullptr);
	virtual ~CRpcEndpoint();

	virtual QString GetIfId() const				{ QReadLocker Locker(&m_Mutex); return m_IfId; }
	virtual QString GetDescription() const		{ QReadLocker Locker(&m_Mutex); return m_Description; }
	virtual QString GetBinding() const			{ QReadLocker Locker(&m_Mutex); return m_Binding; }

protected:
	friend class CWindowsAPI;

	bool							UpdateDynamicData(void* hEnumBind);

	QString							m_IfId;
	QString							m_Description;
	QString							m_Binding;
};

typedef QSharedPointer<CRpcEndpoint> CRpcEndpointPtr;
typedef QWeakPointer<CRpcEndpoint> CRpcEndpointRef;