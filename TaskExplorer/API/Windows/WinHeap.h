#pragma once
#include <qobject.h>
#include "../HeapInfo.h"

class CWinHeap: public CHeapInfo
{
	Q_OBJECT

public:
	CWinHeap(QObject *parent = nullptr);
	virtual ~CWinHeap();

	virtual quint32 GetFlags() const;
	virtual QString GetFlagsString() const;
	virtual quint32 GetClass() const;
	virtual QString GetClassString() const;
	virtual quint32 GetType() const;
	virtual QString GetTypeString() const;
	
protected:
	friend class CWinProcess;

	quint32 m_Signature;
	quint8 m_HeapFrontEndType;
};

typedef QSharedPointer<CWinHeap> CWinHeapPtr;
typedef QWeakPointer<CWinHeap> CWinHeapRef;