#pragma once
#include "../ServiceInfo.h"

//
// A systemd unit, surfaced through the shared "service" abstraction.
//
// The unit list comes from org.freedesktop.systemd1.Manager.ListUnits over the
// system bus; start/stop/restart go through the same interface, which routes
// authorisation through polkit rather than requiring the UI to be root.
//
class CLinuxService : public CServiceInfo
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxService)
public:
	CLinuxService(QObject *parent = nullptr);
	virtual ~CLinuxService();

	// LoadState/ActiveState/SubState as reported by systemd.
	virtual bool			InitStaticData(const QString& UnitName);
	virtual bool			UpdateDynamicData(const QString& LoadState, const QString& ActiveState,
	                                          const QString& SubState, const QString& Description);

	//
	// MainPID and FragmentPath are not part of the ListUnits reply and need a
	// per-unit D-Bus property read, so they are fetched separately and only for
	// units where they mean anything (see CLinuxAPI::UpdateServiceList).
	//
	// ObjectPath is the unit's D-Bus object, taken from the ListUnits reply.
	//
	virtual void			SetObjectPath(const QString& Path)	{ QWriteLocker Locker(&m_Mutex); m_ObjectPath = Path; }
	virtual QString			GetObjectPath() const			{ QReadLocker Locker(&m_Mutex); return m_ObjectPath; }
	virtual bool			FetchProperties(bool bFragmentPathOnly);

	virtual bool			IsStopped() const;
	virtual bool			IsRunning(bool bStrict = false) const;
	virtual bool			IsPaused() const;
	virtual QString			GetStateString() const;

	virtual STATUS			Start();
	virtual STATUS			Pause();
	virtual STATUS			Continue();
	virtual STATUS			Stop();
	virtual STATUS			Delete(bool bForce = false);

	virtual QString			GetUnitName() const	{ QReadLocker Locker(&m_Mutex); return m_SvcName; }
	virtual QString			GetActiveState() const	{ QReadLocker Locker(&m_Mutex); return m_ActiveState; }
	virtual QString			GetSubState() const	{ QReadLocker Locker(&m_Mutex); return m_SubState; }

	//
	// The unit type, i.e. the suffix: "service", "socket", "timer", ...
	//
	// Worth a column of its own now that the list is not only services: a
	// timer and the service it triggers otherwise look alike.
	//
	virtual QString			GetTypeString() const	{ QReadLocker Locker(&m_Mutex); return m_SvcName.section('.', -1); }

protected:
	//
	// Invokes a method on org.freedesktop.systemd1.Manager for this unit.
	//
	// Authorisation is handled by polkit, which is why TaskExplorer does not
	// need to run as root to control services - but the call must explicitly
	// permit interactive authorisation, or polkit rejects it outright instead
	// of prompting the user for credentials.
	//
	STATUS					CallManager(const QString& Method, const QVariantList& Arguments);

protected:
	QString					m_LoadState;
	QString					m_ActiveState;	// active / inactive / failed / activating / deactivating
	QString					m_SubState;	// running / exited / dead / ...
	QString					m_FreezerState;	// running / freezing / frozen / thawing
	QString					m_ObjectPath;
	bool					m_bHaveFragmentPath = false;
};

typedef QSharedPointer<CLinuxService> CLinuxServicePtr;
typedef QWeakPointer<CLinuxService> CLinuxServiceRef;
