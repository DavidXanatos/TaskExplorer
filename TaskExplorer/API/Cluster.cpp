#include "stdafx.h"
#include "Cluster.h"
#include <QtConcurrent>
#include "../../MiscHelpers/Common/CredentialStore.h"

#include "RemoteApi.h"
#include "../../MiscHelpers/Common/Xml.h"
#include "../../MiscHelpers/Common/Settings.h"
#include "../../MiscHelpers/Common/Credentials.h"
#include "../version.h"

CCluster*	theCluster = NULL;

bool CCluster::m_bClusterMode = false;

CCluster::CCluster(QObject* parent)
	: QObject(parent)
{
	//
	// The local system is a node like any other, and goes in first so that
	// GetSystems() never depends on a caller remembering to include it.
	//
	if (!theSystem.isNull())
		m_Nodes.append(theSystem);
}

CCluster::~CCluster()
{
	//
	// Before anything this object owns goes away. See m_ConnectFuture.
	//
	m_ConnectFuture.waitForFinished();

	theCluster = NULL;
}

bool CCluster::IsActive()
{
	return theCluster != NULL;
}

QList<CSystemPtr> CCluster::GetSystems()
{
	if (theCluster)
		return theCluster->GetNodes();

	if (theSystem.isNull())
		return QList<CSystemPtr>();
	return QList<CSystemPtr>() << theSystem;
}

CSystemPtr CCluster::GetViewSystem()
{
	if (theCluster)
	{
		QReadLocker Locker(&theCluster->m_Mutex);
		if (!theCluster->m_ViewSystem.isNull())
			return theCluster->m_ViewSystem;
	}
	return theSystem;
}

QString CCluster::GetLocalEndpoint()
{
	//
	// Settable, because a machine may run its daemon under another name - two
	// of them side by side, or a packaging that renames it. The default is the
	// one the daemon itself uses when told nothing.
	//
	return theConf ? theConf->GetString("Options/LocalDaemon", MY_DAEMON_NAME_STRING) : QString(MY_DAEMON_NAME_STRING);
}

bool CCluster::IsLocalDaemonPresent()
{
	//
	// Asked of the module, which is the only thing that could act on a yes:
	// finding a daemon is the first half of connecting to it, and without
	// TaskRemote the second half returns TE_NoRemoteModule. So on a build that
	// cannot connect this is false whatever is listening, and the window stops
	// offering to switch to a daemon it could not reach.
	//
	// The name is resolved here and passed in - see CRemoteLoader::ProbeEndpoint.
	//
	return CRemoteLoader::ProbeEndpoint(GetLocalEndpoint());
}

STarget::EState CCluster::GetTargetState(CSystemAPI* pSystem)
{
	//
	// The local machine has no target entry and needs none: it is here, it is
	// connected, and it cannot be otherwise.
	//
	if (!theCluster || !pSystem || pSystem == theSystem.data())
		return STarget::eConnected;

	QReadLocker Locker(&theCluster->m_Mutex);
	foreach(const STarget& Target, theCluster->m_Targets)
	{
		if (Target.pSystem.data() == pSystem)
			return Target.State;
	}
	return STarget::eConnected;
}

quint32 CCluster::GetTargetError(CSystemAPI* pSystem)
{
	if (!theCluster || !pSystem)
		return 0;

	QReadLocker Locker(&theCluster->m_Mutex);
	foreach(const STarget& Target, theCluster->m_Targets)
	{
		if (Target.pSystem.data() == pSystem)
			return Target.LastError;
	}
	return 0;
}

//
// A connection died on its own.
//
// The node is deliberately *not* removed here. What is on screen came from a
// real machine and is still true of the moment it was taken, so it stays for the
// persistence window - drawn as leaving, the same as any process that exits -
// and goes when that window closes. Removing the node at once would empty the
// branch under whoever was looking at it.
//
void CCluster::OnConnectionLost()
{
	CSystemAPI* pSystem = qobject_cast<CSystemAPI*>(sender());
	IRemoteSystem* pRemote = pSystem ? pSystem->GetRemote() : NULL;
	if (!pRemote)
		return;

	{
		QWriteLocker Locker(&m_Mutex);
		int Index = -1;
		for (int i = 0; i < m_Targets.count(); i++) {
			if (m_Targets[i].pSystem.data() == pSystem) { Index = i; break; }
		}
		if (Index == -1 || m_Targets[Index].State == STarget::eLost)
			return;		// already handled, or not one of ours

		m_Targets[Index].State = STarget::eLost;
	}

	pRemote->MarkEverythingGone();

	emit TargetsChanged();
}



//
// Keep trying, for as long as the machine is in the tree.
//
// A machine stays until somebody disconnects it, which is what makes an
// automatic retry the right thing rather than a nuisance: the entry *is* the
// standing instruction to be connected to that machine, and removing it is how
// you say stop. There is nothing to back off from and nothing to give up on.
//
// The attempt is posted rather than called. CRemoteSystem::Connect blocks its
// caller until the handshake answers or times out, and the caller here is the
// refresh timer on the GUI thread - a machine that is down would freeze the
// window for the length of every attempt.
//
void CCluster::RetryLostNodes()
{
	if (!theCluster)
		return;

	//
	// Seconds in the file, milliseconds here; see the settings dialog. Floored
	// at one second, because a value of zero would mean an attempt per refresh
	// and there is nothing useful that could do.
	//
	const quint64 Interval = 1000 * (quint64)qMax(1, theConf ? theConf->GetInt("Options/ReconnectInterval", 10) : 10);
	const quint64 Now = GetCurTick();

	QList<IRemoteSystem*> Try;
	{
		QWriteLocker Locker(&theCluster->m_Mutex);
		for (int i = 0; i < theCluster->m_Targets.count(); i++)
		{
			STarget& Target = theCluster->m_Targets[i];
			if (Target.State != STarget::eLost || Target.pSystem.isNull())
				continue;

			if (Target.LastRetry != 0 && Now - Target.LastRetry < Interval)
				continue;
			Target.LastRetry = Now;

			if (IRemoteSystem* pRemote = Target.pSystem->GetRemote())
				Try.append(pRemote);
		}
	}

	//
	// Posted as a functor rather than by method name. Same queueing - which is
	// the whole point, see above - but the compiler checks it, and there is no
	// longer a class here whose slot could be named.
	//
	foreach(IRemoteSystem* pRemote, Try)
		QMetaObject::invokeMethod(pRemote->AsSystem(),
			[pRemote]() { pRemote->TryReconnect(); }, Qt::QueuedConnection);
}

//
// One attempt came back, and it worked.
//
// The same system object, deliberately: the tree keys a machine branch by the
// system pointer, so reusing it means the branch, its expanded state and the
// selection survive a machine going away and coming back.
//
//
// An automatic attempt found something that is not this machine.
//
// Marked failed rather than left lost, which is what stops it being retried:
// RetryLostNodes only looks at eLost. It stays in the tree with its error, so
// the answer to "why did that machine never come back" is on screen instead of
// being a thing that quietly never happened.
//
void CCluster::OnReconnectRefused(quint32 MsgCode)
{
	CSystemAPI* pSystem = qobject_cast<CSystemAPI*>(sender());
	if (!pSystem || !pSystem->GetRemote())
		return;

	{
		QWriteLocker Locker(&m_Mutex);
		for (int i = 0; i < m_Targets.count(); i++)
		{
			if (m_Targets[i].pSystem.data() != pSystem)
				continue;
			m_Targets[i].State = STarget::eFailed;
			m_Targets[i].LastError = MsgCode;
			break;
		}
	}

	emit TargetsChanged();
}

void CCluster::OnReconnected()
{
	CSystemAPI* pSystem = qobject_cast<CSystemAPI*>(sender());
	if (!pSystem || !pSystem->GetRemote())
		return;

	{
		QWriteLocker Locker(&m_Mutex);
		for (int i = 0; i < m_Targets.count(); i++)
		{
			if (m_Targets[i].pSystem.data() != pSystem)
				continue;
			m_Targets[i].State = STarget::eConnected;
			m_Targets[i].LastRetry = 0;
			break;
		}
	}

	emit TargetsChanged();
}

bool CCluster::ClusterModeWanted()
{
	if (!theConf || !theConf->GetBool("Options/ClusterMode", false))
		return false;

	//
	// Not merely hidden: with no module the setting cannot be acted on, and
	// leaving it on would draw a machine level over a list that will only ever
	// hold this machine.
	//
	return CRemoteLoader::IsAvailable();
}

bool CCluster::CanAnswer(CSystemAPI* pSystem, quint32 Feature)
{
	if (!pSystem)
		return false;

	//
	// A local collector is asked directly and either implements something or
	// does not; there is no negotiation and nothing to consult. Whether an
	// individual view has anything to show on this platform is that view's own
	// question and is answered where the platform is known.
	//
	IRemoteSystem* pRemote = pSystem->GetRemote();
	if (!pRemote)
		return true;

	return pRemote->CanAnswer(Feature);
}

bool CCluster::IsLocalDaemon(CSystemAPI* pSystem)
{
	if (!theCluster || !pSystem)
		return false;

	QReadLocker Locker(&theCluster->m_Mutex);
	foreach(const STarget& Target, theCluster->m_Targets)
	{
		if (Target.pSystem.data() == pSystem)
			return Target.bLocalDaemon;
	}
	return false;
}

CSystemPtr CCluster::ConnectLocalDaemon(STATUS* pStatus)
{
	const QString Endpoint = GetLocalEndpoint();

	//
	// Named for what it is rather than for the endpoint. The entry is not a
	// machine somebody configured and it is not written down; it exists for as
	// long as this run does.
	//
	const QString Name = tr("Local daemon");

	CSystemPtr pSystem = Connect(Name, Endpoint, pStatus);
	if (pSystem.isNull())
	{
		//
		// A failed attempt leaves an eFailed entry behind, and this one was
		// never the user's to keep. Removed rather than shown, so a machine
		// that simply has no daemon does not grow a permanent error row.
		//
		RemoveTarget(Name);
		return pSystem;
	}

	{
		QWriteLocker Locker(&m_Mutex);
		int Index = FindTarget(Name);
		if (Index != -1)
			m_Targets[Index].bLocalDaemon = true;
	}
	StoreTargets();	// rewrites the file without this entry in it
	return pSystem;
}

CSystemPtr CCluster::GetActiveSystem()
{
	//
	// Out of cluster mode there is one machine on screen and everything is
	// about it, so this is the view and nothing else.
	//
	if (!m_bClusterMode)
		return GetViewSystem();

	//
	// In cluster mode it is the machine box's own selection, which is the
	// point of having one: the tree holds every machine and the panels follow
	// the row that is selected, but a graph is a history and clicking a
	// process must not throw one away. So the two are separate, and this one
	// moves only when somebody says so.
	//
	if (theCluster)
	{
		QReadLocker Locker(&theCluster->m_Mutex);
		if (!theCluster->m_GraphSystem.isNull())
			return theCluster->m_GraphSystem;
	}
	return theSystem;
}

void CCluster::SetGraphSystem(CSystemAPI* pSystem)
{
	if (!theCluster)
		return;

	{
		QWriteLocker Locker(&theCluster->m_Mutex);

		CSystemPtr pFound;
		foreach(const CSystemPtr& pNode, theCluster->m_Nodes)
		{
			if (pNode.data() == pSystem) {
				pFound = pNode;
				break;
			}
		}

		//
		// The local machine is stored as null, as it is for the view - so that
		// "nothing selected" and "the local machine is selected" cannot drift
		// apart.
		//
		if (!pFound.isNull() && pFound == theSystem)
			pFound.clear();

		if (theCluster->m_GraphSystem == pFound)
			return;
		theCluster->m_GraphSystem = pFound;
	}

	emit theCluster->ActiveSystemChanged();
}

void CCluster::SetClusterMode(bool bSet)
{
	if (m_bClusterMode == bSet)
		return;
	//
	// The graphs' machine is taken from the view on the way in and dropped on
	// the way out.
	//
	// Taken, so that turning the layer on does not make the graphs jump to a
	// machine nobody picked - they go on showing what they were showing, and
	// the box now says so. Dropped, because out of cluster mode the graphs
	// follow the view and a stale selection kept here would come back the next
	// time the layer is turned on.
	//
	if (theCluster)
	{
		QWriteLocker Locker(&theCluster->m_Mutex);
		theCluster->m_GraphSystem = bSet ? theCluster->m_ViewSystem : CSystemPtr();
	}

	m_bClusterMode = bSet;

	//
	// What the window is about has just changed even though nothing connected
	// or disconnected, so everything that follows it has to be told - both the
	// panels, which may now be drawing every machine instead of one, and the
	// graphs, which have just changed which selection they obey.
	//
	if (theCluster)
	{
		emit theCluster->ViewSystemChanged();
		emit theCluster->ActiveSystemChanged();
	}
}

void CCluster::SetViewSystem(CSystemAPI* pSystem)
{
	//
	// Without a cluster there is one machine and nothing to point at, so this
	// is not an error - it is simply already true.
	//
	if (!theCluster)
		return;

	{
		QWriteLocker Locker(&theCluster->m_Mutex);

		CSystemPtr pFound;
		foreach(const CSystemPtr& pNode, theCluster->m_Nodes)
		{
			if (pNode.data() == pSystem) {
				pFound = pNode;
				break;
			}
		}

		//
		// The local machine is stored as null rather than as itself, so that
		// "nothing selected" and "the local machine is selected" are the same
		// state and cannot drift apart.
		//
		if (!pFound.isNull() && pFound == theSystem)
			pFound.clear();

		if (theCluster->m_ViewSystem == pFound)
			return;
		theCluster->m_ViewSystem = pFound;
	}

	emit theCluster->ViewSystemChanged();
}

QList<CSystemPtr> CCluster::GetNodes() const
{
	QReadLocker Locker(&m_Mutex);
	return m_Nodes;
}

void CCluster::AddNode(const CSystemPtr& pSystem)
{
	if (pSystem.isNull())
		return;

	{
		QWriteLocker Locker(&m_Mutex);
		if (m_Nodes.contains(pSystem))
			return;
		m_Nodes.append(pSystem);
	}

	emit NodeAdded(pSystem);
}

void CCluster::RemoveNode(const CSystemPtr& pSystem)
{
	bool bWasInView = false;
	bool bWasGraphed = false;
	{
		QWriteLocker Locker(&m_Mutex);
		if (!m_Nodes.removeOne(pSystem))
			return;

		if (m_ViewSystem == pSystem) {
			m_ViewSystem.clear();
			bWasInView = true;
		}
		if (m_GraphSystem == pSystem) {
			m_GraphSystem.clear();
			bWasGraphed = true;
		}
	}

	emit NodeRemoved(pSystem);
	if (bWasInView)
		emit ViewSystemChanged();
	if (bWasGraphed)
		emit ActiveSystemChanged();
}


int CCluster::FindTarget(const QString& Name) const
{
	for (int i = 0; i < m_Targets.count(); i++) {
		if (m_Targets[i].Name.compare(Name, Qt::CaseInsensitive) == 0)
			return i;
	}
	return -1;
}

QList<STarget> CCluster::GetTargets() const
{
	QReadLocker Locker(&m_Mutex);
	return m_Targets;
}

bool CCluster::HasTarget(const QString& Name) const
{
	QReadLocker Locker(&m_Mutex);
	return FindTarget(Name) != -1;
}

void CCluster::AddTarget(const QString& Name, const QString& Address)
{
	{
		QWriteLocker Locker(&m_Mutex);

		//
		// A name already in the list is the same machine, so the address is
		// taken as a correction rather than ignored - a box that moved is
		// re-entered in the connect dialog, and dropping the new address there
		// would connect to the new one and go on remembering the old.
		//
		int Index = FindTarget(Name);
		if (Index != -1)
		{
			if (m_Targets[Index].Address == Address)
				return;
			m_Targets[Index].Address = Address;
		}
		else
		{
			STarget Target;
			Target.Name = Name;
			Target.Address = Address;
			m_Targets.append(Target);
		}
	}

	StoreTargets();
	emit TargetsChanged();
}

bool CCluster::UpdateTarget(const QString& OldName, const QString& NewName, const QString& Address)
{
	{
		QWriteLocker Locker(&m_Mutex);

		const int Index = FindTarget(OldName);
		if (Index == -1)
			return false;

		//
		// A name is the key everything else finds this entry by, so two machines
		// cannot share one. Checked against the others rather than against all,
		// or renaming a machine to what it is already called would refuse.
		//
		if (NewName.compare(OldName, Qt::CaseInsensitive) != 0)
		{
			for (int i = 0; i < m_Targets.count(); i++)
			{
				if (i != Index && m_Targets[i].Name.compare(NewName, Qt::CaseInsensitive) == 0)
					return false;
			}
		}

		m_Targets[Index].Name = NewName;
		m_Targets[Index].Address = Address;

		//
		// A machine that was found by discovery and has now been edited is one
		// somebody has chosen to keep - StoreTargets writes only saved ones.
		//
		if (m_Targets[Index].State == STarget::eDiscovered)
			m_Targets[Index].State = STarget::eSaved;
	}

	StoreTargets();
	emit TargetsChanged();
	return true;
}

void CCluster::RemoveTarget(const QString& Name)
{
	Disconnect(Name);

	{
		QWriteLocker Locker(&m_Mutex);
		int Index = FindTarget(Name);
		if (Index == -1)
			return;
		m_Targets.removeAt(Index);
	}

	StoreTargets();
	emit TargetsChanged();
}

//
// ---- discovery ----
//

bool CCluster::IsDiscovering() const
{
	return m_pDiscovery != NULL;
}

bool CCluster::StartDiscovery(QString* pError)
{
	if (m_pDiscovery)
		return true;

	//
	// From the module, which owns both halves of this: the socket and the
	// understanding of what arrives on it. A build without TaskRemote has
	// nothing to discover for - a row in the list could not be acted on - so it
	// does not listen at all rather than listening and greying the results.
	//
	m_pDiscovery = CRemoteLoader::CreateDiscovery();
	if (!m_pDiscovery)
	{
		if (pError)
			*pError = CRemoteLoader::GetError();
		return false;
	}

	connect(m_pDiscovery, SIGNAL(Found(const QString&, const QString&, const QString&)),
		this, SLOT(OnDiscovered(const QString&, const QString&, const QString&)));

	if (!m_pDiscovery->Start(pError))
	{
		CRemoteLoader::DestroyDiscovery(m_pDiscovery);
		m_pDiscovery = NULL;
		return false;
	}
	return true;
}

void CCluster::StopDiscovery()
{
	if (!m_pDiscovery)
		return;

	CRemoteLoader::DestroyDiscovery(m_pDiscovery);
	m_pDiscovery = NULL;

	//
	// The suggestions go with it. They were never saved and were only ever true
	// while something was listening; leaving them behind would leave a list that
	// looks current and is not being maintained by anything.
	//
	{
		QWriteLocker Locker(&m_Mutex);
		for (int i = m_Targets.count() - 1; i >= 0; i--)
		{
			if (m_Targets[i].State == STarget::eDiscovered)
				m_Targets.removeAt(i);
		}
	}
	emit TargetsChanged();
}

void CCluster::RefreshDiscovery()
{
	if (m_pDiscovery)
		m_pDiscovery->Query();
}

//
// Something announced itself, already taken apart.
//
// The packet, the fields it carries and the address it came from are the
// module's business - see CWireDiscovery::OnFound. What is left here is what to
// do about it, which is a question about the target list and belongs to the
// cluster.
//
// All three are claims and are treated as such throughout: they are
// unauthenticated, anybody on the segment can send them, and the only thing
// they are allowed to do is put a row in a list that a person then has to act
// on.
//
void CCluster::OnDiscovered(const QString& Address, const QString& HostName, const QString& MachineId)
{
	bool bChanged = false;
	{
		QWriteLocker Locker(&m_Mutex);

		//
		// A machine already known by its id is not a discovery, whatever address
		// it announced from. Adding a second row for one the user has already
		// named is how a list becomes noise.
		//
		for (int i = 0; i < m_Targets.count(); i++)
		{
			if (!MachineId.isEmpty() && m_Targets[i].MachineId == MachineId)
				return;
			if (m_Targets[i].Address == Address)
				return;
		}

		STarget Target;

		//
		// Named for what it says it is, with the address in view. The name is
		// only a label here - nothing is saved under it unless somebody connects,
		// and at that point they have seen it.
		//
		Target.Name = HostName.isEmpty() ? Address : QString("%1 (%2)").arg(HostName).arg(Address);
		Target.Address = Address;
		Target.MachineId = MachineId;
		Target.State = STarget::eDiscovered;
		m_Targets.append(Target);
		bChanged = true;
	}

	if (bChanged)
		emit TargetsChanged();
}

void CCluster::ConnectSavedTargets()
{
	//
	// Nothing to connect with until the store is open. Not an error and not
	// worth reporting: the store asks for its password when the user first
	// wants it, and until then a saved machine simply waits.
	//
	CCredentialStore* pStore = CCredentialStore::Instance();
	if (!pStore->IsUnlocked())
		return;

	//
	// Collected under the lock and connected outside it. Connect() takes the
	// same lock, and it also blocks for the length of a handshake - neither of
	// which should happen with this held.
	//
	QList<QPair<QString, QString> > Pending;
	{
		QReadLocker Locker(&m_Mutex);
		foreach(const STarget& Target, m_Targets)
		{
			if (Target.State != STarget::eSaved || !Target.pSystem.isNull())
				continue;
			if (Target.bLocalDaemon)
				continue;	// found rather than configured; ConnectLocalDaemon has it

			Pending.append(qMakePair(Target.Name, Target.Address));
		}
	}

	//
	// The keys are read here, on the caller's thread, and handed to the worker
	// with the addresses. The store is one object shared by everything that
	// wants a key and it may still ask for a password; neither belongs on a
	// background thread.
	//
	QList<QPair<QString, SCredentials> > Work;
	QStringList Names;
	for (int i = 0; i < Pending.count(); i++)
	{
		SCredentials Cred;
		if (!pStore->Get(Pending[i].first, &Cred))
			continue;	// no key was kept for this one
		Work.append(qMakePair(Pending[i].second, Cred));
		Names.append(Pending[i].first);
	}

	if (Work.isEmpty())
		return;

	//
	// And the handshakes themselves off this thread.
	//
	// Every one of them is a round trip that waits up to five seconds, and they
	// used to run in a row before the window was shown - so one machine that was
	// switched off held up the whole program, including the local machine that
	// needed none of it. What the interface waits for now is nothing: the
	// branches appear as each connection lands, through the signals Connect
	// already emits, which Qt delivers on this thread because that is where the
	// receivers live.
	//
	// bTakeView false: a machine coming back on its own must not move the
	// window. Somebody who wants to look at it selects it.
	//
	m_ConnectFuture = QtConcurrent::run([this, Names, Work]()
	{
		int Connected = 0;

		for (int i = 0; i < Work.count(); i++)
		{
			//
			// Said before the attempt rather than after it, because the attempt
			// is the part that takes the time - up to five seconds against a
			// machine that is switched off.
			//
			emit ConnectingTo(Names[i], i, Work.count());

			//
			// eRequireSaved: a machine that answers with a different id than the
			// one this entry was saved for is not this machine, and reconnecting
			// to it unattended is exactly the case that check exists for.
			//
			if (!Connect(Names[i], Work[i].first, NULL, Work[i].second, eRequireSaved, false).isNull())
				Connected++;
		}

		emit ConnectRunFinished(Connected, Work.count(), Work.count() == 1 ? Names[0] : QString());
	});
}

CSystemPtr CCluster::Connect(const QString& Name, const QString& Address, STATUS* pStatus,
                             const SCredentials& Cred, EIdentityPolicy Identity, bool bTakeView)
{
	//
	// Added first, so that a machine that fails to answer still appears in the
	// list with its error rather than vanishing - which is the difference
	// between "the build box is down" and "the build box was never configured".
	//
	AddTarget(Name, Address);

	{
		QWriteLocker Locker(&m_Mutex);
		int Index = FindTarget(Name);
		if (Index != -1) {
			if (!m_Targets[Index].pSystem.isNull())
				return m_Targets[Index].pSystem;	// already up
			m_Targets[Index].State = STarget::eConnecting;
		}
	}
	emit TargetsChanged();

	//
	// What this entry has been answering to so far, if anything. Read before the
	// attempt so the connection can refuse itself rather than being unpicked
	// afterwards.
	//
	QString Expected;
	if (Identity == eRequireSaved)
	{
		QReadLocker Locker(&m_Mutex);
		int Index = FindTarget(Name);
		if (Index != -1)
			Expected = m_Targets[Index].MachineId;
	}

	//
	// From the module, if there is one. A build shipped without TaskRemote
	// reaches here only if something offered the option anyway, so it is worth
	// a status rather than a null nobody explains.
	//
	CSystemAPI* pNew = CRemoteLoader::Create();
	if (!pNew)
	{
		//
		// Two reasons for a null, and they are not the same thing to tell
		// somebody: there is no module, or the module is there and will not do
		// this without a supporter certificate. The module is asked which,
		// because it is the one that refused.
		//
		if (pStatus)
			*pStatus = ERR(CRemoteLoader::IsAvailable() ? TE_NoCertificate : TE_NoRemoteModule);
		return CSystemPtr();
	}

	IRemoteSystem* pRemote = pNew->GetRemote();
	pRemote->SetExpectedMachineId(Expected);

	STATUS Status = pRemote->Connect(Address, 5000, Cred);

	if (pStatus)
		*pStatus = Status;

	QWriteLocker Locker(&m_Mutex);
	int Index = FindTarget(Name);
	if (Index == -1) {
		CRemoteLoader::Destroy(pNew);
		return CSystemPtr();
	}

	if (Status.IsError())
	{
		m_Targets[Index].State = STarget::eFailed;
		m_Targets[Index].LastError = (quint32)Status.GetMsgCode();
		CRemoteLoader::Destroy(pNew);
		Locker.unlock();
		emit TargetsChanged();
		return CSystemPtr();
	}

	//
	// Given back to the module that made it, rather than deleted here. See
	// CRemoteLoader::Destroy: it still ends in deleteLater, but which module
	// runs that is now the answer to a question rather than an accident.
	//
	CSystemPtr pSystem = CSystemPtr(pNew, &CRemoteLoader::Destroy);

	//
	// The one thing nobody was listening for. A server that dies used to leave
	// its branch in the tree, with its processes, claiming to be connected,
	// for ever.
	//
	connect(pNew, SIGNAL(ConnectionLost()), this, SLOT(OnConnectionLost()));
	connect(pNew, SIGNAL(Reconnected()), this, SLOT(OnReconnected()));
	connect(pNew, SIGNAL(ReconnectRefused(quint32)), this, SLOT(OnReconnectRefused(quint32)));

	m_Targets[Index].State = STarget::eConnected;
	m_Targets[Index].LastError = 0;
	m_Targets[Index].pSystem = pSystem;

	//
	// Recorded, and by now it is known to be the right one: the handshake
	// refused anything else - see CRemoteSystem::SetExpectedMachineId. This
	// writes it for the first connection, which has nothing to compare against,
	// and for the one the user has just looked at and accepted.
	//
	m_Targets[Index].MachineId = pRemote->GetMachineId();

	//
	// From here on the object retries on its own, and it has to keep refusing
	// what this call refused - a reconnection is the same question asked again
	// with nobody watching.
	//
	pRemote->SetExpectedMachineId(m_Targets[Index].MachineId);

	if (!m_Nodes.contains(pSystem))
		m_Nodes.append(pSystem);

	//
	// Without the machine layer the window shows one machine, and connecting to
	// one is a request to look at it - there is nowhere else it could appear.
	// With the layer on it is only a new branch and the selection is left
	// alone.
	//
	bool bNowInView = false;
	if (!m_bClusterMode && bTakeView) {
		m_ViewSystem = pSystem;
		bNowInView = true;
	}

	Locker.unlock();
	StoreTargets();
	emit NodeAdded(pSystem);
	if (bNowInView)
		emit ViewSystemChanged();
	emit TargetsChanged();
	return pSystem;
}

void CCluster::Disconnect(const QString& Name)
{
	CSystemPtr pSystem;
	bool bWasInView = false;
	bool bWasGraphed = false;
	{
		QWriteLocker Locker(&m_Mutex);
		int Index = FindTarget(Name);
		if (Index == -1 || m_Targets[Index].pSystem.isNull())
			return;

		pSystem = m_Targets[Index].pSystem;
		m_Targets[Index].pSystem.clear();
		m_Targets[Index].State = STarget::eSaved;
		m_Nodes.removeOne(pSystem);

		//
		// Neither the panels nor the graphs can go on showing a machine that is
		// no longer connected, so both fall back to the local one.
		//
		if (m_ViewSystem == pSystem) {
			m_ViewSystem.clear();
			bWasInView = true;
		}
		if (m_GraphSystem == pSystem) {
			m_GraphSystem.clear();
			bWasGraphed = true;
		}
	}

	//
	// Closed here rather than left to the destructor: the socket lives in the
	// system's own thread and has to be shut down while that thread is still
	// running, or the last reference is dropped into a race - see TaskConsole.
	//
	if (IRemoteSystem* pRemote = pSystem->GetRemote())
		pRemote->Disconnect();

	emit NodeRemoved(pSystem);
	if (bWasInView)
		emit ViewSystemChanged();
	if (bWasGraphed)
		emit ActiveSystemChanged();
	emit TargetsChanged();
}

void CCluster::LoadTargets()
{
	QWriteLocker Locker(&m_Mutex);

	m_Targets.clear();
	foreach(const QVariant& vTarget, CXml::Read(theConf->GetConfigDir() + "/Targets.xml").toList())
	{
		QVariantMap Map = vTarget.toMap();
		if (Map["Name"].toString().isEmpty())
			continue;

		STarget Target;
		Target.Name = Map["Name"].toString();
		Target.Address = Map["Address"].toString();
		Target.MachineId = Map["MachineId"].toString();
		m_Targets.append(Target);
	}
}

void CCluster::StoreTargets()
{
	QReadLocker Locker(&m_Mutex);

	QVariantList Targets;
	foreach(const STarget& Target, m_Targets)
	{
		//
		// A discovered machine is not written down: it was not configured by
		// anybody, and it will announce itself again or it will not. Nor is this
		// machine's own daemon, which is found at startup rather than
		// configured.
		//
		if (Target.State == STarget::eDiscovered || Target.bLocalDaemon)
			continue;

		QVariantMap Map;
		Map["Name"] = Target.Name;
		Map["Address"] = Target.Address;
		Map["MachineId"] = Target.MachineId;
		Targets.append(Map);
	}

	CXml::Write(Targets, theConf->GetConfigDir() + "/Targets.xml");
}


bool CCluster::GetTargetInfo(CSystemAPI* pSystem, QString* pName, QString* pAddress) const
{
	QReadLocker Locker(&m_Mutex);

	foreach(const STarget& Target, m_Targets)
	{
		if (Target.pSystem.data() != pSystem)
			continue;
		if (pName)
			*pName = Target.Name;
		if (pAddress)
			*pAddress = Target.Address;
		return true;
	}
	return false;
}

QString CCluster::GetTargetAddress(CSystemAPI* pSystem) const
{
	QReadLocker Locker(&m_Mutex);

	foreach(const STarget& Target, m_Targets)
	{
		if (Target.pSystem.data() == pSystem)
			return Target.Address;
	}
	return QString();
}
