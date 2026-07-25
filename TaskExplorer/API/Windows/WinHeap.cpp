#include "stdafx.h"
#include "WinHeap.h"

#include "ProcessHacker.h"

CWinHeap::CWinHeap(QObject *parent) : CHeapInfo(parent)
{
	m_Signature = 0;
	m_HeapFrontEndType = 0;
}

CWinHeap::~CWinHeap()
{
}

quint32 CWinHeap::GetFlags() const 
{ 
	QReadLocker Locker(&m_Mutex); 
	return m_Flags & ~HEAP_CLASS_MASK; 
}

QString CWinHeap::GetFlagsString() const
{
	QStringList Info;

	quint32 Flags = GetFlags();

    if (Flags & HEAP_NO_SERIALIZE)
        Info.append(tr("No serialize"));
    if (Flags & HEAP_GROWABLE)
        Info.append(tr("Growable"));
    if (Flags & HEAP_GENERATE_EXCEPTIONS)
        Info.append(tr("Generate exceptions"));
    if (Flags & HEAP_ZERO_MEMORY)
        Info.append(tr("Zero memory"));
    if (Flags & HEAP_REALLOC_IN_PLACE_ONLY)
        Info.append(tr("Realloc in-place"));
    if (Flags & HEAP_TAIL_CHECKING_ENABLED)
        Info.append(tr("Tail checking"));
    if (Flags & HEAP_FREE_CHECKING_ENABLED)
        Info.append(tr("Free checking"));
    if (Flags & HEAP_DISABLE_COALESCE_ON_FREE)
        Info.append(tr("Coalesce on free"));
    if (Flags & HEAP_CREATE_ALIGN_16)
        Info.append(tr("Align 16"));
    if (Flags & HEAP_CREATE_ENABLE_TRACING)
        Info.append(tr("Traceable"));
    if (Flags & HEAP_CREATE_ENABLE_EXECUTE)
        Info.append(tr("Executable"));
    if (Flags & HEAP_CREATE_SEGMENT_HEAP)
        Info.append(tr("Segment heap"));
    if (Flags & HEAP_CREATE_HARDENED)
        Info.append(tr("Segment hardened"));

	return Info.join(", ");
}

quint32 CWinHeap::GetClass() const
{ 
	QReadLocker Locker(&m_Mutex); 
	return m_Flags & HEAP_CLASS_MASK; 
}

QString CWinHeap::GetClassString() const
{
    switch (GetClass())
    {
    case HEAP_CLASS_0:
        return tr("Process Heap");
    case HEAP_CLASS_1:
        return tr("Private Heap");
    case HEAP_CLASS_2:
        return tr("Kernel Heap");
    case HEAP_CLASS_3:
        return tr("GDI Heap");
    case HEAP_CLASS_4:
        return tr("User Heap");
    case HEAP_CLASS_5:
        return tr("Console Heap");
    case HEAP_CLASS_6:
        return tr("Desktop Heap");
    case HEAP_CLASS_7:
        return tr("CSRSS Shared Heap");
    case HEAP_CLASS_8:
        return tr("CSRSS Port Heap");
    }

    return tr("Unknown Heap");
}

quint32 CWinHeap::GetType() const
{
	QReadLocker Locker(&m_Mutex);
	return m_HeapFrontEndType;
}

QString CWinHeap::GetTypeString() const
{
    QReadLocker Locker(&m_Mutex);

    switch (m_Signature)
    {
    case RTL_HEAP_SIGNATURE:
    {
        switch (m_HeapFrontEndType)
        {
        case 1:
            return tr("NT Heap (Lookaside)");
        case 2:
            return tr("NT Heap (LFH)");
        default:
            return tr("NT Heap");
        }
    }
    break;
    case RTL_HEAP_SEGMENT_SIGNATURE:
    {
        switch (m_HeapFrontEndType)
        {
        case 1:
            return tr("Segment Heap (Lookaside)");
        case 2:
            return tr("Segment Heap (LFH)");
        default:
            return tr("Segment Heap");
        }
    }
    break;
    }

	return tr("Unknown Heap");
}