#include "stdafx.h"
#include "LinuxHeap.h"

CLinuxHeap::CLinuxHeap(QObject *parent)
	: CHeapInfo(parent)
{
}

CLinuxHeap::~CLinuxHeap()
{
}

QString CLinuxHeap::GetFlagsString() const
{
	return QString();
}

quint32 CLinuxHeap::GetClass() const
{
	return 0;
}

QString CLinuxHeap::GetClassString() const
{
	return QString();
}

quint32 CLinuxHeap::GetType() const
{
	return 0;
}

QString CLinuxHeap::GetTypeString() const
{
	return QString();
}
