#include "stdafx.h"
#include "../TaskStrings.h"
#include "../TaskExplorer.h"
#include "ProcessModel.h"
#include "../../../MiscHelpers/Common/Common.h"
#include "../../API/SystemAPI.h"
#include "../../API/Cluster.h"
#include "../../API/Monitors/GpuMonitor.h"


CProcessModel::CProcessModel(QObject *parent)
:CTreeItemModel(parent)
{
	m_bMultiUser = false;
	m_bMultiMachine = false;
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
	m_iUseDescr = 1;

	for (int i = 0; i < columnCount(); i++)
	{
		if(i == eProcess)
			continue;
		m_ColumnsOff.insert(i);
	}
}

CProcessModel::~CProcessModel()
{
}

//
// A user branch is identified by a string and a process node by a number, so
// the two can share one id space without ever meeting. The prefix is there to
// make the id legible in a debugger rather than to keep it apart.
//
//
// A machine's branch, identified by the system object rather than by its name.
//
// Two machines can report the same host name - cloned VMs, a workgroup, half a
// network called DESKTOP-XXXX - so a name cannot be the key. The system pointer
// is unique for as long as the connection is, which is exactly as long as the
// branch exists.
//
QVariant CProcessModel::MkMachineBranch(CSystemAPI* pSystem)
{
	return QVariant(QString("machine:%1").arg((quintptr)pSystem));
}

bool CProcessModel::IsMachineBranch(const QVariant& Id)
{
	return Id.typeId() == QMetaType::QString && Id.toString().startsWith("machine:");
}

void CProcessModel::SetMultiMachine(bool bSet)
{
	if (m_bMultiMachine == bSet)
		return;
	m_bMultiMachine = bSet;
	Clear();
}

//
// The account branch, scoped to the machine it belongs to.
//
// The scope is not cosmetic. CTreeItemModel::Fill looks a branch up in the
// model's *global* m_Map by its id, not among the children of the parent it is
// being placed under - so every branch id has to be unique across the whole
// tree, not just among its siblings. A bare "user:S-1-5-18" is the same string
// on every machine, so the second machine's branch resolved to the first
// machine's node and every one of its processes was hung there. The machine
// branch then had no children of its own and the next Purge removed it, which
// is exactly the "appears for a second and then disappears" that was reported.
//
// Only scoped when the machine level actually exists. Without it there is one
// tree for everything in view and one branch per account is the right answer;
// splitting them by machine would show the same account twice with nothing on
// screen saying why.
//
//
// What grouping by account groups by, which is not the same question on both
// platforms.
//
// Linux has one account per person and processes belong to it, so the account
// is the answer. Windows has accounts that exist to hold one process: a session
// brings up its own DWM-<n> and UMFD-<n>, each in a service account of its own,
// so grouping by account gave a branch per pseudo-account with a single row
// under it - three or four of them per logged-in person, between the branches
// somebody actually wanted to look at.
//
// So on Windows the group is the logon session, which is what those accounts
// belong to and what a reader means by "this person's processes". The branch
// is named after whoever owns the session - see SessionUserName.
//
// Decided by the machine being watched rather than by the one watching: a
// Windows viewer showing a Linux daemon groups that machine's processes by
// account, as it should.
//
QString CProcessModel::GetGroupKey(const CProcessPtr& pProcess)
{
	const CSystemPtr pSystem = pProcess->GetSystem();
	if (!pSystem.isNull() && pSystem->GetOsType() == CSystemAPI::eOsWindows)
		return QString("session:%1").arg(pProcess->GetSessionID());
	return pProcess->GetUserKey();
}

//
// What a group's branch is called.
//
// A session that nobody owns still needs a heading, and "session:0" is the
// key rather than a name.
//
// Session 0 is not merely an unnamed session, it is a particular one: since
// Vista it is where the services run and no one can log into it. So it is
// named for what it holds rather than numbered - the same word the S-1-5-18
// account is given, which is the account most of what is in it runs as.
//
// Any other session without an owner keeps its number. That is a session that
// exists but has nobody in it, and there is nothing truthful to call it.
//
QString CProcessModel::GroupDisplayName(const QString& Key, const QString& Resolved)
{
	if (Key.startsWith("session:"))
	{
		if (!Resolved.isEmpty())
			return Resolved;

		const QString Session = Key.mid(8);
		if (Session == "0")
			return tr("System");
		return tr("Session %1").arg(Session);
	}
	return ::GetUserDisplayName(Key, Resolved);
}

QVariant CProcessModel::MkUserBranch(const QString& UserKey, CSystemAPI* pSystem) const
{
	return QVariant(QString("user:%1:%2").arg(m_bMultiMachine ? (quintptr)pSystem : 0).arg(UserKey));
}

bool CProcessModel::IsUserBranch(const QVariant& Id, QString* pUserKey)
{
	if (Id.typeId() != QMetaType::QString)
		return false;
	const QString Str = Id.toString();
	if (!Str.startsWith("user:"))
		return false;

	//
	// Everything after the scope. Taken from the first separator rather than
	// split, because an account key is opaque here and may hold one itself.
	//
	const int Sep = Str.indexOf(QLatin1Char(':'), 5);
	if (Sep == -1)
		return false;
	if (pUserKey)
		*pUserKey = Str.mid(Sep + 1);
	return true;
}

void CProcessModel::SetMultiUser(bool bSet)
{
	if (m_bMultiUser == bSet)
		return;
	m_bMultiUser = bSet;

	//
	// Every path changes, so nothing that is currently in the tree is in the
	// right place. Clearing is cheaper and more obviously correct than trying
	// to move a few hundred nodes, and the next refresh rebuilds it.
	//
	Clear();
}

//
// Whether a child stays with its parent.
//
// In multi-user mode a process only continues its parent's branch while it runs
// as the same account; the moment it does not it belongs to the other account's
// branch and starts a new chain there.
//
static bool SameUser(const CProcessPtr& pProcess, const CProcessPtr& pParent)
{
	return CProcessModel::GetGroupKey(pProcess) == CProcessModel::GetGroupKey(pParent);
}

//
// Whether the two were seen on the same machine - and it has to be asked before
// anything else touches the parent.
//
// The process map handed to the model is the union over every connected
// machine, keyed by SProcessUID, which is a pid and a creation time. That pair
// is unique on one machine and emphatically not across several: a local process
// looking up its parent can find a *remote* process that happens to share it.
// The lookup then hands a CRemoteProcess to CWinProcess::ValidateParent, which
// casts it to its own type and dereferences a member it does not have. That is
// a null dereference in ValidateParent, and it is what happens within a second
// of connecting a second machine.
//
static bool SameSystem(const CProcessPtr& pProcess, const CProcessPtr& pParent)
{
	return pProcess->GetSystem() == pParent->GetSystem();
}

QSet<quint32> CProcessModel::GetWantedFields() const
{
	QSet<quint32> Fields;

	//
	// One entry per column that is drawn from a field the wire carries. The
	// mandatory core - pid, parent, name, user key, uid, creation time - is not
	// here: the server sends it whatever is asked for, because the tree, the
	// grouping and the highlight are built from it and no column controls them.
	//
	static const struct { int Column; quint32 Field; } Map[] = {
		{ eUserName,	CSystemAPI::eFieldUser		},
		{ eCommandLine,	CSystemAPI::eFieldCmdLine	},
		{ eFileName,	CSystemAPI::eFieldPath		},
		{ eSessionID,	CSystemAPI::eFieldSession	},
		{ eThreads,		CSystemAPI::eFieldThreads	},
		{ eHandles,		CSystemAPI::eFieldHandles	},
		{ eWorkingSet,	CSystemAPI::eFieldWorkingSet	},

		//
		// The counter blocks. Several columns map to the same one, which is the
		// point of them being blocks: asking for CPU time gets the kernel and
		// user split, the cycles and the context switches together, because they
		// are read from the same counters and change on the same tick.
		//
		{ eCPU,					CSystemAPI::eFieldCpuTime	},
		{ eCPU_History,			CSystemAPI::eFieldCpuTime	},
		{ eTotalCPU_Time,		CSystemAPI::eFieldCpuTime	},
		{ eKernelCPU_Time,		CSystemAPI::eFieldCpuTime	},
		{ eUserCPU_Time,		CSystemAPI::eFieldCpuTime	},
		{ eCycles,				CSystemAPI::eFieldCpuTime	},
		{ eCyclesDelta,			CSystemAPI::eFieldCpuTime	},
		{ eContextSwitches,		CSystemAPI::eFieldCpuTime	},
		{ eContextSwitchesDelta,CSystemAPI::eFieldCpuTime	},

		{ eIO_TotalRate,		CSystemAPI::eFieldIoStats	},
		{ eIO_History,			CSystemAPI::eFieldIoStats	},
		{ eIO_Reads,			CSystemAPI::eFieldIoStats	},
		{ eIO_Writes,			CSystemAPI::eFieldIoStats	},
		{ eIO_Other,			CSystemAPI::eFieldIoStats	},
		{ eIO_ReadBytes,		CSystemAPI::eFieldIoStats	},
		{ eIO_WriteBytes,		CSystemAPI::eFieldIoStats	},
		{ eIO_OtherBytes,		CSystemAPI::eFieldIoStats	},

		{ eDisk_TotalRate,		CSystemAPI::eFieldDiskNet	},
		{ eReadBytes,			CSystemAPI::eFieldDiskNet	},
		{ eWriteBytes,			CSystemAPI::eFieldDiskNet	},
		{ eNet_TotalRate,		CSystemAPI::eFieldDiskNet	},
		{ eNetUsage,			CSystemAPI::eFieldDiskNet	},
		{ eNET_History,			CSystemAPI::eFieldDiskNet	},
		{ eReceiveBytes,		CSystemAPI::eFieldDiskNet	},
		{ eSendBytes,			CSystemAPI::eFieldDiskNet	},

		{ ePrivateBytes,		CSystemAPI::eFieldMemory		},
		{ ePeakPrivateBytes,	CSystemAPI::eFieldMemory		},
		{ eVirtualSize,			CSystemAPI::eFieldMemory		},
		{ ePeakVirtualSize,		CSystemAPI::eFieldMemory		},
		{ ePeakWS,				CSystemAPI::eFieldMemory		},
		{ ePrivateWS,			CSystemAPI::eFieldMemory		},
		{ eMEM_History,			CSystemAPI::eFieldMemory		},
		{ ePageFaults,			CSystemAPI::eFieldMemory		},
		{ ePagedPool,			CSystemAPI::eFieldMemory		},
		{ ePeakPagedPool,		CSystemAPI::eFieldMemory		},
		{ eNonPagedPool,		CSystemAPI::eFieldMemory		},
		{ ePeakNonPagedPool,	CSystemAPI::eFieldMemory		},

		{ ePeakThreads,			CSystemAPI::eFieldCounts		},
		{ eGDI_Handles,			CSystemAPI::eFieldCounts		},
		{ eUSER_Handles,		CSystemAPI::eFieldCounts		},
		{ eWND_Handles,			CSystemAPI::eFieldCounts		},

		{ ePriorityClass,		CSystemAPI::eFieldPriority	},
		{ eBasePriority,		CSystemAPI::eFieldPriority	},
		{ ePagePriority,		CSystemAPI::eFieldPriority	},
		{ eIO_Priority,			CSystemAPI::eFieldPriority	},
		{ eAffinity,			CSystemAPI::eFieldPriority	},

		{ eCritical,			CSystemAPI::eFieldFlags		},

		{ eJobObjectID,			CSystemAPI::eFieldMisc		},
		{ eDPI_Awareness,		CSystemAPI::eFieldMisc		},
		{ eErrorMode,			CSystemAPI::eFieldMisc		},
		{ eCodePage,			CSystemAPI::eFieldMisc		},
		{ eReferences,			CSystemAPI::eFieldMisc		},
		{ eTLS,					CSystemAPI::eFieldMisc		},
		{ eHangCount,			CSystemAPI::eFieldMisc		},
		{ eGhostCount,			CSystemAPI::eFieldMisc		},
		{ eSuspendedTime,		CSystemAPI::eFieldMisc		},
		{ eConsolePID,			CSystemAPI::eFieldMisc		},
		{ eSequenceNumber,		CSystemAPI::eFieldMisc		},
		{ eStartKey,			CSystemAPI::eFieldMisc		},
		{ eShareableCommit,		CSystemAPI::eFieldMisc		},
		{ eNetUsage,			CSystemAPI::eFieldMisc		},
		{ eWindowTitle,			CSystemAPI::eFieldMisc		},
		{ eWindowStatus,		CSystemAPI::eFieldMisc		},
		{ eServices,			CSystemAPI::eFieldMisc		},
		{ eOomScore,			CSystemAPI::eFieldMisc		},
		{ eOomScoreAdj,			CSystemAPI::eFieldMisc		},
		{ ePID_LXSS,			CSystemAPI::eFieldMisc		},
		{ eConfinement,			CSystemAPI::eFieldMisc		},
		{ eContainer,			CSystemAPI::eFieldMisc		},

		//
		// The Status column, and the two ways it is answered - a Windows flag
		// set or a Linux run state. Both travel under one request because the
		// viewer does not know which kind of machine it is looking at until it
		// has connected, and by then the projection is already fixed.
		//
		{ eStatus,				CSystemAPI::eFieldStatus		},

		{ eIntegrity,			CSystemAPI::eFieldToken		},

		{ eMitigations,			CSystemAPI::eFieldMitigations },

		{ eGPU_Usage,			CSystemAPI::eFieldGpu		},
		{ eGPU_History,			CSystemAPI::eFieldGpu		},
		{ eVMEM_History,		CSystemAPI::eFieldGpu		},
		{ eGPU_Shared,			CSystemAPI::eFieldGpu		},
		{ eGPU_Dedicated,		CSystemAPI::eFieldGpu		},
		{ eGPU_Adapter,			CSystemAPI::eFieldGpu		},
		{ eElevation,			CSystemAPI::eFieldToken		},
		{ eVirtualized,			CSystemAPI::eFieldToken		},
		{ eConfinement,			CSystemAPI::eFieldStatus		},
	};

	for (int i = 0; i < sizeof(Map) / sizeof(Map[0]); i++)
	{
		if (!m_ColumnsOff.contains(Map[i].Column))
			Fields.insert(Map[i].Field);
	}

	//
	// Two the tooltip reads for any row, whatever is shown: the command line and
	// the image path. They are also the two most expensive to produce, which is
	// precisely what the projection is for - so this is a debt, not a decision.
	// It goes when the tooltip is served by a per-process detail request rather
	// than from the list; see NEXT.md 5.3.
	//
	Fields.insert(CSystemAPI::eFieldPath);
	Fields.insert(CSystemAPI::eFieldCmdLine);

	//
	// And the image info, whatever columns are on: the row icon comes out of it,
	// and an icon is not a column anybody can switch off. The bytes are sent
	// once per image per connection - see MakeFileInfoBlock - so what this costs
	// every round is a description and a version string that do not change.
	//
	Fields.insert(CSystemAPI::eFieldFileInfo);

	//
	// And the session while grouping by account is on, because on a Windows
	// target that is what the grouping is by - see GetGroupKey. Without it a
	// remote machine answers zero for every process and they all land in one
	// branch called "Session 0", which is not a subtle failure but is a silent
	// one: the column that would have shown the truth is off, which is exactly
	// the case this covers.
	//
	if (m_bMultiUser)
		Fields.insert(CSystemAPI::eFieldSession);

	return Fields;
}

QList<QVariant> CProcessModel::MakePath(const CProcessPtr& pProcess, const QMap<SProcessUID, CProcessPtr>& ProcessList)
{
	QList<QVariant> Path;

	//
	// Outermost, so that everything below it - accounts, the process tree -
	// belongs to one machine and cannot be confused with another's.
	//
	if (m_bMultiMachine)
		Path.append(MkMachineBranch(pProcess->GetSystem().data()));

	if (m_bTree)
		Path += MakeProcPath(pProcess, ProcessList);
	else if (m_bMultiUser)
	{
		//
		// List mode: no nesting, but the account branch stays. It is the whole
		// point of the switches being separate - "flat" means flat *within*
		// each account, not one flat list again.
		//
		Path.append(MkUserBranch(GetGroupKey(pProcess), pProcess->GetSystem().data()));
	}

	return Path;
}

bool CProcessModel::TestPath(const QList<QVariant>& Path, const CProcessPtr& pProcess, const QMap<SProcessUID, CProcessPtr>& ProcessList)
{
	//
	// Cheaper than rebuilding the whole path and comparing: this is called for
	// every process on every refresh.
	//
	if (m_bMultiMachine)
	{
		if (Path.isEmpty() || Path.first() != MkMachineBranch(pProcess->GetSystem().data()))
			return false;
	}

	const int Head = m_bMultiMachine ? 1 : 0;

	if (m_bTree)
		return TestProcPath(Path, pProcess, ProcessList, 0, Head);

	if (m_bMultiUser)
		return Path.size() == Head + 1 && Path.last() == MkUserBranch(GetGroupKey(pProcess), pProcess->GetSystem().data());

	return Path.size() == Head;
}

QList<QVariant> CProcessModel::MakeProcPath(const CProcessPtr& pProcess, const QMap<SProcessUID, CProcessPtr>& ProcessList)
{
	QList<QVariant> Path;

	SProcessUID ParentUID = pProcess->GetParentUId();
	CProcessPtr pParent = ProcessList.value(ParentUID);

	if (!pParent.isNull() && SameSystem(pProcess, pParent) && pProcess->ValidateParent(pParent.data())
		&& (!m_bMultiUser || SameUser(pProcess, pParent)))
	{
		Path = MakeProcPath(pParent, ProcessList);

		//
		// The parent's *node* id, which is its object uid - see below. Not its
		// SProcessUID, which is only unique on one machine.
		//
		Path.append(pParent->GetObjectUid());
		return Path;
	}

	//
	// The top of a chain. Without grouping that is the root of the tree; with
	// it, the account's branch.
	//
	if (m_bMultiUser)
		Path.append(MkUserBranch(GetGroupKey(pProcess), pProcess->GetSystem().data()));

	return Path;
}

bool CProcessModel::TestProcPath(const QList<QVariant>& Path, const CProcessPtr& pProcess, const QMap<SProcessUID, CProcessPtr>& ProcessList, int Index, int Head)
{
	SProcessUID ParentUID = pProcess->GetParentUId();
	CProcessPtr pParent = ProcessList.value(ParentUID);

	//
	// Mirrors MakeProcPath exactly - if the two ever disagree, a node is
	// rebuilt on every refresh or never rebuilt when it should be.
	//
	if (!pParent.isNull() && SameSystem(pProcess, pParent) && pProcess->ValidateParent(pParent.data())
		&& (!m_bMultiUser || SameUser(pProcess, pParent)))
	{
		if (Index >= Path.size() || Path[Path.size() - Index - 1] != QVariant(pParent->GetObjectUid()))
			return false;

		return TestProcPath(Path, pParent, ProcessList, Index + 1, Head);
	}

	if (m_bMultiUser)
	{
		//
		// One element left, and it has to be this process's account - a process
		// that changed hands, which nothing does, would otherwise keep hanging
		// under the old one.
		//
		return Path.size() == Head + Index + 1
			&& Path[Head] == MkUserBranch(GetGroupKey(pProcess), pProcess->GetSystem().data());
	}

	return Path.size() == Head + Index;
}

//
// Which columns a branch may add up.
//
// A branch is not a process, so most columns have no answer for one at all:
// there is no priority for an account, no command line for a machine. What is
// left is the counters, and those add - the total is the sum of the parts,
// which is the whole point of a row that stands over a group.
//
// Two deliberate omissions. Peaks are not here, because the sum of each
// process's peak is not the peak of the sum and would be read as one. Neither
// are the columns that report nothing but *shared* memory: working set is
// summed the way every task manager sums it, knowing a shared page is counted
// once per process holding it, but in a column that is only shared pages that
// double counting would be the entire number.
//
// The graph columns are absent too, for a mechanical reason rather than a
// principled one - their cells are drawn by an index widget keyed on a process
// id, which a branch does not have, so a value written there would never be
// looked at.
//
bool CProcessModel::IsSummableColumn(int section)
{
	switch (section)
	{
		// ---- cpu ----
		case eCPU:
		case eTotalCPU_Time:
		case eKernelCPU_Time:
		case eUserCPU_Time:
		case eContextSwitches:
		case eContextSwitchesDelta:
		case eCycles:
		case eCyclesDelta:

		// ---- memory ----
		case ePrivateBytes:
		case ePrivateBytesDelta:
		case eWorkingSet:
		case ePrivateWS:
		case eVirtualSize:
		case ePageFaults:
		case ePageFaultsDelta:
		case eHardFaults:
		case eHardFaultsDelta:
		case ePagedPool:
		case eNonPagedPool:

		// ---- disk ----
		case eDisk_TotalRate:
		case eReads:
		case eWrites:
		case eReadBytes:
		case eWriteBytes:
		case eReadsDelta:
		case eWritesDelta:
		case eReadBytesDelta:
		case eWriteBytesDelta:
		case eReadRate:
		case eWriteRate:

		// ---- network ----
		case eNet_TotalRate:
		case eReceives:
		case eSends:
		case eReceiveBytes:
		case eSendBytes:
		case eReceivesDelta:
		case eSendsDelta:
		case eReceiveBytesDelta:
		case eSendBytesDelta:
		case eReceiveRate:
		case eSendRate:

		// ---- file i/o ----
		case eIO_TotalRate:
		case eIO_Reads:
		case eIO_Writes:
		case eIO_Other:
		case eIO_ReadBytes:
		case eIO_WriteBytes:
		case eIO_OtherBytes:
		case eIO_ReadsDelta:
		case eIO_WritesDelta:
		case eIO_OtherDelta:
		case eIO_ReadBytesDelta:
		case eIO_WriteBytesDelta:
		case eIO_OtherBytesDelta:
		case eIO_ReadRate:
		case eIO_WriteRate:
		case eIO_OtherRate:

		// ---- gpu ----
		case eGPU_Usage:
		case eGPU_Shared:
		case eGPU_Dedicated:

		// ---- objects ----
		case eHandles:
		case eThreads:
		case eWND_Handles:
		case eGDI_Handles:
		case eUSER_Handles:

		case eDebugTotal:
			return true;
	}

	return false;
}

//
// A summed column, given back as the type the same column has on a process
// row - so that sorting compares numbers and the formatter recognises it.
//
static QVariant MkSum(int section, double Value)
{
	switch (section)
	{
		case CProcessModel::eCPU:
		case CProcessModel::eGPU_Usage:
			return Value;					// a fraction of one core-second, not a count
		case CProcessModel::ePrivateBytesDelta:
			return (qint64)Value;			// the only one of these that can be negative
	}

	return (quint64)Value;
}

//
// The idle process, which is not a process.
//
// Windows accounts the time no thread wanted to run to a pseudo-process: pid 0,
// one thread per cpu, owned by SYSTEM. Its cpu column is the machine's idle
// time, and on a quiet machine that is most of the machine - so an account
// branch that added its children up handed the SYSTEM branch eighty or ninety
// per cent and claimed the account that runs the services was doing all of it.
//
// Its neighbours are not affected by this: the Interrupts and DPCs rows the
// Windows collector invents carry interrupt and deferred-call time, which is
// work the machine really did and belongs in the total.
//
// Linux has no such row - the kernel's idle tasks are not processes there - and
// nothing in its list uses pid 0, so the test costs it nothing.
//
static bool IsIdleProcess(const CProcessPtr& pProcess)
{
	if (pProcess.isNull() || pProcess->GetProcessId() != 0)
		return false;

	const CSystemPtr pSystem = pProcess->GetSystem();
	return pSystem.isNull() || pSystem->GetOsType() == CSystemAPI::eOsWindows;
}

//
// Everything underneath a branch, added up.
//
// Read back out of the nodes rather than out of the processes: the column was
// worked out once already this round, so taking it from there means a branch
// can never disagree with the rows it stands over, and a column that is
// switched off costs nothing because nothing computed it in the first place.
//
// Virtual nodes contribute nothing of their own - an account branch inside a
// machine branch is a heading, not a row - so nothing is counted twice. Neither
// does the idle process, and for a related reason: it is a place to put a
// number, not something running. See IsIdleProcess.
//
// All of its columns rather than its cpu alone. What it reports elsewhere is of
// the same kind - context switches into the idle loop, one thread per cpu that
// exists to have somewhere to idle - and a branch that counted those while
// leaving the cpu out would be half-right in a way nobody could read off the
// screen. Its own row is untouched: the idle percentage is worth showing, it is
// simply not the account's.
//
void CProcessModel::SumSubtree(STreeNode* pNode, QVector<double>& Sums, QSet<int>& NotAvailable) const
{
	if (!pNode->Virtual && !IsIdleProcess(((SProcessNode*)pNode)->pProcess))
	{
		for (int i = 0; i < pNode->Values.size() && i < Sums.size(); i++)
		{
			if (!IsSummableColumn(i))
				continue;

			//
			// A string where a number should be is Sync's "N/A": without ETW
			// the network and disk columns have nothing behind them, and a
			// branch reporting zero would be claiming something the rows below
			// it explicitly do not.
			//
			const QVariant& Raw = pNode->Values[i].Raw;
			if (Raw.type() == QVariant::String)
				NotAvailable.insert(i);
			else
				Sums[i] += Raw.toDouble();
		}
	}

	foreach(STreeNode* pChild, pNode->Children)
		SumSubtree(pChild, Sums, NotAvailable);
}

//
// What the machine says about itself, which for these columns is a better
// answer than the sum rather than merely a different one.
//
// The system's own counters include what no process accounts for: the kernel's
// own time, the I/O of processes that started and ended between two refreshes,
// memory that belongs to nobody. A machine row built only out of its children
// would quietly under-report all of it.
//
// Everything here is already carried over the wire for remote machines - it is
// what the System Information panel draws - so a machine row reads the same
// whether it is this computer or one on the far side of a socket.
//
void CProcessModel::FillMachineColumns(CSystemAPI* pSystem, QVector<QVariant>& New) const
{
	auto Set = [&](int Column, const QVariant& Value) {
		if (Column < New.size() && !m_ColumnsOff.contains(Column))
			New[Column] = Value;
	};

	// ---- what the machine is ----
	Set(eDescription, pSystem->GetSystemName());
	Set(eVersion, pSystem->GetSystemVersion());
	Set(eUserName, ::LocalizeName(pSystem->GetUserName()));

	//
	// Uptime and boot time, in the units the process columns use for the same
	// two things: seconds for the one, milliseconds since the epoch for the
	// other. Only when there is an uptime at all, so that a machine which has
	// not answered yet does not claim to have booted in 1970.
	//
	const quint64 UpTime = pSystem->GetUpTime();
	if (UpTime)
	{
		Set(eUpTime, UpTime);
		Set(eStartTime, (quint64)(QDateTime::currentMSecsSinceEpoch() - (qint64)UpTime * 1000));
	}

	// ---- cpu ----
	Set(eCPU, (double)pSystem->GetCpuUsage());

	//
	// ---- memory ----
	//
	// The system's analogues, one for one: commit charge stands for private
	// bytes, physical memory in use for the working set, the commit limit for
	// the virtual size.
	//
	Set(ePrivateBytes, pSystem->GetCommitedMemory());
	Set(ePeakPrivateBytes, pSystem->GetCommitedMemoryPeak());
	Set(eWorkingSet, pSystem->GetPhysicalUsed());
	Set(eVirtualSize, pSystem->GetMemoryLimit());
	Set(ePagedPool, pSystem->GetPagedPool());
	Set(eNonPagedPool, pSystem->GetNonPagedPool());

	// ---- objects ----
	Set(eThreads, pSystem->GetTotalThreads());
	Set(eHandles, pSystem->GetTotalHandles());

	//
	// The GUI object totals, and only where there are any. The base class
	// reports zero for a machine that does not count them, and a zero written
	// over the sum of the rows below would be a worse answer than the sum.
	//
	if (quint32 Total = pSystem->GetTotalWndObjects())	Set(eWND_Handles, Total);
	if (quint32 Total = pSystem->GetTotalGuiObjects())	Set(eGDI_Handles, Total);
	if (quint32 Total = pSystem->GetTotalUserObjects())	Set(eUSER_Handles, Total);

	//
	// GPU memory, machine wide, from the same monitor the graphs read. Engine
	// usage is per adapter and has no single total, so that column keeps the
	// sum of the processes.
	//
	if (CGpuMonitor* pGpu = pSystem->GetGpuMonitor())
	{
		CGpuMonitor::SGpuMemory Memory = pGpu->GetGpuMemory();
		Set(eGPU_Dedicated, Memory.DedicatedUsage);
		Set(eGPU_Shared, Memory.SharedUsage);
	}

	//
	// ---- the three i/o families ----
	//
	// Counters, deltas and rates all come out of the one struct, and the rates
	// were worked out by the same SRateCounter the process rows use - on the
	// remote side, over the interval between the two merges that produced them.
	//
	SSysStats Stats = pSystem->GetStats();

	Set(eReads, Stats.Disk.ReadCount);
	Set(eWrites, Stats.Disk.WriteCount);
	Set(eReadBytes, Stats.Disk.ReadRaw);
	Set(eWriteBytes, Stats.Disk.WriteRaw);
	Set(eReadsDelta, Stats.Disk.ReadDelta.Delta);
	Set(eWritesDelta, Stats.Disk.WriteDelta.Delta);
	Set(eReadBytesDelta, Stats.Disk.ReadRawDelta.Delta);
	Set(eWriteBytesDelta, Stats.Disk.WriteRawDelta.Delta);
	Set(eReadRate, Stats.Disk.ReadRate.Get());
	Set(eWriteRate, Stats.Disk.WriteRate.Get());
	Set(eDisk_TotalRate, Stats.Disk.ReadRate.Get() + Stats.Disk.WriteRate.Get());

	Set(eReceives, Stats.Net.ReceiveCount);
	Set(eSends, Stats.Net.SendCount);
	Set(eReceiveBytes, Stats.Net.ReceiveRaw);
	Set(eSendBytes, Stats.Net.SendRaw);
	Set(eReceivesDelta, Stats.Net.ReceiveDelta.Delta);
	Set(eSendsDelta, Stats.Net.SendDelta.Delta);
	Set(eReceiveBytesDelta, Stats.Net.ReceiveRawDelta.Delta);
	Set(eSendBytesDelta, Stats.Net.SendRawDelta.Delta);
	Set(eReceiveRate, Stats.Net.ReceiveRate.Get());
	Set(eSendRate, Stats.Net.SendRate.Get());
	Set(eNet_TotalRate, Stats.Net.ReceiveRate.Get() + Stats.Net.SendRate.Get());

	Set(eIO_Reads, Stats.Io.ReadCount);
	Set(eIO_Writes, Stats.Io.WriteCount);
	Set(eIO_Other, Stats.Io.OtherCount);
	Set(eIO_ReadBytes, Stats.Io.ReadRaw);
	Set(eIO_WriteBytes, Stats.Io.WriteRaw);
	Set(eIO_OtherBytes, Stats.Io.OtherRaw);
	Set(eIO_ReadsDelta, Stats.Io.ReadDelta.Delta);
	Set(eIO_WritesDelta, Stats.Io.WriteDelta.Delta);
	Set(eIO_OtherDelta, Stats.Io.OtherDelta.Delta);
	Set(eIO_ReadBytesDelta, Stats.Io.ReadRawDelta.Delta);
	Set(eIO_WriteBytesDelta, Stats.Io.WriteRawDelta.Delta);
	Set(eIO_OtherBytesDelta, Stats.Io.OtherRawDelta.Delta);
	Set(eIO_ReadRate, Stats.Io.ReadRate.Get());
	Set(eIO_WriteRate, Stats.Io.WriteRate.Get());
	Set(eIO_OtherRate, Stats.Io.OtherRate.Get());
	Set(eIO_TotalRate, Stats.Io.ReadRate.Get() + Stats.Io.WriteRate.Get() + Stats.Io.OtherRate.Get());
}

//
// The same formatting the process rows get, for the columns a branch fills.
//
// Deliberately a second and much shorter switch rather than a share of the one
// in Sync: that one reaches into the process for half of its cases, and a
// branch has no process to reach into. What is here is only what is above.
//
QVariant CProcessModel::FormatBranchValue(int section, const QVariant& Value, bool bClearZeros) const
{
	//
	// A label, or the "N/A" inherited from the rows below - either way it is
	// already the text to show, and Raw is what the view falls back to.
	//
	if (Value.type() == QVariant::String)
		return QVariant();

	switch (section)
	{
		case eCPU:
		case eGPU_Usage:
			return (!bClearZeros || Value.toDouble() > 0.00004) ? QString::number(Value.toDouble() * 100, 10, 2) + "%" : QString();

		case ePrivateBytes:
		case ePeakPrivateBytes:
		case eWorkingSet:
		case ePrivateWS:
		case eVirtualSize:
		case ePagedPool:
		case eNonPagedPool:

		case eReadBytes:
		case eWriteBytes:
		case eReceiveBytes:
		case eSendBytes:
		case eIO_ReadBytes:
		case eIO_WriteBytes:
		case eIO_OtherBytes:
			return FormatSize(Value.toULongLong());

		// not every machine has a GPU, and not every account touches one
		case eGPU_Shared:
		case eGPU_Dedicated:

		case eReadBytesDelta:
		case eWriteBytesDelta:
		case eReceiveBytesDelta:
		case eSendBytesDelta:
		case eIO_ReadBytesDelta:
		case eIO_WriteBytesDelta:
		case eIO_OtherBytesDelta:
			return FormatSizeEx(Value.toULongLong(), bClearZeros);

		case ePrivateBytesDelta:
		{
			const qint64 iDelta = Value.toLongLong();
			if (iDelta < 0)
				return "-" + FormatSize(iDelta * -1);
			if (iDelta > 0)
				return "+" + FormatSize(iDelta);
			return bClearZeros ? QString() : QString("0");
		}

		case eCycles:
		case eContextSwitches:
		case ePageFaults:
		case eHardFaults:
		case eReads:
		case eWrites:
		case eReceives:
		case eSends:
		case eIO_Reads:
		case eIO_Writes:
		case eIO_Other:
			return FormatNumber(Value.toULongLong());

		case eCyclesDelta:
		case eContextSwitchesDelta:
		case ePageFaultsDelta:
		case eHardFaultsDelta:
		case eReadsDelta:
		case eWritesDelta:
		case eReceivesDelta:
		case eSendsDelta:
		case eIO_ReadsDelta:
		case eIO_WritesDelta:
		case eIO_OtherDelta:
		case eWND_Handles:
		case eGDI_Handles:
		case eUSER_Handles:
			return FormatNumberEx(Value.toULongLong(), bClearZeros);

		case eDisk_TotalRate:
		case eReadRate:
		case eWriteRate:
		case eNet_TotalRate:
		case eReceiveRate:
		case eSendRate:
		case eIO_TotalRate:
		case eIO_ReadRate:
		case eIO_WriteRate:
		case eIO_OtherRate:
			return FormatRateEx(Value.toULongLong(), bClearZeros);

		case eTotalCPU_Time:
		case eKernelCPU_Time:
		case eUserCPU_Time:
		case eUpTime:
			return FormatTime(Value.toULongLong());

		case eStartTime:
			return QDateTime::fromSecsSinceEpoch(Value.toULongLong() / 1000).toString("dd.MM.yyyy hh:mm:ss");
	}

	//
	// Everything else - handles, threads, the plain counts - is shown as the
	// number it is, which is what Sync does for them too.
	//
	return QVariant();
}

void CProcessModel::UpdateBranches(STreeNode* pParent, bool bClearZeros)
{
	foreach(STreeNode* pNode, pParent->Children)
	{
		if (!pNode->Virtual)
			continue;

		FillBranch(pNode, bClearZeros);

		//
		// Account branches sit inside machine branches when both modes are on,
		// so the walk goes down rather than across. FillBranch has already
		// summed this node's whole subtree by then; doing it again one level
		// down costs a second pass over the same rows and is what lets a
		// machine and the accounts on it both add up.
		//
		UpdateBranches(pNode, bClearZeros);
	}
}

void CProcessModel::FillBranch(STreeNode* pNode, bool bClearZeros)
{
	if (pNode->Values.size() <= eProcess)
		return;

	QString Name;
	QString State;
	QString UserKey;
	CSystemAPI* pSystem = NULL;

	if (IsUserBranch(pNode->ID, &UserKey))
		Name = GroupDisplayName(UserKey, m_UserNames.value(UserKey));
	else if (IsMachineBranch(pNode->ID))
	{
		Name = m_MachineNames.value(pNode->ID.toString());

		//
		// A machine row has no status of its own to show - it is not a
		// process - so the column says whether the machine is answering. Empty
		// while it is, because a word on every row every second is not
		// information.
		//
		State = m_MachineStates.value(pNode->ID.toString());
		pSystem = SystemFromBranchId(pNode->ID.toString());
	}
	else
		return;

	//
	// Built whole and then compared, rather than written as it goes: a column
	// that has nothing to say this round has to end up empty, and only a fresh
	// vector can tell "nothing to say" from "unchanged".
	//
	QVector<QVariant> New(pNode->Values.size());
	New[eProcess] = Name;
	if (New.size() > eStatus)
		New[eStatus] = State;

	QVector<double> Sums(New.size(), 0.0);
	QSet<int> NotAvailable;
	SumSubtree(pNode, Sums, NotAvailable);


	for (int i = 0; i < New.size(); i++)
	{
		if (!IsSummableColumn(i) || m_ColumnsOff.contains(i))
			continue;

		New[i] = NotAvailable.contains(i) ? QVariant(tr("N/A")) : MkSum(i, Sums[i]);
	}

	if (pSystem)
		FillMachineColumns(pSystem, New);

	int First = -1;
	int Last = -1;
	for (int i = 0; i < New.size(); i++)
	{
		STreeNode::SValue& Value = pNode->Values[i];
		if (Value.Raw == New[i])
			continue;

		Value.Raw = New[i];
		Value.Formatted = New[i].isValid() ? FormatBranchValue(i, New[i], bClearZeros) : QVariant();

		if (First == -1)
			First = i;
		Last = i;
	}

	if (First == -1)
		return;

	QModelIndex Index = Find(m_Root, pNode);
	if (Index.isValid())
		emit dataChanged(createIndex(Index.row(), First, pNode), createIndex(Index.row(), Last, pNode));
}

//
// A branch, given a label.
//
// Called from CTreeItemModel::Fill while the paths are being laid down, which
// is after the process loop below has recorded what each account is called.
//
CTreeItemModel::STreeNode* CProcessModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (pNode->Values.size() > eProcess)
	{
		//
		// The same two icons the Tasks menu uses for the same two things, so a
		// branch reads as a machine or an account at a glance. Without them the
		// tree falls back to GetDefaultIcon, which is the generic executable -
		// wrong for something that is not a process at all.
		//
		// Built once: a tree with a dozen branches would otherwise decode the
		// same two PNGs on every rebuild.
		//
		static const QPixmap UserIcon = QIcon(":/Actions/Users").pixmap(16, 16);
		static const QPixmap MachineIcon = QIcon(":/Actions/Computer").pixmap(16, 16);

		QString UserKey;
		if (IsUserBranch(Id, &UserKey))
		{
			pNode->Values[eProcess].Raw = GroupDisplayName(UserKey, m_UserNames.value(UserKey));
			pNode->Icon = UserIcon;
			pNode->IsBold = true;
		}
		else if (IsMachineBranch(Id))
		{
			pNode->Values[eProcess].Raw = m_MachineNames.value(Id.toString());
			pNode->Icon = MachineIcon;
			pNode->IsBold = true;
		}
	}

	return pNode;
}

QSet<quint64> CProcessModel::Sync(const QList<QMap<SProcessUID, CProcessPtr> >& Lists)
{
	QSet<quint64> Added;
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	//
	// Rebuilt every round rather than accumulated: an account with no processes
	// left has no branch either, and a stale name would outlive the last thing
	// that could confirm it.
	//
	m_UserNames.clear();
	m_MachineNames.clear();
	m_MachineStates.clear();

	//
	// Who owns each logon session, for the branch headings - see GetGroupKey.
	// Asked of the machine rather than worked out from its processes: the
	// session list is what the platform itself calls a login, and the account a
	// process runs as is often not the person whose session it is at all.
	//
	// Once per round and only where the grouping needs it. A session with no
	// name is not an error - session 0 holds the services and belongs to
	// nobody - and falls back to what the branch is: a numbered session.
	//
	if (m_bMultiUser)
	{
		foreach(const CSystemPtr& pSystem, CCluster::GetSystems())
		{
			if (pSystem.isNull() || pSystem->GetOsType() != CSystemAPI::eOsWindows)
				continue;

			foreach(const CSystemAPI::SUser& User, pSystem->GetUsers())
			{
				if (User.UserName.isEmpty())
					continue;
				m_UserNames.insert(QString("session:%1").arg(User.SessionId), User.UserName);
			}
		}
	}

	//
	// The machine labels, once per round rather than once per process - they do
	// not depend on the process list at all.
	//
	// The wording lives in ::GetMachineDisplayName, because the title bar shows
	// the same thing when the machine layer is off and the two must not drift.
	//
	if (m_bMultiMachine)
	{
		foreach(const CSystemPtr& pSystem, CCluster::GetSystems())
		{
			if (pSystem.isNull())
				continue;

			const QString Branch = MkMachineBranch(pSystem.data()).toString();
			m_MachineNames.insert(Branch, ::GetMachineDisplayName(pSystem.data()));
			m_MachineStates.insert(Branch, ::GetMachineStateString(pSystem.data()));
		}
	}

	bool bShow32 = theConf->GetBool("Options/Show32", true);
	bool bClearZeros = theConf->GetBool("Options/ClearZeros", true);
	bool bShowMaxThread = theConf->GetBool("Options/ShowMaxThread", false);
	int iHighlightMax = theConf->GetInt("Options/HighLoadHighlightCount", 5);
	time_t curTime = GetTime();

	bool bGpuStats = !m_ColumnsOff.contains(eGPU_History) || !m_ColumnsOff.contains(eVMEM_History)
		|| !m_ColumnsOff.contains(eGPU_Usage) || !m_ColumnsOff.contains(eGPU_Shared) || !m_ColumnsOff.contains(eGPU_Dedicated) || !m_ColumnsOff.contains(eGPU_Adapter);

	QVector<QList<QPair<quint64, SProcessNode*> > > Highlights;
	if(iHighlightMax > 0)
		Highlights.resize(columnCount());

	//
	// Machine by machine, so that every parent lookup below happens inside the
	// list the process came from.
	//
	typedef QMap<SProcessUID, CProcessPtr> TProcessList;	// foreach cannot see past the comma
	foreach (const TProcessList& ProcessList, Lists)
	foreach (const CProcessPtr& pProcess, ProcessList)
	{
		//
		// A reflected process is a snapshot clone, not something the user
		// started; it has no meaning in the tree. Systems without the notion
		// never report one.
		//
		if (pProcess->IsReflectedProcess())
			continue;

		if(pProcess->GetParentUId().Get() == 0)
			continue;
		SProcessUID UID = pProcess->GetProcessUId();

		//
		// The heading for an account branch, which is the account's own name.
		//
		// Session branches are not named from here. Their name came from the
		// session list gathered before this loop, and where that list does not
		// mention one - session 0, which holds the services and belongs to
		// nobody who logged in - the answer is that it has no owner rather than
		// the account of whichever of its processes happened to be seen first.
		// That guess reads as a fact: a branch labelled with one service
		// account while holding three of them is wrong, not merely vague.
		//
		if (m_bMultiUser)
		{
			const QString Key = GetGroupKey(pProcess);
			if (!Key.startsWith("session:"))
				m_UserNames.insert(Key, pProcess->GetUserName());
		}


		//
		// Whether the per-process network and disk figures can be filled in at all.
		// On Windows both need ETW, and the disk ones additionally need the
		// extended counter set; where they are unavailable the columns read "N/A"
		// rather than a misleading zero.
		//
		// Read per row, not once before the loop: a capability belongs to the
		// system the process was observed on, and with more than one connected
		// the answer differs from row to row.
		//
		CSystemPtr pSystem = pProcess->GetSystem();
		bool HasExtProcInfo = pSystem->HasCapability(CSystemAPI::eCapExtProcInfo);
		bool IsMonitoringETW = pSystem->HasCapability(CSystemAPI::eCapEtw);
	
		QModelIndex Index;
		
		//
		// The node id is the object uid, not the process uid.
		//
		// With more than one machine in the tree an SProcessUID is not unique -
		// it is a pid and a creation time, and two machines can hold the same
		// pair. CAbstractInfo::GetObjectUid is a counter in *this* process, so
		// every process object the viewer holds has a different one whichever
		// machine it came from. See the note in CVariantCacheT about identity.
		//
		const quint64 NodeId = pProcess->GetObjectUid();

		QHash<QVariant, STreeNode*>::iterator I = Old.find(NodeId);
		SProcessNode* pNode = I != Old.end() ? static_cast<SProcessNode*>(I.value()) : NULL;
		if(!pNode || !TestPath(pNode->Path, pProcess, ProcessList))
		{
			pNode = static_cast<SProcessNode*>(MkNode(NodeId));
			pNode->Values.resize(columnCount());
			pNode->Path = MakePath(pProcess, ProcessList);
			pNode->pProcess = pProcess;
			New[pNode->Path].append(pNode);
			Added.insert(NodeId);
		}
		else
		{
			I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));
		
        CModulePtr pModule = pProcess->GetModuleInfo();
		CTokenInfoPtr pToken = pProcess->GetToken();

		int Col = 0;
		bool State = false;
		int Changed = 0;

		// Note: icons are loaded asynchroniusly
		if (!pNode->Icon.isValid() && pModule)
		{
			QPixmap Icon = ::MakeIcon(pModule->GetFileIcon());
			if (!Icon.isNull()) {
				Changed = 1; // set change for first column
				pNode->Icon = Icon;
			}
		}


		int RowColor = CTaskExplorer::eNone;

		//
		// Wine first, ahead of everything.
		//
		// It used to sit just above the plain user process, on the reasoning that
		// a program under Wine is nearly always the current user's and would
		// otherwise be lost among the others that are. That holds on a desktop
		// running its own programs and not on a machine watched through a daemon:
		// there the whole prefix belongs to root, so every one of them answered
		// yes to system and elevated first and came out the same colour as the
		// rest of the box - with the status column still saying Wine, which is
		// what made it look broken rather than merely ranked.
		//
		// Nothing is hidden by putting it here. The tests it now precedes are a
		// changed token, a critical process and a sandbox - none of which a Wine
		// process can answer yes to: the first two are kernel notions the Linux
		// collector does not report, and the third is Sandboxie. What it does
		// displace is system and elevated, which is the point.
		//
		if ((pProcess->GetStatusFlags() & CProcessInfo::eStatusWine)
			 && CTaskExplorer::UseListColor(CTaskExplorer::eWine))							RowColor = CTaskExplorer::eWine;
		else if (pProcess->TokenHasChanged() && CTaskExplorer::UseListColor(CTaskExplorer::eDangerous))				RowColor = CTaskExplorer::eDangerous;
		else if (pProcess->IsCriticalProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eIsProtected))		RowColor = CTaskExplorer::eIsProtected;
		else if (pProcess->IsSandBoxed() && CTaskExplorer::UseListColor(CTaskExplorer::eSandBoxed))				RowColor = CTaskExplorer::eSandBoxed;
		else
			 if (pProcess->IsServiceProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eService))			RowColor = CTaskExplorer::eService;
		else if (pProcess->IsSystemProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eSystem))			RowColor = CTaskExplorer::eSystem;
		else if (pProcess->IsElevated() && CTaskExplorer::UseListColor(CTaskExplorer::eElevated))				RowColor = CTaskExplorer::eElevated;
		else if (pProcess->IsSubsystemProcess() && CTaskExplorer::UseListColor(CTaskExplorer::ePico))			RowColor = CTaskExplorer::ePico;
		else if (pProcess->IsImmersiveProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eImmersive))		RowColor = CTaskExplorer::eImmersive;
		else if (pProcess->IsNetProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eDotNet))				RowColor = CTaskExplorer::eDotNet;
		//else if (pProcess->IsJobProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eJob))				RowColor = CTaskExplorer::eJob;
		else if (pProcess->IsInJob() && CTaskExplorer::UseListColor(CTaskExplorer::eJob))						RowColor = CTaskExplorer::eJob;
		else if (pProcess->IsUserProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eUser))				RowColor = CTaskExplorer::eUser;
		
		int SortKey = RowColor; // all but he new/removed

		//
		// Ended, by either account.
		//
		// This side's own mark is what a local collector sets when a process
		// leaves the list, and it is the only thing that was looked at here.
		// A process on another machine is not on this side's list at all - it
		// arrives, and what it says about itself is all there is - so the
		// reported state has to count too. The Windows and Linux collectors
		// both derive that flag from the same mark, so a local row is unchanged
		// by this; a remote one stops claiming to be alive.
		//
		if ((pProcess->IsMarkedForRemoval() || (pProcess->GetStatusFlags() & CProcessInfo::eStatusTerminated))
			&& CTaskExplorer::UseListColor(CTaskExplorer::eToBeRemoved))			RowColor = CTaskExplorer::eToBeRemoved;
		else if (pProcess->IsNewlyCreated() && CTaskExplorer::UseListColor(CTaskExplorer::eAdded))				RowColor = CTaskExplorer::eAdded;

		if (pNode->iColor != RowColor) {
			pNode->iColor = RowColor;
			for (int i = 0; i < columnCount(); i++)
				pNode->Values[i].Color = CTaskExplorer::GetListColor(RowColor);
			Changed = 2;
		}

		if (pNode->IsGray != pProcess->IsSuspended())
		{
			pNode->IsGray = pProcess->IsSuspended();
			Changed = 2;
		}

		pNode->Bold.clear();

		SProcStats IoStats = pProcess->GetStats();
		STaskStatsEx CpuStats = pProcess->GetCpuStats();
		STaskStats CpuStats2 = pProcess->GetCpuStats2();
		SGpuStats GpuStats;
		if (bGpuStats) // Note: GetGpuStats marks gpu stats to be keept updated, hence don't get it when its not needed
			GpuStats = pProcess->GetGpuStats();

		for(int section = 0; section < columnCount(); section++)
		{
			if (m_ColumnsOff.contains(section))
				continue; // ignore columns which are hidden

			quint64 CurIntValue = -1;

			QVariant Value;
			switch(section)
			{
				case eProcess:
				{
					QString Name = ::LocalizeName(pProcess->GetName());
					Name += bShow32 && pProcess->IsWoW64() ? " *32" : "";
					if (m_iUseDescr)
					{
						QString Descr;
						bool IsSvcHost = Name.compare("svchost.exe", Qt::CaseInsensitive) == 0;
						if (IsSvcHost && pProcess->IsServiceProcess())
						{
							Descr = pProcess->GetServiceList().join(tr(", "));
						}
						else
							//
							// Asked of the process, not of its module. On a
							// platform where a process can rename itself the
							// answer is the process's, and the module is not
							// always there to ask - see
							// CProcessInfo::GetDescription.
							//
							Descr = pProcess->GetDescription();

						if (!Descr.isEmpty())
						{
							if (m_iUseDescr == 1)
								Value = Descr + " (" + Name + ")";
							else
								Value = Name + " (" + Descr + ")";
							break;
						}
					}
											Value = Name; break;
				}
				case ePID:					Value = pProcess->GetProcessId(); break;
				case ePID_LXSS:				Value = pProcess->GetLXSSProcessId(); break;
				case eParentPID:			Value = pProcess->GetParentId(); break;
				// Console host, sequence number and start key are Windows
				// kernel concepts; these columns stay empty on Linux.
				case eConsolePID:			Value = pProcess->GetConsoleHostId(); break;
				case eSequenceNumber:		Value = pProcess->GetProcessSequenceNumber(); break;
				case eStartKey:				Value = pProcess->GetStartKey(); break;
				case eCPU_History:
				case eCPU:					Value = CpuStats.CpuUsage; CurIntValue = 10000 * Value.toDouble(); break;
				case eIO_History:			Value = qMax(IoStats.Disk.ReadRate.Get(), IoStats.Io.ReadRate.Get()) + qMax(IoStats.Disk.WriteRate.Get(), IoStats.Io.WriteRate.Get()) + IoStats.Io.OtherRate.Get(); break;
				case eIO_TotalRate:			Value = CurIntValue = IoStats.Io.ReadRate.Get() + IoStats.Io.WriteRate.Get() + IoStats.Io.OtherRate.Get(); break;
				case eStatus:				Value = ::GetStatusString(pProcess); break;
				case ePrivateBytes:			Value = CurIntValue = CpuStats.PrivateBytesDelta.Value; break;
				case eUserName:				Value = ::LocalizeName(pProcess->GetUserName()); break;
				case eServices:				Value = pProcess->GetServiceList().join(tr(", ")); break;
				//
				// Portable: these read the main module's file details, which the
				// Linux backend fills from the ELF packaging note or the
				// application's desktop entry. Previously Windows-only, which
				// left the columns permanently blank on Linux.
				//
				case eDescription:			Value = pProcess->GetDescription(); break;
				case eCompanyName:			Value = pModule ? pModule->GetFileInfo("CompanyName") : ""; break;
				case eVersion:				Value = pModule ? pModule->GetFileInfo("FileVersion") : ""; break;

				case eGPU_History:			Value = GpuStats.GpuTimeUsage.Usage; break;
				case eVMEM_History:			Value = qMax(GpuStats.GpuDedicatedUsage,GpuStats.GpuSharedUsage); break;

				case eGPU_Usage:			Value = GpuStats.GpuTimeUsage.Usage; CurIntValue = 10000 * Value.toDouble(); break;
				case eGPU_Shared:			Value = CurIntValue = GpuStats.GpuSharedUsage; break;
				case eGPU_Dedicated:		Value = CurIntValue = GpuStats.GpuDedicatedUsage; break;
				case eGPU_Adapter:			Value = GpuStats.GpuAdapter; break;

				case eFileName:				Value = pProcess->GetFileName(); break;
				case eCommandLine:			Value = pProcess->GetCommandLineStr(); break;
				case ePeakPrivateBytes:		Value = CurIntValue = pProcess->GetPeakPrivateBytes(); break;
				case eMEM_History:
				case eWorkingSet:			Value = CurIntValue = pProcess->GetWorkingSetSize(); break;
				case ePeakWS:				Value = CurIntValue = pProcess->GetPeakWorkingSetSize(); break;
				case ePrivateWS:			Value = CurIntValue = pProcess->GetPrivateWorkingSetSize(); break;
				case eSharedWS:				Value = CurIntValue = pProcess->GetSharedWorkingSetSize(); break;
				case eShareableWS:			Value = CurIntValue = pProcess->GetShareableWorkingSetSize(); break;
				case eShareableCommit:		Value = CurIntValue = pProcess->GetShareableCommitSize(); break;
				case eVirtualSize:			Value = CurIntValue = pProcess->GetVirtualSize(); break;
				case ePeakVirtualSize:		Value = CurIntValue = pProcess->GetPeakVirtualSize(); break;
				case eSessionID:			Value = pProcess->GetSessionID(); break;
				case eDebugTotal:			Value = pProcess->GetDebugMessageCount(); break;
				case eAffinity:				Value = pProcess->GetAffinityMask(); break;
				case ePriorityClass:		Value = (quint32)pProcess->GetPriority(); break;
				case eBasePriority:			Value = (quint32)pProcess->GetBasePriority(); break;
				case ePriorityBoost:		Value = pProcess->HasPriorityBoost(); break;
				case eThreads:				Value = CurIntValue = (quint32)pProcess->GetNumberOfThreads(); break;
				case ePeakThreads:			Value = CurIntValue = (quint32)pProcess->GetPeakNumberOfThreads(); break;
				case eHandles:				Value = CurIntValue = (quint32)pProcess->GetNumberOfHandles(); break;
				case ePeakHandles:			Value = CurIntValue = (quint32)pProcess->GetPeakNumberOfHandles(); break;
				case eWND_Handles:			Value = CurIntValue = (quint32)pProcess->GetWndHandles(); break;
				case eGDI_Handles:			Value = CurIntValue = (quint32)pProcess->GetGdiHandles(); break;
				case eUSER_Handles:			Value = CurIntValue = (quint32)pProcess->GetUserHandles(); break;
				case eIntegrity:			Value = pToken ? pToken->GetIntegrityLevel() : 0; break;
				case eIO_Priority:			Value = (quint32)pProcess->GetIOPriority(); break;
				case ePagePriority:			Value = (quint32)pProcess->GetPagePriority(); break;
				case eStartTime:			Value = pProcess->GetCreateTimeStamp(); break;
				case eTotalCPU_Time:		Value = CurIntValue = (CpuStats.CpuKernelDelta.Value + CpuStats.CpuUserDelta.Value) / pSystem->GetCpuTimeDivider(); break;
				case eKernelCPU_Time:		Value = CurIntValue = CpuStats.CpuKernelDelta.Value / pSystem->GetCpuTimeDivider(); break;
				case eUserCPU_Time:			Value = CurIntValue = CpuStats.CpuUserDelta.Value / pSystem->GetCpuTimeDivider(); break;
				case eVerificationStatus:	Value = ::GetVerifyResultString(pModule); break;
				case eVerifiedSigner:		Value = pModule ? pModule->GetVerifySignerName() : ""; break;
				case eMitigations:			Value = ::GetMitigationsString(pProcess); break;
				case eImageCoherency:		Value = pModule ? pModule->GetImageCoherency() : -1.0F; break;
				case eUpTime:				Value = pProcess->GetCreateTimeStamp() != 0 ? (curTime - pProcess->GetCreateTimeStamp() / 1000) : 0; break; // we must update the value to refresh the display
				case eArchitecture:			Value = ::GetArchString(pProcess); break;
				case eElevation:			Value = ::GetElevationString(pToken); break;
				case eWindowTitle:			Value = pProcess->GetWindowTitle();  break;
				case eWindowStatus:			Value = ::GetWindowStatusString(pProcess); break;
				case eCycles:				Value = CurIntValue = CpuStats.CycleDelta.Value; break;
				case eCyclesDelta:			Value = CurIntValue = CpuStats.CycleDelta.Delta; break;
				case eVirtualized:			Value = ::GetVirtualizationString(pToken); break;
				case eContextSwitches:		Value = CurIntValue = CpuStats.ContextSwitchesDelta.Value; break;
				case eContextSwitchesDelta:	Value = CurIntValue = CpuStats.ContextSwitchesDelta.Delta; break;
				case ePageFaults:			Value = CurIntValue = CpuStats.PageFaultsDelta.Value; break;
				case ePageFaultsDelta:		Value = CurIntValue = CpuStats.PageFaultsDelta.Delta; break;
				case eHardFaults:			Value = CurIntValue = CpuStats.HardFaultsDelta.Value; break;
				case eHardFaultsDelta:		Value = CurIntValue = CpuStats.HardFaultsDelta.Delta; break;

				// IO
				case eIO_Reads:				Value = CurIntValue = IoStats.Io.ReadCount; break;
				case eIO_Writes:			Value = CurIntValue = IoStats.Io.WriteCount; break;
				case eIO_Other:				Value = CurIntValue = IoStats.Io.OtherCount; break;
				case eIO_ReadBytes:			Value = CurIntValue = IoStats.Io.ReadRaw; break;
				case eIO_WriteBytes:		Value = CurIntValue = IoStats.Io.WriteRaw; break;
				case eIO_OtherBytes:		Value = CurIntValue = IoStats.Io.OtherRaw; break;
				//case eIO_TotalBytes:		Value = CurIntValue = ; break;
				case eIO_ReadsDelta:		Value = CurIntValue = IoStats.Io.ReadDelta.Delta; break;
				case eIO_WritesDelta:		Value = CurIntValue = IoStats.Io.WriteDelta.Delta; break;
				case eIO_OtherDelta:		Value = CurIntValue = IoStats.Io.OtherDelta.Delta; break;
				//case eIO_TotalDelta:		Value = CurIntValue = ; break;
				case eIO_ReadBytesDelta:	Value = CurIntValue = IoStats.Io.ReadRawDelta.Delta; break;
				case eIO_WriteBytesDelta:	Value = CurIntValue = IoStats.Io.WriteRawDelta.Delta; break;
				case eIO_OtherBytesDelta:	Value = CurIntValue = IoStats.Io.OtherRawDelta.Delta; break;
				//case eIO_TotalBytesDelta:	Value = CurIntValue = ; break;
				case eIO_ReadRate:			Value = CurIntValue = IoStats.Io.ReadRate.Get(); break;
				case eIO_WriteRate:			Value = CurIntValue = IoStats.Io.WriteRate.Get(); break;
				case eIO_OtherRate:			Value = CurIntValue = IoStats.Io.OtherRate.Get(); break;
				//case eIO_TotalRate:		Value = CurIntValue = ; break;
				case eOS_Context:			Value = (quint32)pProcess->GetOsContextVersion(); break;
				case ePagedPool:			Value = CurIntValue = pProcess->GetPagedPool(); break;
				case eTLS:					Value = CurIntValue = pProcess->GetTlsBitmapCount(); break;
				case ePeakPagedPool:		Value = CurIntValue = pProcess->GetPeakPagedPool(); break;
				case eNonPagedPool:			Value = CurIntValue = pProcess->GetNonPagedPool(); break;
				case ePeakNonPagedPool:		Value = CurIntValue = pProcess->GetPeakNonPagedPool(); break;
				case eMinimumWS:			Value = /*CurIntValue =*/ pProcess->GetMinimumWS(); break;
				case eMaximumWS:			Value = /*CurIntValue =*/ pProcess->GetMaximumWS(); break;
				case ePrivateBytesDelta:	Value = /*CurIntValue =*/ CpuStats.PrivateBytesDelta.Delta; break;
				case eSubsystem:			Value = (quint32)pProcess->GetSubsystem(); break;
				case eOomScore:				Value = CurIntValue = pProcess->GetOomScore(); break;
				case eOomScoreAdj:			Value = pProcess->GetOomScoreAdj(); break;
				case eContainer:			Value = pProcess->GetContainer(); break;
				case eConfinement:			Value = pProcess->GetConfinement(); break;
				case eInotifyWatches:		Value = CurIntValue = pProcess->GetInotifyWatches(); break;
				case eCGroup:				Value = pProcess->GetCGroupPath(); break;
				case ePackageName:			Value = pProcess->GetPackageName(); break;
				case eAppID:				Value = pProcess->GetAppID();  break;
				case eDPI_Awareness:		Value = (int)pProcess->GetDPIAwareness(); break;
				case eTimeStamp:			Value = pModule ? pModule->GetTimeStamp() : 0; break;
				case eFileModifiedTime:		Value = pModule ? pModule->GetModificationTime() : 0; break;
				case eFileSize:				Value = pModule ? pModule->GetFileSize() : 0; break;
				case eJobObjectID:			Value = pProcess->GetJobObjectID(); break;
				case eProtection:			Value = (int)pProcess->GetProtection(); break;
				case eDesktop:				Value = pProcess->GetUsedDesktop(); break;
				case eCritical:				Value = pProcess->IsCriticalProcess(); break;

				case ePowerThrottling:		Value = pProcess->IsPowerThrottled(); break;

				case eRunningTime:			Value = pProcess->GetUpTime(); break;
				case eSuspendedTime:		Value = pProcess->GetSuspendTime(); break;
				case eHangCount:			Value = pProcess->GetHangCount(); break;
				case eGhostCount:			Value = pProcess->GetGhostCount(); break;

				case eErrorMode:			Value = pProcess->GetErrorMode(); break;
				case eCodePage:				Value = pProcess->GetCodePage(); break;
				case eReferences:			Value = pProcess->GetReferenceCount(); break;
				case eGrantedAccess:		Value = pProcess->GetAccessMask(); break;
				// Network IO
				case eNET_History:
				case eNet_TotalRate:		Value = CurIntValue = IoStats.Net.ReceiveRate.Get() + IoStats.Net.SendRate.Get(); break; 
				case eNetUsage:				Value = pProcess->GetNetworkUsageFlags(); break;
				case eReceives:				Value = CurIntValue = IoStats.Net.ReceiveCount; break; 
				case eSends:				Value = CurIntValue = IoStats.Net.SendCount; break; 
				case eReceiveBytes:			Value = CurIntValue = IoStats.Net.ReceiveRaw; break; 
				case eSendBytes:			Value = CurIntValue = IoStats.Net.SendRaw; break; 
				//case eTotalBytes:			Value = CurIntValue = ; break; 
				case eReceivesDelta:		Value = CurIntValue = IoStats.Net.ReceiveDelta.Delta; break; 
				case eSendsDelta:			Value = CurIntValue = IoStats.Net.SendDelta.Delta; break; 
				case eReceiveBytesDelta:	Value = CurIntValue = IoStats.Net.ReceiveRawDelta.Delta; break; 
				case eSendBytesDelta:		Value = CurIntValue = IoStats.Net.SendRawDelta.Delta; break; 
				//case eTotalBytesDelta:	Value = CurIntValue = ; break; 
				case eReceiveRate:			Value = CurIntValue = IoStats.Net.ReceiveRate.Get(); break; 
				case eSendRate:				Value = CurIntValue = IoStats.Net.SendRate.Get(); break; 

				// Disk IO
				case eDisk_TotalRate:		Value = CurIntValue = IoStats.Disk.ReadRate.Get() + IoStats.Disk.WriteRate.Get(); break; 
				case eReads:				Value = CurIntValue = IoStats.Disk.ReadCount; break;
				case eWrites:				Value = CurIntValue = IoStats.Disk.WriteCount; break;
				case eReadBytes:			Value = CurIntValue = IoStats.Disk.ReadRaw; break;
				case eWriteBytes:			Value = CurIntValue = IoStats.Disk.WriteRaw; break;
				//case eTotalBytes:			Value = CurIntValue = ; break;
				case eReadsDelta:			Value = CurIntValue = IoStats.Disk.ReadDelta.Delta; break;
				case eWritesDelta:			Value = CurIntValue = IoStats.Disk.WriteDelta.Delta; break;
				case eReadBytesDelta:		Value = CurIntValue = IoStats.Disk.ReadRawDelta.Delta; break;
				case eWriteBytesDelta:		Value = CurIntValue = IoStats.Disk.WriteRawDelta.Delta; break;
				//case eTotalBytesDelta:	Value = CurIntValue = ; break;
				case eReadRate:				Value = CurIntValue = IoStats.Disk.ReadRate.Get(); break;
				case eWriteRate:			Value = CurIntValue = IoStats.Disk.WriteRate.Get(); break;
			}

			SProcessNode::SValue& ColValue = pNode->Values[section];

			bool bChanged = false;
			if (!IsMonitoringETW // Note: this columns are not available without ETW being enabled
			 && (section == eNET_History || (section >= eNet_TotalRate && section <= eSendRate)
			 || ((section >= eDisk_TotalRate && section <= eWriteRate) && !HasExtProcInfo)))
				Value = tr("N/A");
			else
			if (CurIntValue == -1)
			{
				CurIntValue = 0;
				bChanged = (ColValue.Raw != Value);
			}
			else if (ColValue.Raw.isNull())
				bChanged = true;
			else // if the change is less than 0.01%, i.e. in unit notation the difference will be not displayed, dont issue an update
			{
				// Note: this savec 20% of CPU load in the debug build

				quint64 OldIntValue;
				switch (section)
				{
					case eCPU:
					case eGPU_Usage:
						OldIntValue = ColValue.Raw.toDouble() * 10000; 
						break;
					default:
						OldIntValue = ColValue.Raw.toULongLong();
				}
				
				if (CurIntValue > OldIntValue)
					bChanged = (10000 * (CurIntValue - OldIntValue) / CurIntValue) > 0;
				else if (OldIntValue > CurIntValue)
					bChanged = (10000 * (OldIntValue - CurIntValue) / OldIntValue) > 0;
			}

			if (bChanged)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch(section)
				{
					case ePID:
					case eParentPID:
					case eConsolePID:
											if (Value.toLongLong() < 0) ColValue.Formatted = ""; 
											else ColValue.Formatted = theGUI->FormatID(Value.toLongLong()); 
											break;
					case eStartKey:			ColValue.Formatted = tr("0x%1").arg(Value.toULongLong(), 0, 16); break;
					case eCPU:
											if(!bClearZeros || Value.toDouble() > 0.00004)
											{
												QString ValueStr = QString::number(Value.toDouble()*100, 10, 2) + "%"; 
												if(bShowMaxThread)
													ValueStr += " / " + QString::number(CpuStats2.CpuUsage*100, 10, 2) + "%"; 
												ColValue.Formatted = ValueStr;
											}
											else
												ColValue.Formatted = "";
											break;


					case eGPU_Usage:
											ColValue.Formatted = (!bClearZeros || Value.toDouble() > 0.00004) ? QString::number(Value.toDouble()*100, 10, 2) + "%" : ""; break;

					case eStatus:			ColValue.SortKey = SortKey;	break;

					case ePrivateBytes:		
					case ePeakPrivateBytes:
					case eWorkingSet:
					case ePeakWS:
					case ePrivateWS:
					case eSharedWS:
					case eShareableWS:
					case eShareableCommit:
					case eVirtualSize:
					case ePeakVirtualSize:
					case ePagedPool:
					case ePeakPagedPool:
					case eNonPagedPool:
					case ePeakNonPagedPool:
					case eMinimumWS:
					case eMaximumWS:

					case eFileSize:
											ColValue.Formatted = FormatSize(Value.toULongLong()); break;
					case eTLS:				ColValue.Formatted = ::GetTlsBitmapCountString(pProcess); break;

					case eErrorMode:		ColValue.Formatted = ::GetErrorModeString(pProcess); break;
					case eGrantedAccess:	ColValue.Formatted = ::GetAccessMaskString(pProcess); break;
					// since not all programs use GPU memory, make this value clearable
					case eGPU_Dedicated:
					case eGPU_Shared:
											ColValue.Formatted = FormatSizeEx(Value.toULongLong(), bClearZeros); break;

					case ePrivateBytesDelta:
					{
						qint64 iDelta = Value.toLongLong();
						if (iDelta < 0)
							ColValue.Formatted = "-" + FormatSize(iDelta * -1);
						else if (iDelta > 0)
							ColValue.Formatted = "+" + FormatSize(iDelta);
						else if (bClearZeros)
							ColValue.Formatted = QString();
						else
							ColValue.Formatted = "0";
						break;
					}

					case eCycles:
					case eContextSwitches:
					case ePageFaults:
					case eHardFaults:
					case eIO_Reads:
					case eIO_Writes:
					case eIO_Other:
					case eReceives:
					case eSends:
					case eReads:
					case eWrites:
										ColValue.Formatted = FormatNumber(Value.toULongLong()); break;
					// since not all programs use GUI resources, make this value clearable
					case eWND_Handles:
					case eGDI_Handles:
					case eUSER_Handles:
					case eHangCount:
					case eGhostCount:
					case eCyclesDelta:
					case eContextSwitchesDelta:
					case ePageFaultsDelta:
					case eHardFaultsDelta:
					case eIO_ReadsDelta:
					case eIO_WritesDelta:
					case eIO_OtherDelta:
					case eReceivesDelta:
					case eSendsDelta:
					case eReadsDelta:
					case eWritesDelta:
											ColValue.Formatted = FormatNumberEx(Value.toULongLong(), bClearZeros); break;

					case eStartTime:		ColValue.Formatted = QDateTime::fromSecsSinceEpoch(Value.toULongLong()/1000).toString("dd.MM.yyyy hh:mm:ss"); break;
					case eTimeStamp:
					case eFileModifiedTime:
											if (Value.toULongLong() != 0) ColValue.Formatted = QDateTime::fromSecsSinceEpoch(Value.toULongLong()).toString("dd.MM.yyyy hh:mm:ss"); break;
                    case eUpTime:
					case eRunningTime:			
					case eSuspendedTime:
											ColValue.Formatted = (Value.toULongLong() == 0) ? QString() : FormatTime(Value.toULongLong()); break;
					case eTotalCPU_Time:
					case eKernelCPU_Time:
					case eUserCPU_Time:
											ColValue.Formatted = FormatTime(Value.toULongLong()); break;
											
					case eAffinity:			ColValue.Formatted = ::GetAffinityMaskString(pProcess.data()); break;
					case ePriorityClass:	ColValue.Formatted = ::GetPriorityString(pProcess); break;
					case ePriorityBoost:	ColValue.Formatted = pProcess->HasPriorityBoost() ? tr("Yes") : ""; break;
					case eBasePriority:		ColValue.Formatted = ::GetBasePriorityString(pProcess); break;
					case ePagePriority:		ColValue.Formatted = ::GetPagePriorityString(pProcess); break;
					case eIO_Priority:		ColValue.Formatted = ::GetIOPriorityString(pProcess); break;
					case eIntegrity:		ColValue.Formatted = ::GetIntegrityString(pToken); break;
					case eImageCoherency:	ColValue.Formatted = ::GetImageCoherencyString(pModule); break;
					case eCritical:			ColValue.Formatted = pProcess->IsCriticalProcess() ? tr("Critical") : ""; break;

					case ePowerThrottling:	ColValue.Formatted = pProcess->IsPowerThrottled() ? tr("Yes") : ""; break;
					case eSubsystem:		ColValue.Formatted = ::GetSubsystemString(pProcess); break;
					case eDPI_Awareness:	ColValue.Formatted = ::GetDPIAwarenessString(pProcess); break;
					case eProtection:		ColValue.Formatted = ::GetProcessProtectionString(pProcess); break;
					case eOS_Context:		ColValue.Formatted = ::GetOsContextString(pProcess); break;
					case eNetUsage:			ColValue.Formatted = ::GetNetworkUsageString(pProcess); break;

					case eIO_ReadBytes:
					case eIO_WriteBytes:
					case eIO_OtherBytes:

					case eReceiveBytes:
					case eSendBytes:

					case eReadBytes:
					case eWriteBytes:
												if(Value.type() != QVariant::String) ColValue.Formatted = FormatSize(Value.toULongLong()); break; 

					case eIO_ReadBytesDelta:
					case eIO_WriteBytesDelta:
					case eIO_OtherBytesDelta:

					case eReceiveBytesDelta:
					case eSendBytesDelta:

					case eReadBytesDelta:
					case eWriteBytesDelta:
												if(Value.type() != QVariant::String) ColValue.Formatted = FormatSizeEx(Value.toULongLong(), bClearZeros); break; 

					case eIO_TotalRate:
					case eIO_ReadRate:
					case eIO_WriteRate:
					case eIO_OtherRate:
					case eReceiveRate:
					case eSendRate:
					case eNet_TotalRate:
					case eReadRate:
					case eWriteRate:
					case eDisk_TotalRate:
												if(Value.type() != QVariant::String) ColValue.Formatted = FormatRateEx(Value.toULongLong(), bClearZeros); break; 


				}
			}


			if(!Highlights.isEmpty() && CurIntValue != 0)
			{
				QList<QPair<quint64, SProcessNode*> >& List = Highlights[section];

				if (List.isEmpty() || List.last().first == CurIntValue)
					List.append(qMakePair(CurIntValue, pNode));
				else if (List.last().first < CurIntValue || List.count() < iHighlightMax)
				{
					int i = 0;
					for (; i < List.size(); i++)
					{
						if (List.at(i).first < CurIntValue)
							break;
					}
					List.insert(i, qMakePair(CurIntValue, pNode));

					while (List.count() > iHighlightMax)
						List.removeLast();
				}
			}


			if(State != (Changed != 0))
			{
				if(State && Index.isValid())
					emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), section-1, pNode));
				State = (Changed != 0);
				Col = section;
			}
			if(Changed == 1)
				Changed = 0;
		}
		if(State && Index.isValid())
			emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount()-1, pNode));
	}

	if (!Highlights.isEmpty())
	{
		for (int section = eProcess; section < columnCount(); section++)
		{
			QList<QPair<quint64, SProcessNode*> >& List = Highlights[section];

			for (int i = 0; i < List.size(); i++)
				List.at(i).second->Bold.insert(section);
		}
	}

	CTreeItemModel::Sync(New, Old);

	if (m_bMultiUser || m_bMultiMachine)
		UpdateBranches(m_Root, bClearZeros);

	//for (QMap<QList<QVariant>, QList<STreeNode*> >::const_iterator I = New.begin(); I != New.end(); I++)
	//{
	//	foreach(STreeNode* pNode, I.value())
	//	{
	//		QModelIndex Index = Find(m_Root, pNode);
	//		if(Index.isValid())
	//			AllIndexes.append(Index);
	//	}
	//}
	//m_AllIndexes = AllIndexes;

	return Added;
}

QVariant CProcessModel::NodeData(STreeNode* pNode, int role, int section) const
{
    switch(role)
	{
		case Qt::FontRole:
		{
			SProcessNode* pProcessNode = static_cast<SProcessNode*>(pNode);
			if (pProcessNode->Bold.contains(section))
			{
				QFont fnt;
				fnt.setBold(true);
				return fnt;
			}
			break;
		}
	}

	return CTreeItemModel::NodeData(pNode, role, section);
}

CProcessModel::ERowKind CProcessModel::GetRowKind(const QModelIndex &index, QString* pKey) const
{
	if (!index.isValid())
		return eProcessRow;

	STreeNode* pNode = static_cast<STreeNode*>(index.internalPointer());
	if (!pNode)
		return eProcessRow;

	QString UserKey;
	if (IsUserBranch(pNode->ID, &UserKey)) {
		if (pKey) *pKey = UserKey;
		return eUserRow;
	}
	if (IsMachineBranch(pNode->ID)) {
		if (pKey) *pKey = pNode->ID.toString();
		return eMachineRow;
	}
	return eProcessRow;
}

//
// Recovered from the branch id, which is the system pointer written out - see
// MkMachineBranch. Checked against the systems currently in view rather than
// cast back blindly, so a stale branch cannot hand out a dangling pointer.
//
CSystemAPI* CProcessModel::SystemFromBranchId(const QString& Key)
{
	foreach(const CSystemPtr& pSystem, CCluster::GetSystems())
	{
		if (!pSystem.isNull() && MkMachineBranch(pSystem.data()).toString() == Key)
			return pSystem.data();
	}
	return NULL;
}

//
// A machine branch stays even with nothing under it, so that a machine which
// is connecting, or answering with an empty list, still has a row rather than
// vanishing and coming back.
//
// Only while the machine level is drawn at all, though. Purge removes an empty
// branch unless it is pinned, so pinning one on a tree that has no machine
// level left it there for good: turning cluster mode off emptied the branches
// of their processes and then kept the empty rows, one per connected machine,
// above a flat list they no longer had anything to do with.
//
QModelIndex CProcessModel::FindMachineBranch(CSystemAPI* pSystem)
{
	if (!m_bMultiMachine || !pSystem)
		return QModelIndex();
	return FindIndex(MkMachineBranch(pSystem));
}

bool CProcessModel::IsBranchPinned(const QVariant& Id) const
{
	return m_bMultiMachine && IsMachineBranch(Id) && SystemFromBranchId(Id.toString()) != NULL;
}

CSystemAPI* CProcessModel::GetBranchSystem(const QModelIndex &index) const
{
	QString Key;
	switch (GetRowKind(index, &Key))
	{
		case eMachineRow:
			break;

		//
		// An account branch belongs to the machine it hangs under, and a menu
		// opened on it has to act on that machine and no other. Asked of the
		// parent rather than parsed out of the account branch's own id, which
		// carries the system only while the machine level exists at all - see
		// MkUserBranch.
		//
		case eUserRow:
			return GetBranchSystem(index.parent());

		default:
			return NULL;
	}

	return SystemFromBranchId(Key);
}

CProcessPtr CProcessModel::GetProcess(const QModelIndex &index) const
{
	if (!index.isValid())
        return CProcessPtr();

	SProcessNode* pNode = static_cast<SProcessNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pProcess;
}

int CProcessModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CProcessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return GetColumHeader(section);
    return QVariant();
}

QString CProcessModel::GetColumHeader(int section) const
{
	switch(section)
	{
		case eProcess:				return tr("Process");
		case ePID:					return tr("PID");
		case ePID_LXSS:				return tr("PID (LXSS)");
		case eParentPID:			return tr("Parent PID");
		case eConsolePID:			return tr("Console PID");
		case eSequenceNumber:		return tr("Seq. number");
		case eStartKey:				return tr("Start key");
		case eCPU:					return tr("CPU");
		case eIO_TotalRate:			return tr("I/O total rate");
		case eStatus:				return tr("Status");
		case ePrivateBytes:			return tr("Private bytes");
		case eUserName:				return tr("User name");
		case eServices:				return tr("Services");
		case eDescription:			return tr("Description");
		case eCompanyName:			return tr("Company name");
		case eVersion:				return tr("Version");
		case eNetUsage:				return tr("Network");

		case eFileName:				return tr("File name");
		case eCommandLine:			return tr("Command line");
		case ePeakPrivateBytes:		return tr("Peak private bytes");
		case eWorkingSet:			return tr("Working set");
		case ePeakWS:				return tr("Peak working set");
		case ePrivateWS:			return tr("Private WS");
		case eSharedWS:				return tr("Shared WS (slow)");
		case eShareableWS:			return tr("Shareable WS (slow)");
		case eShareableCommit:		return tr("Shared commit");
		case eVirtualSize:			return tr("Virtual size");
		case ePeakVirtualSize:		return tr("Peak virtual size");
		case eDebugTotal:			return tr("Debug Messages");
		//case eDebugDelte:			return tr("Debug Message Delta");
		case eSessionID:			return tr("Session ID");
		case eAffinity:				return tr("CPU Affinity");
		case ePriorityClass:		return tr("Priority class");
		case eBasePriority:			return tr("Base priority");
		case ePriorityBoost:		return tr("Priority boost");
		case eGPU_Usage:			return tr("GPU");
		case eGPU_Shared:			return tr("Shared");
		case eGPU_Dedicated:		return tr("Dedicated");
		case eGPU_Adapter:			return tr("GPU Adapter");

		case eThreads:				return tr("Threads");
		case ePeakThreads:			return tr("Peak threads");
		case eHandles:				return tr("Handles");
		case ePeakHandles:			return tr("Peak handles");
		case eWND_Handles:			return tr("Windows");	
		case eGDI_Handles:			return tr("GDI handles");
		case eUSER_Handles:			return tr("USER handles");
		case eIntegrity:			return tr("Integrity");
		case eIO_Priority:			return tr("I/O priority");
		case ePagePriority:			return tr("Page priority");
		case eStartTime:			return tr("Start time");
		case eTotalCPU_Time:		return tr("Total CPU time");
		case eKernelCPU_Time:		return tr("Kernel CPU time");
		case eUserCPU_Time:			return tr("User CPU time");
		case eVerificationStatus:	return tr("Verification status");
		case eVerifiedSigner:		return tr("Verified signer");
		case eUpTime:				return tr("Up Time");
		case eArchitecture:			return tr("Architecture");
		case eElevation:			return tr("Elevation");
		case eWindowTitle:			return tr("Window title");
		case eWindowStatus:			return tr("Window status");
		case eCycles:				return tr("Cycles");
		case eCyclesDelta:			return tr("Cycles delta");
		case eCPU_History:			return tr("CPU graph");
		case eMEM_History:			return tr("Mem. graph");
		case eIO_History:			return tr("I/O graph");
		case eNET_History:			return tr("Net. graph");
		case eGPU_History:			return tr("GPU graph");
		case eVMEM_History:			return tr("V. Mem. graph");
		case eMitigations:			return tr("Mitigations");
		case eImageCoherency:		return tr("Image coherency");
		case eVirtualized:			return tr("Virtualized");
		case eContextSwitches:		return tr("Context switches");
		case eContextSwitchesDelta:	return tr("Context switches delta");
		case ePageFaults:			return tr("Page faults");
		case ePageFaultsDelta:		return tr("Page faults delta");
		case eHardFaults:			return tr("Hard faults");
		case eHardFaultsDelta:		return tr("Hard faults delta");

		// IO
		case eIO_Reads:				return tr("I/O reads");
		case eIO_Writes:			return tr("I/O writes");
		case eIO_Other:				return tr("I/O other");
		case eIO_ReadBytes:			return tr("I/O read bytes");
		case eIO_WriteBytes:		return tr("I/O write bytes");
		case eIO_OtherBytes:		return tr("I/O other bytes");
		//case eIO_TotalBytes:		return tr("I/O total bytes");
		case eIO_ReadsDelta:		return tr("I/O reads delta");
		case eIO_WritesDelta:		return tr("I/O writes delta");
		case eIO_OtherDelta:		return tr("I/O other delta");
		//case eIO_TotalDelta:		return tr("I/O total delta");
		case eIO_ReadBytesDelta:	return tr("I/O read bytes delta");
		case eIO_WriteBytesDelta:	return tr("I/O write bytes delta");
		case eIO_OtherBytesDelta:	return tr("I/O other bytes delta");
		//case eIO_TotalBytesDelta:	return tr("I/O total bytes delta");
		case eIO_ReadRate:			return tr("I/O read rate");
		case eIO_WriteRate:			return tr("I/O write rate");
		case eIO_OtherRate:			return tr("I/O other rate");
		//case eIO_TotalRate:		return tr("I/O total rate");
		case eOS_Context:			return tr("OS context");
		case eTLS:					return tr("Thread local storage");
		case ePagedPool:			return tr("Paged pool");
		case ePeakPagedPool:		return tr("Peak paged pool");
		case eNonPagedPool:			return tr("Non-paged pool");
		case ePeakNonPagedPool:		return tr("Peak non-paged pool");
		case eMinimumWS:			return tr("Minimum working set");
		case eMaximumWS:			return tr("Maximum working set");
		case ePrivateBytesDelta:	return tr("Private bytes delta");
		case eSubsystem:			return tr("Subsystem"); // WSL or Wine
		case eOomScore:				return tr("OOM score");
		case eOomScoreAdj:			return tr("OOM adjust");
		case eContainer:			return tr("Container");
		case eConfinement:			return tr("Confinement");
		case eInotifyWatches:		return tr("Inotify watches");
		case eCGroup:				return tr("Control group");
		case ePackageName:			return tr("Package name");
		case eAppID:				return tr("App ID");
		case eDPI_Awareness:		return tr("DPI awareness");
		case eTimeStamp:			return tr("Time stamp");
		case eFileModifiedTime:		return tr("File modified time");
		case eFileSize:				return tr("File size");
		case eJobObjectID:			return tr("Job Object ID");
		case eProtection:			return tr("Protection");
		case eDesktop:				return tr("Desktop");
		case eCritical:				return tr("Critical Process");

		case ePowerThrottling:		return tr("Power throttling");
		case eRunningTime:			return tr("Running Time");
		case eSuspendedTime:		return tr("Suspended Time");
		case eHangCount:			return tr("Hang Count");
		case eGhostCount:			return tr("Ghost Count");

		case eErrorMode:			return tr("Error mode");
		case eCodePage:				return tr("Code page");
		case eReferences:			return tr("References");
		case eGrantedAccess:		return tr("Granted access");
		// Network IO
		case eNet_TotalRate:		return tr("Network total rate");
		case eReceives:				return tr("Network receives");
		case eSends:				return tr("Network sends");
		case eReceiveBytes:			return tr("Network receive bytes");
		case eSendBytes:			return tr("Network send bytes");
		//case eTotalBytes:			return tr("Network Total bytes");
		case eReceivesDelta:		return tr("Network receives delta");
		case eSendsDelta:			return tr("Network sends delta");
		case eReceiveBytesDelta:	return tr("Network receive bytes delta");
		case eSendBytesDelta:		return tr("Network send bytes delta");
		//case eTotalBytesDelta:	return tr("Network total bytes delta");
		case eReceiveRate:			return tr("Network receive rate");
		case eSendRate:				return tr("Network send rate");

		// Disk IO
		case eDisk_TotalRate:		return tr("Disk total rate");
		case eReads:				return tr("Disk reads");
		case eWrites:				return tr("Disk writes");
		case eReadBytes:			return tr("Disk read bytes");
		case eWriteBytes:			return tr("Disk write bytes");
		//case eTotalBytes:			return tr("Disk total bytes");
		case eReadsDelta:			return tr("Disk reads delta");
		case eWritesDelta:			return tr("Disk writes delta");
		case eReadBytesDelta:		return tr("Disk read bytes delta");
		case eWriteBytesDelta:		return tr("Disk write bytes delta");
		//case eTotalBytesDelta:		return tr("Disk total bytes delta");
		case eReadRate:				return tr("Disk read rate");
		case eWriteRate:			return tr("Disk write rate");
	}
	return "";
}

QVariant CProcessModel::GetDefaultIcon() const 
{ 
	return g_ExeIcon;
}
