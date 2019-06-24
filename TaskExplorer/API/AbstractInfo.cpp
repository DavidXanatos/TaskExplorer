#include "stdafx.h"
#include "AbstractInfo.h"
#include "../../GUI/TaskExplorer.h"


CAbstractInfoEx::CAbstractInfoEx(QObject *parent)
	: CAbstractInfo(parent)
{
	m_CreateTimeStamp = 0;
	m_RemoveTimeStamp = 0;
}

CAbstractInfoEx::~CAbstractInfoEx()
{
}

bool CAbstractInfoEx::CanBeRemoved() const
{ 
	QReadLocker Locker(&m_Mutex); 
	if (m_RemoveTimeStamp == 0)
		return false;
	return GetCurTick() - m_RemoveTimeStamp > theConf->GetUInt64("Options/PersistenceTime", 5000);
}

bool CAbstractInfoEx::IsNewlyCreated() const
{
	QReadLocker Locker(&m_Mutex);
	quint64 curTime = (qint64)GetTime() * 1000ULL;
	return curTime - m_CreateTimeStamp < theConf->GetUInt64("Options/PersistenceTime", 5000);
}