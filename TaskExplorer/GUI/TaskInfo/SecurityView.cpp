#include "stdafx.h"
#include "../TaskExplorer.h"
#include "SecurityView.h"
#include "../../API/Linux/ProcFs.h"
#include "../../API/Linux/LinuxHelper.h"
#include "../../../MiscHelpers/Common/Common.h"

CSecurityView::CSecurityView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QGridLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	m_pList = new CPanelWidgetEx();
	m_pList->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pList->GetView())->setHeaderLabels(tr("Property|Value").split("|"));
	m_pList->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pList->GetView()->setSortingEnabled(false);
	m_pMainLayout->addWidget(m_pList, 0, 0);

	AddPanelItemsToMenu();

	m_pList->GetView()->header()->restoreState(theConf->GetBlob(objectName() + "/SecurityView_Columns"));
}

CSecurityView::~CSecurityView()
{
	theConf->SetBlob(objectName() + "/SecurityView_Columns", m_pList->GetView()->header()->saveState());
}

void CSecurityView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}

void CSecurityView::SetValue(const QString& Group, const QString& Name, const QString& Value)
{
	QTreeWidgetItem* pGroup = m_Groups.value(Group);
	if (!pGroup)
	{
		pGroup = new QTreeWidgetItem();
		pGroup->setText(0, Group);
		pGroup->setFirstColumnSpanned(true);
		m_pList->GetTree()->addTopLevelItem(pGroup);
		pGroup->setExpanded(true);
		m_Groups.insert(Group, pGroup);
	}

	const QString Key = Group + "/" + Name;
	m_LiveKeys.insert(Key);

	QTreeWidgetItem* pItem = m_Items.value(Key);
	if (!pItem)
	{
		pItem = new QTreeWidgetItem();
		pItem->setText(0, Name);
		pGroup->addChild(pItem);
		m_Items.insert(Key, pItem);
	}

	if (pItem->text(1) != Value)
		pItem->setText(1, Value);
}

void CSecurityView::PruneStale()
{
	for (auto I = m_Items.begin(); I != m_Items.end(); )
	{
		if (m_LiveKeys.contains(I.key())) { ++I; continue; }
		delete I.value();
		I = m_Items.erase(I);
	}

	for (auto I = m_Groups.begin(); I != m_Groups.end(); )
	{
		if (I.value()->childCount() > 0) { ++I; continue; }
		delete I.value();
		I = m_Groups.erase(I);
	}
}

void CSecurityView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	QSharedPointer<CLinuxProcess> pProcess;
	if (Processes.count() == 1)
	{
		setEnabled(true);
		pProcess = Processes.first().objectCast<CLinuxProcess>();
	}
	else
		setEnabled(false);

	m_pCurProcess = pProcess;

	Refresh();
}

void CSecurityView::Refresh()
{
	m_LiveKeys.clear();

	if (m_pCurProcess.isNull())
	{
		PruneStale();
		return;
	}

	const quint64 Pid = m_pCurProcess->GetProcessId();
	const ProcFs::SProcSecurity Security = ProcFs::ReadProcSecurity(Pid);

	if (!Security.Valid)
	{
		SetValue(tr("Identity"), tr("Status"), tr("The process has exited."));
		PruneStale();
		return;
	}

	// ---- identity ----

	SetValue(tr("Identity"), tr("User"), tr("%1 (uid %2)")
		.arg(m_pCurProcess->GetUserName()).arg(m_pCurProcess->GetUid()));
	SetValue(tr("Identity"), tr("Group id"), QString::number(m_pCurProcess->GetGid()));

	const QString Container = m_pCurProcess->GetContainer();
	SetValue(tr("Identity"), tr("Container"), Container.isEmpty() ? tr("none (host)") : Container);

	// ---- confinement ----

	//
	// An absent label means no LSM is active at all, which is materially
	// different from an active LSM that has decided not to confine this
	// process - so the two are worded differently rather than both blank.
	//
	const QString Confinement = Security.Confinement;
	if (Confinement.isEmpty())
		SetValue(tr("Confinement"), tr("Profile"), tr("no LSM active"));
	else if (Confinement == "unconfined")
		SetValue(tr("Confinement"), tr("Profile"), tr("unconfined"));
	else
		SetValue(tr("Confinement"), tr("Profile"), Confinement);

	SetValue(tr("Confinement"), tr("Seccomp"), Security.SeccompFilters
		? tr("%1 (%2 filters)").arg(ProcFs::SeccompModeToString(Security.Seccomp)).arg(Security.SeccompFilters)
		: ProcFs::SeccompModeToString(Security.Seccomp));

	//
	// no_new_privs means the process can never gain privileges through execve,
	// so a setuid binary it runs stays unprivileged. It is what makes a sandbox
	// hold across exec.
	//
	SetValue(tr("Confinement"), tr("No new privileges"), Security.NoNewPrivs ? tr("Yes") : tr("No"));

	// ---- capabilities ----

	//
	// Five sets, but they answer different questions and only two are usually
	// worth reading:
	//
	//   Effective - what the process can use right now
	//   Permitted - what it may re-enable at will, so effectively the same power
	//   Bounding  - the ceiling; nothing outside it can ever be acquired
	//   Inheritable / Ambient - what survives an execve
	//
	auto Describe = [](quint64 Mask, quint64 FullSet) -> QString {
		if (Mask == 0)
			return CSecurityView::tr("none");
		// A full set is how an unconfined root process looks; listing all
		// forty names there would bury the cases that matter.
		if (FullSet && Mask == FullSet)
			return CSecurityView::tr("all");
		return ProcFs::DecodeCapabilities(Mask).join(", ");
	};

	// The bounding set of an unrestricted process is the full set the running
	// kernel supports, so it is the right reference for "all".
	const quint64 FullSet = Security.CapBnd;

	SetValue(tr("Capabilities"), tr("Effective"), Describe(Security.CapEff, FullSet));
	SetValue(tr("Capabilities"), tr("Permitted"), Describe(Security.CapPrm, FullSet));
	SetValue(tr("Capabilities"), tr("Inheritable"), Describe(Security.CapInh, FullSet));
	SetValue(tr("Capabilities"), tr("Ambient"), Describe(Security.CapAmb, FullSet));
	SetValue(tr("Capabilities"), tr("Bounding"), Describe(Security.CapBnd, 0));

	// ---- namespaces ----

	//
	// Shown against pid 1's, because the inode numbers themselves mean nothing
	// to a reader - what matters is whether this process shares the host's
	// namespace or has one of its own.
	//
	static const ProcFs::SNamespaces Host = ProcFs::ReadNamespaces(1);
	const ProcFs::SNamespaces Namespaces = m_pCurProcess->GetNamespaces();

	auto ShowNamespace = [this](const QString& Name, quint64 Value, quint64 HostValue) {
		if (!Value)
			SetValue(tr("Namespaces"), Name, tr("not readable"));
		else if (!HostValue || Value == HostValue)
			SetValue(tr("Namespaces"), Name, tr("host"));
		else
			SetValue(tr("Namespaces"), Name, tr("private (%1)").arg(Value));
	};

	ShowNamespace(tr("pid"), Namespaces.Pid, Host.Pid);
	ShowNamespace(tr("net"), Namespaces.Net, Host.Net);
	ShowNamespace(tr("mnt"), Namespaces.Mnt, Host.Mnt);
	ShowNamespace(tr("user"), Namespaces.User, Host.User);
	ShowNamespace(tr("uts"), Namespaces.Uts, Host.Uts);
	ShowNamespace(tr("ipc"), Namespaces.Ipc, Host.Ipc);
	ShowNamespace(tr("cgroup"), Namespaces.CGroup, Host.CGroup);

	// ---- out of memory killer ----

	SetValue(tr("Out of memory killer"), tr("Score"), QString::number(m_pCurProcess->GetOomScore()));
	SetValue(tr("Out of memory killer"), tr("Adjustment"), QString::number(m_pCurProcess->GetOomScoreAdj()));

	// ---- resources ----

	SetValue(tr("Resources"), tr("inotify watches"), FormatNumber(m_pCurProcess->GetInotifyWatches()));

	PruneStale();
}
