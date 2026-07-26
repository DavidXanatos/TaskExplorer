#pragma once
#include "../ModuleInfo.h"

//
// A mapped shared object, coalesced from the file-backed entries of
// /proc/<pid>/maps (one .so contributes several segments; they collapse into a
// single module keyed by its lowest base address).
//
class CLinuxModule : public CModuleInfo
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxModule)
public:
	CLinuxModule(QObject *parent = nullptr);
	virtual ~CLinuxModule();

	virtual bool			InitStaticData(const QString& FileName, quint64 BaseAddress, quint64 Size);

	virtual STATUS			Unload(bool bForce = false);

	virtual bool			IsExecutable() const	{ QReadLocker Locker(&m_Mutex); return m_IsExecutable; }

protected:
	bool					m_IsExecutable;
};

typedef QSharedPointer<CLinuxModule> CLinuxModulePtr;
typedef QWeakPointer<CLinuxModule> CLinuxModuleRef;
