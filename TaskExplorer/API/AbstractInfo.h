#pragma once
#include <qobject.h>
#include "../Common/Common.h"

class CAbstractInfo : public QObject
{
	Q_OBJECT

public:
	CAbstractInfo(QObject *parent = nullptr) : QObject(parent) {}
	virtual ~CAbstractInfo() {}

	mutable QReadWriteLock		m_Mutex;
};


class CAbstractInfoEx : public CAbstractInfo
{
	Q_OBJECT

public:
	CAbstractInfoEx(QObject *parent = nullptr);
	virtual ~CAbstractInfoEx();

	static void					SetHighlightTime(quint64 time)		{ m_HighlightTime = time; }
	static quint64				GetHighlightTime()					{ return m_HighlightTime; }
	virtual quint64				GetCreateTimeStamp() const			{ QReadLocker Locker(&m_Mutex); return m_CreateTimeStamp; }
	virtual bool				IsNewlyCreated() const;
	
	static void					SetPersistenceTime(quint64 time)	{ m_PersistenceTime = time; }
	static quint64				GetPersistenceTime()				{ return m_PersistenceTime; }
	virtual void				InitTimeStamp()						{ QWriteLocker Locker(&m_Mutex); m_CreateTimeStamp = GetTime() * 1000; }
	virtual void				MarkForRemoval()					{ QWriteLocker Locker(&m_Mutex); m_RemoveTimeStamp = GetCurTick(); }
	virtual bool				IsMarkedForRemoval() const			{ QReadLocker Locker(&m_Mutex); return m_RemoveTimeStamp != 0; }
	virtual bool				CanBeRemoved() const;
	virtual void				ClearPersistence();

protected:
	volatile mutable bool		m_NewlyCreated;
	quint64						m_CreateTimeStamp;

	quint64						m_RemoveTimeStamp;

	static volatile quint64		m_PersistenceTime;
	static volatile quint64		m_HighlightTime;
};