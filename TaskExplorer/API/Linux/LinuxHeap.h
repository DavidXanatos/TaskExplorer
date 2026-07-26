#pragma once
#include <qobject.h>
#include "../HeapInfo.h"

//
// Placeholder for the heap view.
//
// Unlike the Windows heap manager, glibc's malloc publishes no enumerable list
// of heaps or per-heap accounting to an external observer. The closest
// available data is the arena summary from malloc_info(), which only the owning
// process can produce. This class therefore exists to satisfy the shared
// interface; the Linux heap list is expected to stay empty.
//
class CLinuxHeap : public CHeapInfo
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxHeap)
public:
	CLinuxHeap(QObject *parent = nullptr);
	virtual ~CLinuxHeap();

	virtual QString			GetFlagsString() const;
	virtual quint32			GetClass() const;
	virtual QString			GetClassString() const;
	virtual quint32			GetType() const;
	virtual QString			GetTypeString() const;
};

typedef QSharedPointer<CLinuxHeap> CLinuxHeapPtr;
typedef QWeakPointer<CLinuxHeap> CLinuxHeapRef;
