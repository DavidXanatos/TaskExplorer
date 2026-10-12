#include "stdafx.h"
#include "../../API/Cluster.h"
#include "../../API/RemoteApi.h"
#include "SystemView.h"
#include "../TaskExplorer.h"
#include "../TaskStrings.h"
#include "../StatsView.h"
#include "./KernelInfo/DriversView.h"
#ifdef WIN32
#include "./KernelInfo/PoolView.h"
#include "./KernelInfo/NtObjectView.h"
#include "./KernelInfo/AtomView.h"
#include "./KernelInfo/RunObjView.h"
#include "RpcView.h"
#endif

CSystemView::CSystemView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	m_pScrollArea = new QScrollArea();
	m_pMainLayout->addWidget(m_pScrollArea);

	m_pInfoWidget = new QWidget();
	m_pScrollArea->setFrameShape(QFrame::NoFrame);
	m_pScrollArea->setWidgetResizable(true);
	m_pScrollArea->setWidget(m_pInfoWidget);
	QPalette pal = m_pScrollArea->palette();
	pal.setColor(QPalette::Window, Qt::transparent);
	m_pScrollArea->setPalette(pal);

	m_pInfoLayout = new QVBoxLayout();
	m_pInfoWidget->setLayout(m_pInfoLayout);

	m_pSystemBox = new QGroupBox(tr("System"));

	//
	// Exactly as tall as its contents, never taller.
	//
	// A group box grows by default, so in the vertical layout below it took a
	// share of whatever height was going spare and sat there mostly empty - a
	// caption block the height of the panel. The stretch row inside it keeps the
	// lines together at the top, but that only decides where the empty space
	// goes; this decides that there is none, and the tabs underneath get it
	// instead.
	//
	m_pSystemBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	m_pInfoLayout->addWidget(m_pSystemBox);

	m_pSystemLayout = new QGridLayout();
	m_pSystemLayout->setSpacing(2);
	m_pSystemBox->setLayout(m_pSystemLayout);

	int row = 0;

	m_pIcon = new QLabel();
	m_pSystemLayout->addWidget(m_pIcon, 0, 0, 4, 1);

	m_pSystemName = new QLabel();
	m_pSystemName->setSizePolicy(QSizePolicy::Expanding, m_pSystemName->sizePolicy().verticalPolicy());
	m_pSystemLayout->addWidget(m_pSystemName, row++, 1, 1, 2);
	
	m_pSystemLayout->addWidget(new QLabel(tr("Type:")), row, 1);
	m_pSystemType = new QLabel();
	m_pSystemLayout->addWidget(m_pSystemType, row++, 2);

	m_pSystemLayout->addWidget(new QLabel(tr("Version:")), row, 1);
	m_pSystemVersion = new QLabel();
	m_pSystemVersion->setSizePolicy(QSizePolicy::Expanding, m_pSystemVersion->sizePolicy().verticalPolicy());
	m_pSystemLayout->addWidget(m_pSystemVersion, row++, 2);

	m_pSystemLayout->addWidget(new QLabel(tr("Build:")), row, 1);
	m_pSystemBuild = new QLabel();
	m_pSystemLayout->addWidget(m_pSystemBuild, row++, 2);

	//
	// The daemon answering for this machine, beside the operating system it is
	// running on rather than under it.
	//
	// Side by side because they are two descriptions of the same machine, not
	// one after the other: what it is on the left, who is answering for it on
	// the right, with a rule between them. Stacked, the box grew tall and half
	// of it was empty space to the right of four short lines.
	//
	// Its own widget rather than more rows in the same grid, so that hiding it
	// for a machine read directly is one call and leaves no gaps behind.
	//
	m_pServerWidget = new QWidget();
	QGridLayout* pServerLayout = new QGridLayout();
	pServerLayout->setContentsMargins(0, 0, 0, 0);
	pServerLayout->setSpacing(2);
	m_pServerWidget->setLayout(pServerLayout);

	//
	// A vertical rule, and the server block beside it.
	//
	// Both span one row further than the operating system's own - the empty
	// stretch row added below - and that extra row is the point. Spanning only
	// the four, the server block is the taller of the two columns and the grid
	// has to find its height somewhere: it shares it out among the four rows it
	// spans, and the operating system's four lines drift apart to match a block
	// that has six. Given the stretch row to grow into, both columns keep their
	// natural line spacing and the slack collects underneath.
	//
	// Top aligned for the same reason, so neither column centres itself in what
	// it was given.
	//
	QFrame* pLine = new QFrame();
	pLine->setFrameShape(QFrame::VLine);
	pLine->setFrameShadow(QFrame::Sunken);
	m_pSystemLayout->addWidget(pLine, 0, 3, row + 1, 1);
	m_pSystemLayout->addWidget(m_pServerWidget, 0, 4, row + 1, 1, Qt::AlignTop);

	//
	// Both halves share the width. Without this the operating system's value
	// column takes everything and the server block is pushed against the right
	// edge, which reads as two panels rather than one.
	//
	m_pSystemLayout->setColumnStretch(2, 1);
	m_pSystemLayout->setColumnStretch(4, 1);

	//
	// One empty row below everything, and it takes all the slack.
	//
	// A QGridLayout with nothing stretchable in it shares spare height out among
	// its rows, so when the box was taller than its contents the four lines beside
	// the logo drifted apart until they spanned the whole height. Giving the row
	// after them the stretch instead keeps them together at the top and lets the
	// box grow underneath, which is what a caption block should do.
	//
	m_pSystemLayout->setRowStretch(row, 1);

	int srow = 0;

	//
	// Four rows, not six.
	//
	// The protocol version and the byte count are worth having and not worth a
	// line each - nobody reads them at a glance, and two extra rows made this
	// block half again as tall as the operating system's beside it, which set
	// the height of the whole box.
	//
	// So each rides with the row it belongs to. The protocol goes in the tooltip
	// of the server version, which is the thing it describes; the traffic goes
	// in brackets after the connection time, because how long a connection has
	// been up and how much has come over it are read together.
	//
	//
	// pLabel is kept only where something later needs the caption itself - the
	// two rows that carry a tooltip, which has to go on both halves or it is
	// only there for whichever half the pointer happens to be over.
	//
	struct { const char* Label; QLabel** pValue; QLabel** pLabel; } Rows[] = {
		{ QT_TR_NOOP("Task Server:"),	&m_pServerVersion,		&m_pServerVersionRow },
		{ QT_TR_NOOP("Endpoint:"),		&m_pServerAddress,		NULL },
		{ QT_TR_NOOP("Machine ID:"),	&m_pServerMachineId,	NULL },
		{ QT_TR_NOOP("Connected:"),		&m_pServerUptime,		&m_pServerUptimeRow },
	};
	for (int i = 0; i < sizeof(Rows) / sizeof(Rows[0]); i++)
	{
		QLabel* pLabel = new QLabel(tr(Rows[i].Label));
		if (Rows[i].pLabel)
			*Rows[i].pLabel = pLabel;
		pServerLayout->addWidget(pLabel, srow, 0);
		*Rows[i].pValue = new QLabel();
		(*Rows[i].pValue)->setTextInteractionFlags(Qt::TextSelectableByMouse);
		pServerLayout->addWidget(*Rows[i].pValue, srow++, 1);
	}

	pServerLayout->setColumnStretch(1, 1);

	//
	// Hidden until there is a daemon to describe. The rule goes with it - a
	// line down the middle of a box with nothing on the other side of it is
	// worse than no line.
	//
	m_pServerLine = pLine;
	m_pServerWidget->setVisible(false);
	m_pServerLine->setVisible(false);


	//m_pSystemLayout->addWidget(new QLabel(tr("Up time:")), row, 0);
	//m_pUpTime = new QLabel();
	//m_pSystemLayout->addWidget(m_pUpTime, row++, 1, 1, 2);

	//m_pSystemLayout->addWidget(new QLabel(tr("Host name:")), row, 0);
	//m_pHostName = new QLabel();
	//m_pSystemLayout->addWidget(m_pHostName, row++, 1, 1, 2);

	//m_pSystemLayout->addWidget(new QLabel(tr("User name:")), row, 0);
	//m_pUserName = new QLabel();
	//m_pSystemLayout->addWidget(m_pUserName, row++, 1, 1, 2);

	//m_pSystemLayout->addWidget(new QLabel(tr("System directory:")), row++, 0, 1, 3);
	//m_pSystemDir = new QLineEdit();
	//m_pSystemDir->setReadOnly(true);
	//m_pSystemLayout->addWidget(m_pSystemDir, row++, 0, 1, 3);

	//m_pSystemLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), row, 0);

	//m_pStatsView = new CStatsView(CStatsView::eSystem, this);
	//m_pStatsView->setSizePolicy(m_pStatsView->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
	//m_pInfoLayout->addWidget(m_pStatsView);

	/*QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pProcessLayout->addWidget(pSpacer, row, 1);*/
	//m_pInfoLayout->addWidget(pSpacer);

	m_pTabs = new QTabWidget();
	//m_pTabs->setDocumentMode(true);
	//m_pTabs->setTabPosition(QTabWidget::South);
	//m_pTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	m_pInfoLayout->addWidget(m_pTabs);
	
	m_pStatsView = new CStatsView(CStatsView::eSystem, this);
	m_pTabs->addTab(m_pStatsView, tr("Statistics"));

	//
	// Kernel modules, on both kinds of machine and for remote ones too.
	//
	// A Windows driver and a Linux kernel module are the same thing under two
	// names - code loaded into the kernel - and both collectors already build
	// the list: CWindowsAPI from the loaded-module enumeration, CLinuxAPI from
	// /proc/modules. One tab rather than two, with the columns that only one of
	// them can fill hidden on the other; see CDriversView::OnResetColumns.
	//
	// It is deliberately created before the #ifdef below. The four views after
	// it read Windows kernel tables through a downcast of theSystem and can only
	// ever show this machine, which is what UpdateAvailability greys them for -
	// this one has no such limit and must not be caught by it.
	//
	m_pDriversView = new CDriversView(this);
	m_DriversTab = m_pTabs->addTab(m_pDriversView, tr("Kernel Modules"));

#ifdef WIN32
	m_pPoolView = new CPoolView(this);
	m_pTabs->addTab(m_pPoolView, tr("Pool Table"));

	m_pNtObjectView = new CNtObjectView(this);
	m_pTabs->addTab(m_pNtObjectView, tr("Nt Objects"));

	m_pAtomView = new CAtomView(this);
	m_pTabs->addTab(m_pAtomView, tr("Atom Table"));

	m_pRunObjView = new CRunObjView(this);
	m_pTabs->addTab(m_pRunObjView, tr("Running Objects"));

	//
	// RPC endpoints belong with the four above rather than beside CPU and Disk,
	// which is where they used to be. They are the same kind of thing: a table
	// the machine keeps, read here through a downcast of theSystem, only ever
	// this machine's. As a top-level tab it was the one entry in the System Tabs
	// menu that could not be shown for a remote machine, and it took a full tab
	// of strip width to say so.
	//
	m_pRpcView = new CRpcView(this);
	m_pTabs->addTab(m_pRpcView, tr("RPC Endpoints"));
#endif

	m_pTabs->setCurrentIndex(theConf->GetValue(objectName() + "/SystemView_Tab").toInt());

	connect(theGUI, SIGNAL(ViewSystemChanged()), this, SLOT(UpdateAvailability()));
	UpdateAvailability();
}

CSystemView::~CSystemView()
{
	theConf->SetValue(objectName() + "/SystemView_Tab", m_pTabs->currentIndex());
}


//
// The daemon answering for this machine, when one is.
//
// Everything here came out of the handshake or is counted on this side, so it
// costs nothing to show and needs no request of its own. What is deliberately
// *not* here is who the server thinks we are: the local transport tells the
// server who connected but says nothing back, and over the network that answer
// arrives with the PSK identity, which does not exist yet. An invented "unknown"
// row would suggest the question had been asked.
//
void CSystemView::ShowServer(IRemoteSystem* pRemote)
{
	m_pServerWidget->setVisible(pRemote != NULL);
	m_pServerLine->setVisible(pRemote != NULL);
	if (!pRemote)
		return;

	//
	// The protocol rides in the tooltip. On the label as well as the value, so
	// that hovering anywhere on the row shows it.
	//
	QString Version = pRemote->GetServerVersion();
	if (Version.isEmpty())
		Version = tr("(not reported)");

	//
	// And whether that server has the kernel driver, in the same badge the title
	// bar uses for this machine's own.
	//
	// It belongs on this line rather than a row of its own because it is a
	// property of the daemon, not of the machine: two daemons on one machine
	// could differ, and what the numbers on screen are worth depends on the one
	// answering. Without it the only way to find out was to read the server's
	// log on the far machine.
	//
	CSystemAPI::SKernelDriver Driver;
	if (CSystemAPI* pSystem = pRemote->AsSystem())
		Driver = pSystem->GetKernelDriver();

	const QString Badge = CTaskExplorer::GetKernelBadge(Driver);
	if (!Badge.isEmpty())
		Version += " " + Badge;

	m_pServerVersion->setText(Version);

	QString Protocol = tr("Protocol %1, %2 commands")
		.arg(pRemote->GetProtocolVersion()).arg(pRemote->GetFeatureCount());

	//
	// The badge is three characters and says nothing about what it cost to earn;
	// the tooltip spells it out, including the case the badge cannot show - a
	// daemon with no driver at all, where the badge is simply absent.
	//
	Protocol += "\n" + (Driver.Connected
		? tr("Kernel driver: %1%2").arg(CTaskExplorer::GetKernelLevelString(Driver.Level))
			.arg(Driver.DynDataLoaded ? QString() : tr(", without this kernel's offsets"))
		: tr("Kernel driver: not loaded - this daemon cannot answer for protected processes"));

	m_pServerVersion->setToolTip(Protocol);
	if (m_pServerVersionRow)
		m_pServerVersionRow->setToolTip(Protocol);

	m_pServerAddress->setText(pRemote->IsNetworkTransport()
		? tr("%1 (network)").arg(pRemote->GetAddress())
		: tr("%1 (local)").arg(pRemote->GetAddress()));

	m_pServerMachineId->setText(pRemote->GetMachineId());

	//
	// A connection that is not up says so rather than showing a frozen count.
	// The lists keep whatever they last knew - see CRemoteSystem::DoDisconnect -
	// so without this the panel would look live.
	//
	const quint64 Since = pRemote->GetConnectedSince();
	if (!pRemote->IsConnected() || Since == 0)
		m_pServerUptime->setText(tr("not connected"));
	else
	{
		//
		// How much has come over the connection, in brackets after how long it
		// has been up. Two facts about the same thing, on one line.
		//
		m_pServerUptime->setText(tr("%1 (%2 received)")
			.arg(FormatTime(GetTime() - Since))
			.arg(FormatSize(pRemote->GetBytesReceived())));
	}

	//
	// The clock difference rides in the tooltip of the connection time rather
	// than taking a row.
	//
	// It is the same kind of fact - something about this connection rather than
	// about the machine at the other end - and it is worth almost nothing until
	// it is worth a great deal: zero on every well-behaved link, and the
	// explanation for timestamps that look wrong when a target is minutes out
	// of step. A row that reads "0 ms" for months is a row nobody reads on the
	// day it does not.
	//
	// On both halves of the row, because a tooltip on the value alone is
	// missing for anybody who points at the caption.
	//
	const qint64 Offset = pRemote->GetTimeOffset();
	const QString ClockTip = Offset == 0
		? tr("This machine's clock and the target's agree.")
		: tr("The target's clock is %1 ms %2 this one. Timestamps from it are shown "
			 "on this machine's clock, adjusted by that much.")
			.arg(qAbs(Offset)).arg(Offset > 0 ? tr("ahead of") : tr("behind"));

	m_pServerUptime->setToolTip(ClockTip);
	m_pServerUptimeRow->setToolTip(ClockTip);
}

void CSystemView::UpdateAvailability()
{
	//
	// Everything after "Statistics". Those views hold Windows kernel tables
	// that no wire format carries, and they reach for them through a downcast
	// of theSystem, so they can only ever be the local machine's.
	//
	const bool bLocal = CCluster::GetViewSystem() == theSystem;
	for (int i = 1; i < m_pTabs->count(); i++)
	{
		//
		// Except the kernel module list, which every collector builds and the
		// wire carries - see where it is added above.
		//
		if (i == m_DriversTab)
			continue;

		m_pTabs->setTabEnabled(i, bLocal);
		m_pTabs->setTabToolTip(i, bLocal ? QString() : tr("Only available for the local machine."));
	}

	//
	// Its columns are a property of the machine being looked at, so a saved
	// layout from a machine of the other kind is not the right one to keep.
	//
	m_pDriversView->OnResetColumns();
}

void CSystemView::Refresh()
{
	CSystemPtr pSystem = CCluster::GetViewSystem();

	//
	// Rebuilt when the machine changes rather than only when there is no pixmap
	// yet: the panel follows the selection now, and the previous machine's logo
	// would otherwise sit beside the new one's name.
	//
	if (m_pIconFrom != pSystem.data())
	{
		m_pIconFrom = pSystem.data();
		//
		// Sized by MakeIcon rather than scaled afterwards: scaling a pixmap that
		// already carries a device pixel ratio scales the ratio with it, and the
		// logo came out at half size on a scaled display.
		//
		const QPixmap Logo = ::MakeIcon(pSystem->GetSystemIcon(), 64);
		m_pIcon->setPixmap(Logo);
	}

	m_pSystemName->setText(pSystem->GetSystemName());
	m_pSystemType->setText(pSystem->GetSystemType());
	m_pSystemVersion->setText(::GetSystemVersionString(pSystem.data()));
	m_pSystemBuild->setText(::GetSystemBuildString(pSystem.data()));

	ShowServer(pSystem->GetRemote());

	//m_pUpTime->setText(FormatTime(CCluster::GetViewSystem()->GetUpTime()));
	//m_pHostName->setText(CCluster::GetViewSystem()->GetHostName());
	//m_pUserName->setText(CCluster::GetViewSystem()->GetUserName());
	//m_pSystemDir->setText(CCluster::GetViewSystem()->GetSystemDir());

	m_pStatsView->ShowSystem();

	//
	// The kernel module list, on whichever machine is being looked at. Outside
	// the guard below because the tab is: the four views after it are Windows
	// kernel tables and only exist in a Windows build, this one is built
	// everywhere and serves remote machines too.
	//
	if (m_pTabs->currentWidget() == m_pDriversView)
		m_pDriversView->Refresh();

#ifdef WIN32
	//if(m_pTabs->currentWidget() == m_pPoolView)
		m_pPoolView->Refresh(); // needed for the allocs on win 10
	//else 
    if(m_pTabs->currentWidget() == m_pNtObjectView)
		m_pNtObjectView->Refresh();
	else if(m_pTabs->currentWidget() == m_pAtomView)
		m_pAtomView->Refresh();
	else if(m_pTabs->currentWidget() == m_pRunObjView)
		m_pRunObjView->Refresh();
	else if(m_pTabs->currentWidget() == m_pRpcView)
		m_pRpcView->Refresh();
#endif
}
