#include "stdafx.h"
#include "../TaskExplorer.h"
#include "CGroupView.h"
#include "../../API/Linux/ProcFs.h"
#include "../../../MiscHelpers/Common/Common.h"

CCGroupView::CCGroupView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QGridLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	int row = 0;

	m_pPathLabel = new QLabel(tr("Control group:"));
	m_pMainLayout->addWidget(m_pPathLabel, row, 0);

	//
	// Read only, but a line edit rather than a label: these paths are long and
	// worth being able to select and copy.
	//
	m_pPath = new QLineEdit();
	m_pPath->setReadOnly(true);
	m_pMainLayout->addWidget(m_pPath, row++, 1);

	m_pList = new CPanelWidgetEx();
	m_pList->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pList->GetView())->setHeaderLabels(tr("Property|Value").split("|"));
	m_pList->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pList->GetView()->setSortingEnabled(false);
	m_pMainLayout->addWidget(m_pList, row++, 0, 1, 2);

	AddPanelItemsToMenu();

	m_pList->GetView()->header()->restoreState(theConf->GetBlob(objectName() + "/CGroupView_Columns"));
}

CCGroupView::~CCGroupView()
{
	theConf->SetBlob(objectName() + "/CGroupView_Columns", m_pList->GetView()->header()->saveState());
}

void CCGroupView::OnResetColumns()
{
}

void CCGroupView::OnColumnsChanged()
{
}

void CCGroupView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}

void CCGroupView::SetValue(const QString& Group, const QString& Name, const QString& Value)
{
	//
	// Grouped under a top level heading, so that memory, cpu, pids and pressure
	// stay visually separated rather than becoming one long list.
	//
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

	// Only touch the text when it actually changed, so a value being read does
	// not flicker on every refresh.
	if (pItem->text(1) != Value)
		pItem->setText(1, Value);
}

void CCGroupView::PruneStale()
{
	//
	// Controllers can be enabled or disabled while a process runs, and the
	// selection can move to a process in a different cgroup, so rows that no
	// longer apply have to go rather than sit there stale.
	//
	for (auto I = m_Items.begin(); I != m_Items.end(); )
	{
		if (m_LiveKeys.contains(I.key()))
		{
			++I;
			continue;
		}

		delete I.value();
		I = m_Items.erase(I);
	}

	for (auto I = m_Groups.begin(); I != m_Groups.end(); )
	{
		if (I.value()->childCount() > 0)
		{
			++I;
			continue;
		}

		delete I.value();
		I = m_Groups.erase(I);
	}
}

void CCGroupView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	QSharedPointer<CLinuxProcess> pProcess;
	if (Processes.count() == 1)
	{
		setEnabled(true);
		pProcess = Processes.first().objectCast<CLinuxProcess>();
	}
	else
	{
		// A cgroup is a property of one process; showing several at once would
		// have to merge unrelated hierarchies.
		setEnabled(false);
	}

	m_pCurProcess = pProcess;

	Refresh();
}

void CCGroupView::Refresh()
{
	m_LiveKeys.clear();

	if (m_pCurProcess.isNull())
	{
		m_pPath->clear();
		PruneStale();
		return;
	}

	const QString Path = m_pCurProcess->GetCGroupPath();
	if (m_pPath->text() != Path)
		m_pPath->setText(Path);

	if (Path.isEmpty())
	{
		//
		// Kernel threads are in the root cgroup and have no accounting of their
		// own; a v1-only system has no unified path either.
		//
		SetValue(tr("Control group"), tr("Status"), tr("This process is not in a unified (v2) control group."));
		PruneStale();
		return;
	}

	const ProcFs::SCGroupStats Stats = ProcFs::ReadCGroupStats(Path);

	SetValue(tr("Control group"), tr("Path"), Path);
	if (!Stats.Controllers.isEmpty())
		SetValue(tr("Control group"), tr("Controllers"), Stats.Controllers.join(", "));

	if (!Stats.Valid)
	{
		SetValue(tr("Control group"), tr("Status"), tr("No controllers are enabled for this group."));
		PruneStale();
		return;
	}

	//
	// Memory. "max" in the kernel files means no limit, which ReadCGroupStats
	// normalises to 0 - shown as "unlimited" rather than as a limit of nothing.
	//
	if (Stats.MemoryCurrent || Stats.MemoryPeak)
	{
		SetValue(tr("Memory"), tr("Current"), FormatSize(Stats.MemoryCurrent));
		SetValue(tr("Memory"), tr("Peak"), FormatSize(Stats.MemoryPeak));
		SetValue(tr("Memory"), tr("Limit"), Stats.MemoryMax ? FormatSize(Stats.MemoryMax) : tr("unlimited"));

		// memory.high throttles rather than kills, so it is worth showing
		// separately from the hard limit.
		if (Stats.MemoryHigh)
			SetValue(tr("Memory"), tr("Throttle above"), FormatSize(Stats.MemoryHigh));

		if (Stats.MemoryMax)
		{
			SetValue(tr("Memory"), tr("Used of limit"),
				tr("%1%").arg(100.0 * Stats.MemoryCurrent / Stats.MemoryMax, 0, 'f', 1));
		}

		SetValue(tr("Memory"), tr("Swap"), FormatSize(Stats.MemorySwapCurrent));
		if (Stats.MemorySwapMax)
			SetValue(tr("Memory"), tr("Swap limit"), FormatSize(Stats.MemorySwapMax));
	}

	// CPU. The counters are cumulative microseconds since the group was created.
	if (Stats.CpuUsageUs)
	{
		SetValue(tr("CPU"), tr("Total time"), FormatTime(Stats.CpuUsageUs / 1000, true));
		SetValue(tr("CPU"), tr("User time"), FormatTime(Stats.CpuUserUs / 1000, true));
		SetValue(tr("CPU"), tr("System time"), FormatTime(Stats.CpuSystemUs / 1000, true));
	}

	//
	// Throttling only appears once a cpu.max limit exists. When it does, the
	// number of throttled periods is the interesting one: it says the group hit
	// its quota and was stopped, which looks like unexplained slowness from
	// inside the process.
	//
	if (Stats.NrPeriods)
	{
		SetValue(tr("CPU"), tr("Periods"), FormatNumber(Stats.NrPeriods));
		SetValue(tr("CPU"), tr("Throttled periods"),
			tr("%1 (%2%)").arg(FormatNumber(Stats.NrThrottled))
			              .arg(100.0 * Stats.NrThrottled / Stats.NrPeriods, 0, 'f', 1));
		SetValue(tr("CPU"), tr("Throttled time"), FormatTime(Stats.ThrottledUs / 1000, true));
	}

	if (Stats.PidsCurrent)
	{
		SetValue(tr("Tasks"), tr("Current"), FormatNumber(Stats.PidsCurrent));
		SetValue(tr("Tasks"), tr("Limit"), Stats.PidsMax ? FormatNumber(Stats.PidsMax) : tr("unlimited"));
	}

	if (Stats.IoReadBytes || Stats.IoWriteBytes)
	{
		SetValue(tr("I/O"), tr("Read"), FormatSize(Stats.IoReadBytes));
		SetValue(tr("I/O"), tr("Written"), FormatSize(Stats.IoWriteBytes));
	}

	//
	// Per-cgroup pressure: how much time this group's tasks spent stalled. This
	// is the number that distinguishes "the machine is busy" from "this group
	// in particular is being starved", which no Windows job object reports.
	//
	static const struct { const char* Resource; const char* Label; } Resources[] = {
		{ "cpu",	QT_TR_NOOP("CPU") },
		{ "memory",	QT_TR_NOOP("Memory") },
		{ "io",		QT_TR_NOOP("I/O") },
	};

	for (size_t i = 0; i < sizeof(Resources) / sizeof(Resources[0]); i++)
	{
		const ProcFs::SPressure Pressure = ProcFs::ReadCGroupPressure(Path, Resources[i].Resource);
		if (!Pressure.Valid)
			continue;

		SetValue(tr("Pressure (some, 10s / 60s)"), tr(Resources[i].Label),
			tr("%1% / %2%").arg(Pressure.SomeAvg10, 0, 'f', 2).arg(Pressure.SomeAvg60, 0, 'f', 2));
	}

	PruneStale();
}
