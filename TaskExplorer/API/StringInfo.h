#pragma once
#include <qobject.h>
#include "AbstractInfo.h"
#include "../Common/FlexError.h"
#include "ProcessInfo.h"

class CStringInfo : public QObject
{
	Q_OBJECT
public:
	CStringInfo(quint64 Address, quint64 Size, quint64 BaseAddress, quint64 RegionSize, const QString& String, const CProcessPtr& pProcess)
	{
		m_Address = Address;
		m_Size = Size;
		m_BaseAddress = BaseAddress;
		m_RegionSize = RegionSize;
		m_String = String;

		m_pProcess = pProcess;
	}

	quint64 GetAddress() const				{ return m_Address; }
	quint64 GetSize() const					{ return m_Size; }
	quint64 GetBaseAddress() const			{ return m_BaseAddress; }
	quint64 GetRegionSize() const			{ return m_RegionSize; }
	QString GetString() const				{ return m_String; }
	
	CProcessPtr	GetProcess() const			{ return m_pProcess; }

protected:
	quint64 m_Address;
	quint64 m_Size;
	quint64 m_BaseAddress;
	quint64 m_RegionSize;
	QString m_String;
	
	CProcessPtr	m_pProcess;
};

typedef QSharedPointer<CStringInfo> CStringInfoPtr;
typedef QWeakPointer<CStringInfo> CStringInfoRef;