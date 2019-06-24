#pragma once
#include <qobject.h>
#include "AbstractInfo.h"

#include "ModuleInfo.h"

class CDriverInfo: public CAbstractInfoEx
{
	Q_OBJECT

public:
	CDriverInfo(QObject *parent = nullptr);
	virtual ~CDriverInfo();

	virtual QString GetFileName() const				{ QReadLocker Locker(&m_Mutex); return m_FileName; }
	virtual QString GetBinaryPath() const			{ QReadLocker Locker(&m_Mutex); return m_BinaryPath; }

	virtual CModulePtr GetModuleInfo()				{ QReadLocker Locker(&m_Mutex); return m_pModuleInfo; }

protected:

	QString							m_FileName;
	QString							m_BinaryPath;

	CModulePtr						m_pModuleInfo;
};

typedef QSharedPointer<CDriverInfo> CDriverPtr;
typedef QWeakPointer<CDriverInfo> CDriverRef;