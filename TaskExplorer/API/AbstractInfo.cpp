#include "stdafx.h"
#include "AbstractInfo.h"
#include "../Common/Settings.h"


CAbstractInfoEx::CAbstractInfoEx(QObject *parent)
	: CAbstractInfo(parent)
{
	m_NewlyCreated = true;
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
	if (m_NewlyCreated)
	{
		// Note: GetTime() is very slow, and there is no point to check it over and over agian once we know that this object is old
		quint64 curTime = (qint64)GetTime() * 1000ULL;
		if (!(curTime - m_CreateTimeStamp < theConf->GetUInt64("Options/PersistenceTime", 5000)))
			m_NewlyCreated = false;
	}
	return m_NewlyCreated;
}
