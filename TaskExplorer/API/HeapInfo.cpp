#include "stdafx.h"
#include "HeapInfo.h"


CHeapInfo::CHeapInfo(QObject *parent) : CAbstractInfoEx(parent)
{
    m_Flags = 0;
    m_NumberOfEntries = 0;
    m_BaseAddress = 0;
    m_BytesAllocated = 0;
    m_BytesCommitted = 0;
}

CHeapInfo::~CHeapInfo()
{
}
