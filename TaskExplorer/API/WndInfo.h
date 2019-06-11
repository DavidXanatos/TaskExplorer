#pragma once
#include <qobject.h>

class CWndInfo: public QObject
{
	Q_OBJECT

public:
	CWndInfo(QObject *parent = nullptr);
	virtual ~CWndInfo();


protected:

	QWeakPointer<CWndInfo>						m_ParentWindow;
	QMap<quint64, QSharedPointer<CWndInfo> >	m_ChildWindows;

	mutable QReadWriteLock			m_Mutex;
};

typedef QSharedPointer<CWndInfo> CWndPtr;
typedef QWeakPointer<CWndInfo> CWndRef;