#pragma once
#include <QFuture>
#include "SystemAPI.h"
#include "../../MiscHelpers/Common/Credentials.h"

class QHostAddress;

//
// A directory of systems, not a connection manager.
//
// Each system owns its own link - connect, retry, drop, fingerprint check all
// live on CRemoteSystem - so what is left over to share between them is just
// two things: the list of nodes somebody configured by hand, and the optional
// discovery that can suggest more. That is all this holds.
//
// It is deliberately optional. In the ordinary single-machine case, and when a
// GUI is simply pointed at one remote core, theCluster stays NULL and theSystem
// is the whole story. Ask through GetSystems() rather than testing theCluster
// for NULL at every call site - it answers correctly either way.
//
//
// A machine somebody configured, whether or not it is currently reachable.
//
// Saved entries and discovered ones are the same kind of thing in different
// states, which is why the state is a field rather than two lists. A discovered
// machine is shown and not connected; a saved one is shown whether the
// connection is up or not, so that "the build box is down" is something the
// window can say rather than something it omits.
//
struct STarget
{
	//
	// What the user calls it, and what identifies it.
	//
	// Name is the entry's identity for the saved list - it is what a person
	// typed and what the tree shows when there is no connection to ask. The
	// machine id is the server's own random value and is what says whether the
	// thing that answered is the thing that was saved; a host name addresses a
	// machine but does not identify one. See API_HS_MACHINEID.
	//
	QString			Name;
	QString			Address;		// pipe name, or host:port
	QString			MachineId;		// learnt on the first successful connection

	enum EState
	{
		eSaved = 0,		// configured, never connected this session
		eConnecting,
		eConnected,
		eFailed,

		//
		// Was connected and the far side went away, as opposed to never having
		// answered. Different because what is on screen is real, just no longer
		// current - see CCluster::OnConnectionLost.
		//
		eLost,

		eDiscovered		// announced itself; never connected automatically
	};
	EState			State = eSaved;

	//
	// The last thing that went wrong, as a code rather than a sentence - the
	// viewer writes the words. Zero when nothing has.
	//
	quint32			LastError = 0;

	CSystemPtr		pSystem;		// only while connected

	//
	// When the last automatic reconnection attempt was made. Zero means none
	// yet, so a machine that has just gone is tried at once rather than after a
	// full interval.
	//
	quint64			LastRetry = 0;

	//
	// This machine, read through a daemon running on it rather than collected
	// directly. Not a machine anybody configured, so it is never written to
	// Targets.xml - it is found at startup or it is not there.
	//
	bool			bLocalDaemon = false;
};

class TASKCORE_EXPORT CCluster : public QObject
{
	Q_OBJECT

public:
	CCluster(QObject* parent = nullptr);
	virtual ~CCluster();

	//
	// The configured machines, in the order they were added. The local machine
	// is not one of them - it is always present and cannot be disconnected, so
	// it has no entry to configure.
	//
	QList<STarget>				GetTargets() const;
	bool						HasTarget(const QString& Name) const;

	//
	// The address a connected system was reached at - the pipe name, or
	// host:port. Empty for anything not in the target list, which includes the
	// local machine: it has no address because nothing was dialled to reach it.
	//
	QString						GetTargetAddress(CSystemAPI* pSystem) const;

	//
	// The entry a live connection came from, or false when the system is not
	// one of ours - the local machine, most obviously, which was never dialled.
	//
	bool						GetTargetInfo(CSystemAPI* pSystem, QString* pName, QString* pAddress) const;

	//
	// Connect, and add the machine to the list if it is not there yet.
	//
	// Returns the system on success and leaves the target in eFailed with
	// LastError set otherwise, so a caller that wants to report the failure and
	// one that wants to show it in the tree read the same place.
	//
	//
	// What to do when the machine that answers is not the one saved under this
	// name.
	//
	// Refusing is the default and the caller has to say otherwise, because the
	// alternative was what this did before: record whatever answered, silently,
	// so that the one case the machine id exists to catch was the one case
	// nothing reported. Accepting is a decision a person makes with both ids in
	// front of them - which is why it is a second call rather than a flag on the
	// first.
	//
	enum EIdentityPolicy
	{
		eRequireSaved = 0,	// refuse anything that is not the saved machine
		eAcceptNew			// the user has looked and says this is the machine now
	};

	//
	// bTakeView is what somebody asking for a machine gets and a reconnection
	// running by itself does not: connecting because a person typed an address
	// is a request to look at that machine, while a saved entry coming back on
	// its own must not move the window out from under them.
	//
	CSystemPtr					Connect(const QString& Name, const QString& Address, STATUS* pStatus = NULL,
										const SCredentials& Cred = SCredentials(),
										EIdentityPolicy Identity = eRequireSaved,
										bool bTakeView = true);
	void						Disconnect(const QString& Name);

private:
	//
	// The saved network machines, reconnected off the interface thread.
	//
	// Each handshake is a round trip with a five second timeout, and they were
	// done one after another before the window was shown - so a machine that was
	// switched off cost five seconds of a program that looked hung, before it
	// had drawn the local machine it did not need any of this for.
	//
	// Waited for in the destructor rather than detached: the worker touches this
	// object, and a program closing while one of those timeouts is still running
	// is exactly when it would be gone.
	//
	QFuture<void>				m_ConnectFuture;

public:

	void						AddTarget(const QString& Name, const QString& Address);
	void						RemoveTarget(const QString& Name);

	//
	// Changes a saved machine in place - its name, its address, or both.
	//
	// In place rather than remove-and-add, because the entry carries something
	// neither of those two arguments does: the machine id it was last seen with,
	// which is what catches an address that has come to reach a different
	// machine. Recreating the entry would throw that away and the next connection
	// would accept whatever answered.
	//
	// A connected machine keeps its connection: the address is what a *future*
	// connection uses, and dropping a working one to rename its row would be a
	// surprise. False when there is no such machine, or when the new name is
	// already another's.
	//
	bool						UpdateTarget(const QString& OldName, const QString& NewName,
									const QString& Address);

	//
	// The saved list, in the XML the rest of the application already uses for
	// things that are not settings - see CSystemAPI's process presets.
	//
	// Deliberately not the ini: a target is a small record with fields, and
	// there will be more of them (a fingerprint, a credential reference) before
	// long.
	//
	void						LoadTargets();
	void						StoreTargets();

	//
	// Every system currently in view. Without a cluster that is the one system
	// theSystem points at; with one it is the configured node list. Callers
	// that mean "for each machine" should use this and nothing else, so that
	// the single-system case never has to be special-cased - and so that no
	// site can forget to include the local machine.
	//
	static QList<CSystemPtr>	GetSystems();
	static bool					IsActive();

	//
	// The machine the panels are showing.
	//
	// Separate from theSystem on purpose. theSystem is what this process
	// *collects* - the local machine, always, and the thing the graph bar and
	// the status line report. The view system is only a question of what is
	// being looked at, which follows the selection in the tree and may be any
	// connected machine. Nothing about collection changes when it moves.
	//
	// Always answers something: with no cluster, or with nothing selected, or
	// after the selected machine went away, that something is theSystem.
	//
	static CSystemPtr			GetViewSystem();

	//
	// Resolved against the node list rather than stored as given, so a pointer
	// taken from a row that has since gone cannot be kept. An unknown pointer
	// - the caller's own idea of a machine, or one already disconnected -
	// falls back to the local machine.
	//
	static void					SetViewSystem(CSystemAPI* pSystem);

	//
	// The machine the graph bar, the status line and the tray graph report on,
	// as opposed to the one the panels are showing.
	//
	// With the machine layer off the two are the same thing: there is one
	// machine on screen and the window is about that one - title bar, panels,
	// graph bar, status line and all.
	//
	// With it on they come apart on purpose. The tree holds every machine and
	// the panels follow whatever row is selected, which moves constantly; the
	// graphs are a history and must not be thrown away every time somebody
	// clicks a process. So the graphs follow their own selection, made in the
	// toolbar's machine box and nowhere else.
	//
	static CSystemPtr			GetActiveSystem();

	//
	// Points the graphs at a machine. Only meaningful with the machine layer
	// on - without it there is nothing to choose between, and the graphs follow
	// the view like everything else.
	//
	// Resolved against the node list like SetViewSystem, for the same reason.
	//
	static void					SetGraphSystem(CSystemAPI* pSystem);

	//
	// Whether the machine layer is drawn. A presentation switch, kept here
	// because it decides what GetActiveSystem answers and every reader of that
	// would otherwise have to ask the tree.
	//
	// Static, and not a member, because it has to answer before anything is
	// connected - which is when theCluster does not exist yet.
	//
	static bool					IsClusterMode()		{ return m_bClusterMode; }
	static void					SetClusterMode(bool bSet);

	//
	// This machine's own daemon.
	//
	// A viewer can read the machine it runs on in two ways: collect it itself,
	// or ask a TaskServer on it. The second is the point of running a daemon at
	// all - it can be privileged where the viewer is not - so a viewer looks for
	// one at startup and uses it when it is there.
	//
	// IsPresent only asks whether something is listening on the endpoint. It
	// does not speak the protocol, and it is not meant to: it exists to decide
	// whether to offer the choice, and connecting is what finds out whether the
	// thing that answered is a TaskServer.
	//
	static QString				GetLocalEndpoint();
	static bool					IsLocalDaemonPresent();
	CSystemPtr					ConnectLocalDaemon(STATUS* pStatus = NULL);

	//
	// Connect the machines that were saved, using the keys that were saved with
	// them.
	//
	// Without this a saved machine was loaded into the list and then sat there:
	// the retry timer only ever looked at entries in eLost - "was connected and
	// went away" - and one just read from the file has never been connected, so
	// nothing ever tried. The list remembered the machine and the viewer behaved
	// as though it had forgotten it.
	//
	// Entries with no saved key are left alone rather than attempted: a
	// connection needs an identity and a key, and the honest thing for a machine
	// whose key was never kept is to sit in the list until somebody supplies one
	// rather than to fail every ten seconds for the rest of the session.
	//
	void						ConnectSavedTargets();
	static bool					IsLocalDaemon(CSystemAPI* pSystem);

	//
	// Whether a machine can answer a given command.
	//
	// A machine this process collects from answers for itself, so the question
	// only has content for a remote one, where it is the feature list from the
	// handshake. Asking before offering is the whole point: a view that shows an
	// empty list reads as "there is nothing", which is a different statement
	// from "this was never asked".
	//
	//
	// Whether the machine can answer one of CSystemAPI::EFeature. Always true
	// for a local collector; asked of the far side for a remote one.
	//
	static bool					CanAnswer(CSystemAPI* pSystem, quint32 Feature);

	//
	// Whether the machine layer should be on.
	//
	// The setting, and something to make a second machine possible with. Without
	// TaskRemote there can only ever be one machine in view, and a tree that
	// groups one machine under a machine branch is a level of nesting that says
	// nothing. Asked here rather than at each reader so the tree and the window
	// cannot disagree about it.
	//
	static bool					ClusterModeWanted();

	//
	// What state a system's target is in, for anything that wants to say so on
	// screen. eConnected for the local machine, which is always there.
	//
	static STarget::EState		GetTargetState(CSystemAPI* pSystem);

	//
	// Why the last attempt failed, as a code. Zero when nothing has.
	//
	// The state says a machine is not connected; only this says whether that is
	// because nothing answered or because something did and was refused. They
	// look identical in the tree otherwise, and they send whoever is reading it
	// to opposite places - one to the network, one to the machine.
	//
	static quint32				GetTargetError(CSystemAPI* pSystem);

	//
	// ---- finding machines, and being asked to stop ----
	//
	// Off unless switched on, and switched on separately from anything the
	// daemon on this machine does. A viewer that only ever connects to two
	// machines it already knows has no reason to be sending anything to a
	// multicast group.
	//
	// Announcements arrive as targets in the eDiscovered state: shown, never
	// connected, never saved. Connecting to one is a decision, and it is the
	// same decision as typing the address - which is why it goes through the
	// same dialog.
	//
	bool						StartDiscovery(QString* pError = nullptr);
	void						StopDiscovery();
	bool						IsDiscovering() const;

	//
	// Asks now rather than waiting out the interval - what the connect dialog
	// calls when it opens.
	//
	void						RefreshDiscovery();

	//
	// Try the machines whose connection died, one attempt each.
	//
	// A machine stays in the tree until somebody disconnects it, so as long as
	// it is there the viewer keeps trying to reach it - at
	// Options/ReconnectInterval, ten seconds by default. Called from the
	// refresh, which is the one place that runs on a clock.
	//
	static void					RetryLostNodes();

	QList<CSystemPtr>			GetNodes() const;
	void						AddNode(const CSystemPtr& pSystem);
	void						RemoveNode(const CSystemPtr& pSystem);

signals:
	void						NodeAdded(const CSystemPtr& pSystem);
	void						NodeRemoved(const CSystemPtr& pSystem);

	//
	// Something about the list changed - a target added, removed, connected or
	// dropped. One signal rather than four, because every listener redraws the
	// same things regardless of which it was.
	//
	void						TargetsChanged();

	//
	// The saved machines being connected at startup, one at a time.
	//
	// TargetsChanged says the list moved; it does not say what is being waited
	// for, and a handshake to a machine that is switched off takes five seconds
	// in which the window has nothing to say for itself. These two are that
	// sentence: which machine is being tried now, and what came of the whole
	// run.
	//
	// Emitted from the worker thread that does the handshakes - see
	// ConnectSavedTargets - so anything connected to them is delivered on the
	// receiver's own thread, which for the window is the one that may touch it.
	//
	void						ConnectingTo(const QString& Name, int Index, int Count);

	//
	// Name is the machine, and only means anything when there was one of them -
	// "connected to the build box" is worth saying, "connected to the build box"
	// when three were tried is not.
	//
	void						ConnectRunFinished(int Connected, int Count, const QString& Name);

	//
	// The panels are now looking at a different machine. Everything they hold
	// belongs to the old one, so this is a "throw it away and ask again", not a
	// "redraw".
	//
	void						ViewSystemChanged();

	//
	// The graphs are now about a different machine. Separate from the one
	// above because the two selections are separate: the panels reload on that
	// one, the graph bar and the status line on this one, and in cluster mode
	// each moves without disturbing the other.
	//
	void						ActiveSystemChanged();

private slots:
	//
	// A connection died on its own, as opposed to being dropped from the menu.
	//
	void						OnConnectionLost();
	void						OnReconnectRefused(quint32 MsgCode);
	void						OnDiscovered(const QString& Address, const QString& HostName, const QString& MachineId);
	void						OnReconnected();

public:

protected:
	//
	// Discovery belongs here rather than on a system, because it is what
	// produces candidate systems in the first place. It is not built yet; when
	// it is, two things have to hold: announcing is opt-in on the daemon and
	// listening is opt-in here, and a discovered node is never connected
	// automatically. Anything in a response - host name, version, the
	// advertised fingerprint - is attacker-controlled display text until
	// authentication has succeeded.
	//

	QList<CSystemPtr>			m_Nodes;
	QList<STarget>				m_Targets;
	//
	// The module's, not one of ours - see CRemoteLoader::CreateDiscovery. Null
	// when nothing is listening, which is also the case on a build that has no
	// module to listen with.
	//
	class CRemoteDiscovery*		m_pDiscovery = NULL;

	// Null means the local machine; see GetViewSystem.
	CSystemPtr					m_ViewSystem;

	// The same, for the graphs; see GetActiveSystem. Read only in cluster mode.
	CSystemPtr					m_GraphSystem;

	static bool					m_bClusterMode;
	mutable QReadWriteLock		m_Mutex;

	int							FindTarget(const QString& Name) const;
};

//
// NULL unless cluster mode is on. Nothing may assume it exists.
//
extern TASKCORE_EXPORT CCluster*		theCluster;
