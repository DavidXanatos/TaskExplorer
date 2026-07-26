#include "stdafx.h"
#include "LinuxService.h"
#include "LinuxHelper.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>

CLinuxService::CLinuxService(QObject *parent)
	: CServiceInfo(parent)
{
	m_ProcessId = 0;
}

CLinuxService::~CLinuxService()
{
}

bool CLinuxService::InitStaticData(const QString& UnitName)
{
	QWriteLocker Locker(&m_Mutex);
	m_SvcName = UnitName;
	//
	// Only the name is set here. The unit properties (FragmentPath, ExecStart,
	// MainPID, ...) come from FetchProperties, which the API layer calls when
	// the service is actually displayed - fetching them for every unit on every
	// refresh would mean a D-Bus round trip per unit.
	//
	return true;
}

bool CLinuxService::FetchProperties(bool bFragmentPathOnly)
{
	const QString Path = GetObjectPath();
	if (Path.isEmpty())
		return false;

	QDBusConnection Bus = QDBusConnection::systemBus();
	if (!Bus.isConnected())
		return false;

	QDBusInterface Props("org.freedesktop.systemd1", Path, "org.freedesktop.DBus.Properties", Bus);
	if (!Props.isValid())
		return false;

	bool bChanged = false;

	//
	// FragmentPath is the unit file backing this service. It never changes for
	// the life of the unit, so it is read once and then left alone.
	//
	if (!m_bHaveFragmentPath)
	{
		QDBusReply<QVariant> Reply = Props.call("Get", "org.freedesktop.systemd1.Unit", "FragmentPath");
		if (Reply.isValid())
		{
			const QString FragmentPath = Reply.value().toString();
			QWriteLocker Locker(&m_Mutex);
			m_BinaryPath = FragmentPath;
			m_FileName = FragmentPath;
			m_bHaveFragmentPath = true;
			bChanged = true;
		}
	}

	if (bFragmentPathOnly)
		return bChanged;

	//
	// MainPID is 0 for anything not currently running, including oneshot units
	// that have already exited.
	//
	QDBusReply<QVariant> Reply = Props.call("Get", "org.freedesktop.systemd1.Service", "MainPID");
	if (Reply.isValid())
	{
		const quint64 MainPID = Reply.value().toUInt();
		QWriteLocker Locker(&m_Mutex);
		if (m_ProcessId != MainPID)
		{
			m_ProcessId = MainPID;
			bChanged = true;
		}
	}

	//
	// Whether the unit's cgroup is frozen, which is what IsPaused() reports.
	// Absent on systemd older than 246, in which case the unit is simply never
	// considered paused.
	//
	QDBusReply<QVariant> FreezerReply = Props.call("Get", "org.freedesktop.systemd1.Unit", "FreezerState");
	if (FreezerReply.isValid())
	{
		const QString FreezerState = FreezerReply.value().toString();
		QWriteLocker Locker(&m_Mutex);
		if (m_FreezerState != FreezerState)
		{
			m_FreezerState = FreezerState;
			bChanged = true;
		}
	}

	return bChanged;
}

bool CLinuxService::UpdateDynamicData(const QString& LoadState, const QString& ActiveState,
                                      const QString& SubState, const QString& Description)
{
	QWriteLocker Locker(&m_Mutex);

	bool bChanged = (m_LoadState != LoadState) || (m_ActiveState != ActiveState) || (m_SubState != SubState);

	m_LoadState = LoadState;
	m_ActiveState = ActiveState;
	m_SubState = SubState;
	m_DisplayName = Description;

	return bChanged;
}

bool CLinuxService::IsStopped() const
{
	QReadLocker Locker(&m_Mutex);
	return m_ActiveState == "inactive" || m_ActiveState == "failed";
}

bool CLinuxService::IsRunning(bool bStrict) const
{
	QReadLocker Locker(&m_Mutex);
	if (bStrict)
		return m_ActiveState == "active" && m_SubState == "running";
	return m_ActiveState == "active" || m_ActiveState == "activating";
}

bool CLinuxService::IsPaused() const
{
	//
	// systemd has no "paused" state as such; a unit frozen through the cgroup
	// freezer is the equivalent, and it reports that as FreezerState.
	//
	// Values are running / freezing / frozen / thawing.
	//
	QReadLocker Locker(&m_Mutex);
	return m_FreezerState == "frozen" || m_FreezerState == "freezing";
}

QString CLinuxService::GetStateString() const
{
	QReadLocker Locker(&m_Mutex);
	if (m_SubState.isEmpty())
		return m_ActiveState;
	return QString("%1 (%2)").arg(m_ActiveState).arg(m_SubState);
}

//
// Turns a D-Bus error reply into something a user can act on. The raw error
// names are precise but opaque, and the ones below are the cases that actually
// come up when driving systemd from a desktop session.
//
static STATUS DBusErrorToStatus(const QString& Unit, const QString& Action, const QDBusError& Error)
{
	const QString Name = Error.name();

	if (Name == "org.freedesktop.DBus.Error.InteractiveAuthorizationRequired")
	{
		return ERR(QObject::tr("Not authorised to %1 %2, and no interactive authentication agent is available. "
		                       "Run TaskExplorer from a desktop session with a polkit agent, or as root.")
		           .arg(Action).arg(Unit));
	}

	if (Name == "org.freedesktop.DBus.Error.AccessDenied")
		return ERR(QObject::tr("Not authorised to %1 %2.").arg(Action).arg(Unit));

	if (Name == "org.freedesktop.systemd1.NoSuchUnit")
		return ERR(QObject::tr("The unit %1 no longer exists.").arg(Unit));

	if (Name == "org.freedesktop.systemd1.UnitMasked")
		return ERR(QObject::tr("The unit %1 is masked and cannot be started.").arg(Unit));

	if (Name == "org.freedesktop.DBus.Error.UnknownMethod")
	{
		return ERR(QObject::tr("This operation is not supported by the running version of systemd."));
	}

	if (Name == "org.freedesktop.DBus.Error.NoReply" || Name == "org.freedesktop.DBus.Error.Timeout")
		return ERR(QObject::tr("Timed out waiting for systemd to %1 %2.").arg(Action).arg(Unit));

	// Anything else: systemd's own message is usually descriptive.
	const QString Message = Error.message();
	if (!Message.isEmpty())
		return ERR(QObject::tr("Failed to %1 %2: %3").arg(Action).arg(Unit).arg(Message));

	return ERR(QObject::tr("Failed to %1 %2: %3").arg(Action).arg(Unit).arg(Name));
}

STATUS CLinuxService::CallManager(const QString& Method, const QVariantList& Arguments)
{
	QDBusConnection Bus = QDBusConnection::systemBus();
	if (!Bus.isConnected())
		return ERR(tr("Cannot connect to the system bus."));

	QDBusMessage Call = QDBusMessage::createMethodCall("org.freedesktop.systemd1",
	                                                   "/org/freedesktop/systemd1",
	                                                   "org.freedesktop.systemd1.Manager",
	                                                   Method);
	Call.setArguments(Arguments);

	//
	// Without this flag polkit fails the call with
	// InteractiveAuthorizationRequired rather than asking the user to
	// authenticate, so an unprivileged user could never start or stop anything.
	//
	Call.setInteractiveAuthorizationAllowed(true);

	//
	// The reply arrives as soon as systemd has enqueued the job, not when the
	// job completes - so this is normally fast. The long timeout is for the
	// polkit prompt, which blocks until the user answers it.
	//
	const QDBusMessage Reply = Bus.call(Call, QDBus::Block, 120 * 1000);

	if (Reply.type() == QDBusMessage::ErrorMessage)
	{
		// Derive a readable verb from the method name for the error text.
		QString Action = Method;
		Action.remove("Unit");
		return DBusErrorToStatus(GetUnitName(), Action.toLower(), QDBusError(Reply));
	}

	return OK;
}

STATUS CLinuxService::Start()
{
	//
	// "replace" is the standard job mode: it displaces any conflicting queued
	// job rather than failing, which is what systemctl start does.
	//
	return CallManager("StartUnit", QVariantList() << GetUnitName() << "replace");
}

STATUS CLinuxService::Stop()
{
	return CallManager("StopUnit", QVariantList() << GetUnitName() << "replace");
}

STATUS CLinuxService::Pause()
{
	//
	// Freezing suspends every process in the unit's cgroup, which is the
	// closest equivalent to pausing a Windows service. It needs systemd 246 or
	// newer and only applies to a unit that is actually running - both cases
	// surface as a D-Bus error.
	//
	return CallManager("FreezeUnit", QVariantList() << GetUnitName());
}

STATUS CLinuxService::Continue()
{
	return CallManager("ThawUnit", QVariantList() << GetUnitName());
}

STATUS CLinuxService::Delete(bool bForce)
{
	//
	// Deliberately not implemented.
	//
	// On Windows deleting a service removes a registry registration this
	// application created a view of. A systemd unit is a file on disk that
	// almost always belongs to a distribution package, so "delete" would mean
	// removing a packaged file - destructive, not reversible from here, and
	// liable to be silently restored by the next package update.
	//
	// The two operations a user actually wants are offered by systemctl and
	// are named here rather than guessed at.
	//
	return ERR(tr("Deleting a systemd unit is not supported: the unit file usually belongs to a distribution package. "
	              "Use 'systemctl disable %1' to stop it starting at boot, or 'systemctl mask %1' to prevent it "
	              "from being started at all.").arg(GetUnitName()));
}
