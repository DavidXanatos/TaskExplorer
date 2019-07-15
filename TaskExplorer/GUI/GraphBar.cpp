#include "stdafx.h"
#include "GraphBar.h"
#include "TaskExplorer.h"
#include "..\Common\ItemChooser.h"
#ifdef WIN32
#include "..\API\Windows\WindowsAPI.h"
#endif

CGraphBar::CGraphBar()
{
	m_Rows = 0;

	this->setMinimumHeight(50);
	//this->setMaximumHeight(200);

	m_pMainLayout = new QGridLayout();
	this->setLayout(m_pMainLayout);

	m_pMainLayout->setMargin(1);
	m_pMainLayout->setSpacing(2);

	QPalette pal = palette();
	pal.setColor(QPalette::Background, Qt::gray);
	this->setAutoFillBackground(true);
	this->setPalette(pal);


	m_pMenu = new QMenu();
	m_pResetPlot = m_pMenu->addAction(tr("Reset Plot"), this, SLOT(ClearGraphs()));
	m_pResetAll = m_pMenu->addAction(tr("Reset All Plots"), this, SLOT(ClearGraphs()));
	m_pCustomize = m_pMenu->addAction(tr("Customize Plots"), this, SLOT(CustomizeGraphs()));

	m_pLastTipGraph = NULL;

	int Rows = theConf->GetInt("Options/GraphRows", 2);
	QList<EGraph> Graphs;
	QStringList GraphList = theConf->GetStringList("Options/Graphs");
	if (GraphList.isEmpty())
	{
		Graphs.append(eGpuMemPlot);
		Graphs.append(eMemoryPlot);
		Graphs.append(eObjectPlot);
		Graphs.append(eWindowsPlot);
		Graphs.append(eHandledPlot);
		//Graphs.append(eDiskIoPlot);
		Graphs.append(eMMapIoPlot);
		Graphs.append(eFileIoPlot);
		Graphs.append(eSambaPlot);
		//Graphs.append(eClientPlot);
		//Graphs.append(eServerPlot);
		Graphs.append(eRasPlot);
		Graphs.append(eNetworkPlot);
		Graphs.append(eGpuPlot);
		Graphs.append(eCpuPlot);
	}
	else
	{
		foreach(const QString& Graph, GraphList)
			Graphs.append((EGraph)Graph.toInt());
	}

	AddGraphs(Graphs, Rows);
}

CGraphBar::~CGraphBar()
{
	QStringList GraphList;
	foreach(const TGraphPair& Graph, m_Graphs)
		GraphList.append(QString::number(Graph.first));
	theConf->SetValue("Options/GraphRows", m_Rows);
	theConf->SetValue("Options/Graphs", GraphList);
}

void CGraphBar::AddGraphs(QList<EGraph> Graphs, int Rows)
{
	m_Rows = Rows;

	int Count = Graphs.count();
	int Columns = Count / Rows + (Count % Rows ? 1 : 0);

	for (int row = 0; row < Rows; row++)
	{
		for (int column = 0; column < Columns; column++)
		{
			int index = row * Columns + column;
			if (index >= Graphs.count())
				break;

			AddGraph(Graphs.at(index), row, column);
		}
	}

	emit Resized(m_Rows * 40);
}

void CGraphBar::UpdateLengths()
{
	int interval = theConf->GetInt("Options/RefreshInterval", 1000); // 5 minutes default
	int limit = interval ? theConf->GetInt("Options/GraphLength", 300) * 1000 / interval : 300;
	
	foreach(const TGraphPair& Graph, m_Graphs)
		Graph.second->SetLimit(limit);
}

void CGraphBar::AddGraph(EGraph Graph, int row, int column)
{
	CIncrementalPlot* pPlot = new CIncrementalPlot();

	int interval = theConf->GetInt("Options/RefreshInterval", 1000); // 5 minutes default
	int limit = interval ? theConf->GetInt("Options/GraphLength", 300) * 1000 / interval : 300;
	pPlot->SetLimit(limit);

	switch (Graph)
	{
	case eMemoryPlot:
		pPlot->AddPlot("Commited", Qt::green, Qt::SolidLine, true);
		pPlot->AddPlot("Swapped", Qt::red, Qt::SolidLine, true);
		pPlot->AddPlot("Cache", Qt::blue, Qt::SolidLine, true);
		pPlot->AddPlot("Physical", Qt::yellow, Qt::SolidLine, true);
		pPlot->AddPlot("Limit", Qt::white, Qt::SolidLine);
		break;
	case eGpuMemPlot:
	{
#ifdef WIN32
		pPlot->SetRagne(100);
		pPlot->AddPlot("Dedicated", Qt::green, Qt::SolidLine, true);
		pPlot->AddPlot("Shared", Qt::red, Qt::SolidLine, true);
#endif
		break;
	}
	case eObjectPlot:
#ifdef WIN32
		pPlot->AddPlot("Gdi", Qt::green, Qt::SolidLine);
		pPlot->AddPlot("User", Qt::red, Qt::SolidLine);
#endif
		break;
	case eWindowsPlot:
#ifdef WIN32
		pPlot->AddPlot("Wnd", Qt::green, Qt::SolidLine);
#endif
		break;
	case eHandledPlot:
		pPlot->AddPlot("Handles", Qt::green, Qt::SolidLine);
		break;
	case eDiskIoPlot:
		pPlot->AddPlot("Read", Qt::green, Qt::SolidLine);
		pPlot->AddPlot("Write", Qt::red, Qt::SolidLine);
		break;
	case eMMapIoPlot:
		pPlot->AddPlot("Read", Qt::green, Qt::SolidLine);
		pPlot->AddPlot("Write", Qt::red, Qt::SolidLine);
		break;
	case eFileIoPlot:
		pPlot->AddPlot("Read", Qt::green, Qt::SolidLine);
		pPlot->AddPlot("Write", Qt::red, Qt::SolidLine);
		pPlot->AddPlot("Other", Qt::blue, Qt::SolidLine);
		break;
	case eSambaPlot:
		pPlot->AddPlot("RecvTotal", Qt::green, Qt::SolidLine);
		pPlot->AddPlot("SentTotal", Qt::red, Qt::SolidLine);
		pPlot->AddPlot("RecvServer", Qt::green, Qt::DashLine);
		pPlot->AddPlot("SentServer", Qt::red, Qt::DashLine);
		pPlot->AddPlot("RecvClient", Qt::green, Qt::DotLine);
		pPlot->AddPlot("SentClient", Qt::red, Qt::DotLine);
		break;
	case eClientPlot:
		pPlot->AddPlot("RecvClient", Qt::green, Qt::SolidLine);
		pPlot->AddPlot("SentClient", Qt::red, Qt::SolidLine);
		break;
	case eServerPlot:
		pPlot->AddPlot("RecvServer", Qt::green, Qt::SolidLine);
		pPlot->AddPlot("SentServer", Qt::red, Qt::SolidLine);
		break;
	case eRasPlot:
		pPlot->AddPlot("Recv", Qt::green, Qt::SolidLine);
		pPlot->AddPlot("Send", Qt::red, Qt::SolidLine);
		break;
	case eNetworkPlot:
		pPlot->AddPlot("Recv", Qt::green, Qt::SolidLine);
		pPlot->AddPlot("Send", Qt::red, Qt::SolidLine);
		break;
	case eGpuPlot:
	{
		pPlot->SetRagne(100);
#ifdef WIN32
		CGpuMonitor* pGpuMonitor = ((CWindowsAPI*)theAPI)->GetGpuMonitor();
		QColor GpuColors[] = { Qt::green, Qt::cyan, Qt::yellow, Qt::blue};
		for (int i = 0; i < pGpuMonitor->GetGpuCount(); i++)
			pPlot->AddPlot("Gpu_" + QString::number(i), GpuColors[i % ARRSIZE(GpuColors)], Qt::SolidLine, true);
#endif
		break;
	}
	case eCpuPlot:
		pPlot->SetRagne(100);
		pPlot->AddPlot("User", Qt::green, Qt::SolidLine, true);
		pPlot->AddPlot("Kernel", Qt::red, Qt::SolidLine, true);
		pPlot->AddPlot("DPC", Qt::blue, Qt::SolidLine, true);
		break;
	}

	FixPlotScale(pPlot);

	m_Graphs.append(qMakePair(Graph, pPlot));
	
	m_pMainLayout->addWidget(pPlot, row, column);

	pPlot->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(pPlot, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(OnMenu(const QPoint &)));

	//connect(pPlot, SIGNAL(Entered()), this, SLOT(OnEntered()));
	//connect(pPlot, SIGNAL(Moveed(QMouseEvent*)), this, SLOT(OnMoveed(QMouseEvent*)));
	//connect(pPlot, SIGNAL(Exited()), this, SLOT(OnExited()));

	connect(pPlot, SIGNAL(ToolTipRequested(QEvent*)), this, SLOT(OnToolTipRequested(QEvent*)));
}

void CGraphBar::DeleteGraphs()
{
	foreach(const TGraphPair& Graph, m_Graphs)
		delete Graph.second;
	m_Graphs.clear();
}

void CGraphBar::FixPlotScale(CIncrementalPlot* pPlot)
{
	// add a dummy curve that always stays at 0 in order to force autoscale to keep the lower bound always at 0
	pPlot->AddPlot("end", Qt::transparent, Qt::NoPen);
	pPlot->AddPlotPoint("end", 0.1);
	pPlot->AddPlotPoint("end", 0.0);
}


void CGraphBar::UpdateGraphs()
{
	SSysStats Stats = theAPI->GetStats();
#ifdef WIN32
	CGpuMonitor* pGpuMonitor = ((CWindowsAPI*)theAPI)->GetGpuMonitor();
#endif

	foreach(const TGraphPair& Graph, m_Graphs)
	{
		CIncrementalPlot* pPlot = Graph.second;
		QString Text;
		QStringList Texts;
		switch (Graph.first)
		{
		case eMemoryPlot:
			Text = tr("Memory=%1%").arg(theAPI->GetInstalledMemory() ? (int)100*theAPI->GetPhysicalUsed()/theAPI->GetInstalledMemory() : 0);
			pPlot->SetRagne(theAPI->GetMemoryLimit());
			pPlot->AddPlotPoint("Commited", theAPI->GetCommitedMemory());
			pPlot->AddPlotPoint("Swapped", theAPI->GetPhysicalUsed() + theAPI->GetSwapedOutMemory());
			pPlot->AddPlotPoint("Cache", theAPI->GetCacheMemory());
			pPlot->AddPlotPoint("Physical", theAPI->GetPhysicalUsed() - theAPI->GetCacheMemory());
			pPlot->AddPlotPoint("Limit", theAPI->GetInstalledMemory());
			break;
		case eGpuMemPlot:
		{
#ifdef WIN32
			CGpuMonitor::SGpuMemory GpuMemory = pGpuMonitor->GetGpuMemory();
			
			Text = tr("Gpu Memory");

			Texts.append(FormatUnit(GpuMemory.DedicatedUsage, 0));
			pPlot->AddPlotPoint("Dedicated", GpuMemory.DedicatedLimit ? 100 * GpuMemory.DedicatedUsage / GpuMemory.DedicatedLimit : 0);

			Texts.append(FormatUnit(GpuMemory.SharedUsage, 0));
			pPlot->AddPlotPoint("Shared", GpuMemory.SharedLimit ? 100 * GpuMemory.SharedUsage / GpuMemory.SharedLimit : 0);
#endif
			break;
		}
		case eObjectPlot:
#ifdef WIN32
			Text = tr("Objects<%1").arg(FormatUnit(pPlot->GetRangeMax()));
			
			Texts.append(FormatUnit(((CWindowsAPI*)theAPI)->GetTotalGuiObjects(), 1));
			pPlot->AddPlotPoint("Gdi", ((CWindowsAPI*)theAPI)->GetTotalGuiObjects());

			Texts.append(FormatUnit(((CWindowsAPI*)theAPI)->GetTotalUserObjects(), 1));
			pPlot->AddPlotPoint("User", ((CWindowsAPI*)theAPI)->GetTotalUserObjects());
#endif
			break;

		case eWindowsPlot:
#ifdef WIN32
			Text = tr("Windows<%1").arg(FormatUnit(pPlot->GetRangeMax()));

			Texts.append(FormatUnit(((CWindowsAPI*)theAPI)->GetTotalWndObjects(), 1));
			pPlot->AddPlotPoint("Wnd", ((CWindowsAPI*)theAPI)->GetTotalWndObjects());
#endif
			break;

		case eHandledPlot:
			Text = tr("Handles<%1").arg(FormatUnit(pPlot->GetRangeMax()));

			pPlot->AddPlotPoint("Handles", theAPI->GetTotalHandles());

			break;

		case eDiskIoPlot:
			Text = tr("DiskIO<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));
			
			Texts.append(FormatUnit(Stats.Disk.ReadRate.Get(), 0));
			pPlot->AddPlotPoint("Read", Stats.Disk.ReadRate.Get());

			Texts.append(FormatUnit(Stats.Disk.WriteRate.Get(), 0));
			pPlot->AddPlotPoint("Write", Stats.Disk.WriteRate.Get());
			break;

		case eMMapIoPlot:
			Text = tr("MMapIO<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));

			Texts.append(FormatUnit(Stats.MMapIo.ReadRate.Get(), 0));
			pPlot->AddPlotPoint("Read", Stats.MMapIo.ReadRate.Get());

			Texts.append(FormatUnit(Stats.MMapIo.WriteRate.Get(), 0));
			pPlot->AddPlotPoint("Write", Stats.MMapIo.WriteRate.Get());

			break;

		case eFileIoPlot:
			Text = tr("FileIO<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));

			Texts.append(FormatUnit(Stats.Io.ReadRate.Get(), 0));
			pPlot->AddPlotPoint("Read", Stats.Io.ReadRate.Get());

			Texts.append(FormatUnit(Stats.Io.WriteRate.Get(), 0));
			pPlot->AddPlotPoint("Write", Stats.Io.WriteRate.Get());

			Texts.append(FormatUnit(Stats.Io.OtherRate.Get(), 0));
			pPlot->AddPlotPoint("Other", Stats.Io.OtherRate.Get());

			break;

		case eSambaPlot:
			Text = tr("Samba<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));
			
			Texts.append(FormatUnit(Stats.SambaClient.ReceiveRate.Get() + Stats.SambaServer.ReceiveRate.Get(), 0));
			Texts.append(FormatUnit(Stats.SambaClient.SendRate.Get() + Stats.SambaServer.SendRate.Get(), 0));

			pPlot->AddPlotPoint("RecvTotal", Stats.SambaClient.ReceiveRate.Get() + Stats.SambaServer.ReceiveRate.Get());
			pPlot->AddPlotPoint("SentTotal", Stats.SambaClient.SendRate.Get() + Stats.SambaServer.SendRate.Get());
			pPlot->AddPlotPoint("RecvServer", Stats.SambaServer.ReceiveRate.Get());
			pPlot->AddPlotPoint("SentServer", Stats.SambaServer.SendRate.Get());
			pPlot->AddPlotPoint("RecvClient", Stats.SambaClient.ReceiveRate.Get());
			pPlot->AddPlotPoint("SentClient", Stats.SambaClient.SendRate.Get() );

			break;

		case eClientPlot:
			Text = tr("Client<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));

			Texts.append(FormatUnit(Stats.SambaClient.ReceiveRate.Get(), 0));
			pPlot->AddPlotPoint("RecvClient", Stats.SambaClient.ReceiveRate.Get());

			Texts.append(FormatUnit(Stats.SambaClient.SendRate.Get(), 0));
			pPlot->AddPlotPoint("SentClient", Stats.SambaClient.SendRate.Get() );

			break;

		case eServerPlot:
			Text = tr("Server<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));
	
			Texts.append(FormatUnit(Stats.SambaServer.ReceiveRate.Get(), 0));
			pPlot->AddPlotPoint("RecvServer", Stats.SambaServer.ReceiveRate.Get());

			Texts.append(FormatUnit(Stats.SambaServer.SendRate.Get(), 0));
			pPlot->AddPlotPoint("SentServer", Stats.SambaServer.SendRate.Get() );

			break;

		case eRasPlot:
			Text = tr("RAS/VPN<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));
	
			Texts.append(FormatUnit(Stats.NetRas.ReceiveRate.Get(), 0));
			pPlot->AddPlotPoint("Recv", Stats.NetRas.ReceiveRate.Get());

			Texts.append(FormatUnit(Stats.NetRas.SendRate.Get(), 0));
			pPlot->AddPlotPoint("Send", Stats.NetRas.SendRate.Get());

			break;

		case eNetworkPlot:
			Text = tr("TCP/IP<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));

			Texts.append(FormatUnit(Stats.NetIf.ReceiveRate.Get(), 0));
			pPlot->AddPlotPoint("Recv", Stats.NetIf.ReceiveRate.Get());

			Texts.append(FormatUnit(Stats.NetIf.SendRate.Get(), 0));
			pPlot->AddPlotPoint("Send", Stats.NetIf.SendRate.Get());

			break;

		case eGpuPlot:
		{
#ifdef WIN32
			int MaxGpuUsage = 0;
			for (int i = 0; i < pGpuMonitor->GetGpuCount(); i++)
			{
				int GpuUsage = 100 * pGpuMonitor->GetGpuUsage(i);
				Texts.append(tr("%1%").arg(GpuUsage));
				pPlot->AddPlotPoint("Gpu_" + QString::number(i), GpuUsage);
				if (GpuUsage > MaxGpuUsage)
					MaxGpuUsage = GpuUsage;
			}
			Text = tr("Gpu=%1%").arg(MaxGpuUsage);
#endif
			break;
		}
		case eCpuPlot:
			Text = tr("Cpu=%1%").arg(int(100*theAPI->GetCpuUsage()));
			
			Texts.append(tr("%1%").arg(int(100*theAPI->GetCpuUserUsage())));
			pPlot->AddPlotPoint("User", theAPI->GetCpuUsage()*100);

			Texts.append(tr("%1%").arg(int(100*theAPI->GetCpuKernelUsage())));
			pPlot->AddPlotPoint("Kernel", theAPI->GetCpuKernelUsage()*100 + theAPI->GetCpuDPCUsage()*100);

			Texts.append(tr("%1%").arg(int(100*theAPI->GetCpuDPCUsage())));
			pPlot->AddPlotPoint("DPC", theAPI->GetCpuDPCUsage()*100);

			break;
		}
		pPlot->SetText(Text);
		pPlot->SetTexts(Texts);
	}
}


void CGraphBar::OnMenu(const QPoint& Point)
{
	m_pCurPlot = qobject_cast<CIncrementalPlot*>(sender());
	m_pMenu->popup(QCursor::pos());	
}

void CGraphBar::ClearGraphs()
{
	if (sender() == m_pResetPlot)
	{
		if (m_pCurPlot)
		{
			m_pCurPlot->Reset();
			FixPlotScale(m_pCurPlot);
		}
	}
	else
	{
		foreach(const TGraphPair& Graph, m_Graphs)
		{
			Graph.second->Reset();
			FixPlotScale(Graph.second);
		}
	}
}

void CGraphBar::CustomizeGraphs()
{
	QWidget* pWidget = new QWidget();
	QHBoxLayout* pLayout = new QHBoxLayout();
	pWidget->setLayout(pLayout);
	pLayout->addWidget(new QLabel(tr("Graph Rows")));
	QSpinBox* pRows = new QSpinBox();
	pLayout->addWidget(pRows);
	pLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum));

	pRows->setMaximum(1);
	pRows->setMaximum(4);
	pRows->setValue(m_Rows);

	CItemChooser ItemChooser(tr("Select graphs that will be displayed in the graph bar."));
	ItemChooser.setWindowTitle(tr("Graph Chooser"));
	ItemChooser.AddWidget(pWidget);

	ItemChooser.AddItem(tr("GPU Memory"), eGpuMemPlot);
	ItemChooser.AddItem(tr("System Memory"), eMemoryPlot);
	ItemChooser.AddItem(tr("Object Usage"), eObjectPlot);
	ItemChooser.AddItem(tr("Window Usage"), eWindowsPlot);
	ItemChooser.AddItem(tr("Handle Usage"), eHandledPlot);
	ItemChooser.AddItem(tr("Disk I/O"), eDiskIoPlot);
	ItemChooser.AddItem(tr("Memory Mapped I/O"), eMMapIoPlot);
	ItemChooser.AddItem(tr("File I/O"), eFileIoPlot);
	ItemChooser.AddItem(tr("Samba Combined U/D"), eSambaPlot);
	ItemChooser.AddItem(tr("Samba Client U/D"), eClientPlot);
	ItemChooser.AddItem(tr("Samba Server U/D"), eServerPlot);
	ItemChooser.AddItem(tr("RAS / VPN"), eRasPlot);
	ItemChooser.AddItem(tr("Network U/D"), eNetworkPlot);
	ItemChooser.AddItem(tr("GPU Usage"), eGpuPlot);
	ItemChooser.AddItem(tr("CPU Usage"), eCpuPlot);

	QVariantList ChoosenItems;
	foreach(const TGraphPair& Graph, m_Graphs)
		ChoosenItems.append(Graph.first);
	ItemChooser.ChooseItems(ChoosenItems);

	if (!ItemChooser.exec())
		return;

	DeleteGraphs();

	QList<EGraph> Graphs;
	foreach(const QVariant& Data, ItemChooser.GetChoosenItems())
		Graphs.append((EGraph)Data.toInt());
	AddGraphs(Graphs, pRows->value());
}

/*void CGraphBar::OnEntered()
{
}

void CGraphBar::OnMoveed(QMouseEvent* event)
{
	
}

void CGraphBar::OnExited()
{
}*/

void CGraphBar::OnToolTipRequested(QEvent* event)
{
	if (m_pLastTipGraph != sender());
	{
		QToolTip::hideText();
		m_pLastTipGraph = (QWidget*)sender();
	}

	QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);

	EGraph Type = eCount;
	foreach(const TGraphPair& Graph, m_Graphs)
	{
		if (Graph.second == m_pLastTipGraph)
		{
			Type = Graph.first;
			break;
		}
	}
	if (Type == eCount)
		return;

	SSysStats Stats = theAPI->GetStats();
#ifdef WIN32
	CGpuMonitor* pGpuMonitor = ((CWindowsAPI*)theAPI)->GetGpuMonitor();
#endif

	QString Text;
	switch (Type)
	{
	case eMemoryPlot:
		Text = tr("<h3>System Memory Usage:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>Commited</b> memory: %1</li>").arg(FormatSize(theAPI->GetCommitedMemory()));
		Text += tr("<li><b>Swapped</b> memory: %1</li>").arg(FormatSize(theAPI->GetSwapedOutMemory()));
		Text += tr("<li><b>Cache</b> memory: %1</li>").arg(FormatSize(theAPI->GetCacheMemory()));
		Text += tr("<li><b>Physical</b> memory used: %1/%2</li>").arg(FormatSize(theAPI->GetPhysicalUsed())).arg(FormatSize(theAPI->GetInstalledMemory()));
		Text += tr("</ul>");
		break;
	case eGpuMemPlot:
	{
#ifdef WIN32
		Text = tr("<h3>GPU Memory Usage:</h3>");
		Text += tr("<ul>");
		for (int i = 0; i < pGpuMonitor->GetGpuCount(); i++)
		{
			CGpuMonitor::SGpuInfo GpuInfo = pGpuMonitor->GetGpuInfo(i);
			Text += tr("<li>%1</li>").arg(GpuInfo.Description);
			Text += tr("<ul>");

			CGpuMonitor::SGpuMemory GpuMemory = pGpuMonitor->GetGpuMemory(i);
			Text += tr("<li><b>Dedicated</b> memory: %1/%2</li>").arg(FormatSize(GpuMemory.DedicatedUsage)).arg(FormatSize(GpuMemory.DedicatedLimit));
			Text += tr("<li><b>Shared</b> memory: %1/%2</li>").arg(FormatSize(GpuMemory.SharedUsage)).arg(FormatSize(GpuMemory.SharedLimit));

			Text += tr("</ul>");
		}
		Text += tr("</ul>");
#endif
		break;
	}
	case eObjectPlot:
#ifdef WIN32
		Text = tr("<h3>Object Usage:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>Gdi</b> objects: %1</li>").arg(((CWindowsAPI*)theAPI)->GetTotalGuiObjects());
		Text += tr("<li><b>User</b> objects: %1</li>").arg(((CWindowsAPI*)theAPI)->GetTotalUserObjects());
		Text += tr("</ul>");
#endif
		break;

	case eWindowsPlot:
#ifdef WIN32
		Text = tr("<h3>Window Usage:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>Window</b> objects: %1</li>").arg(((CWindowsAPI*)theAPI)->GetTotalWndObjects());
		Text += tr("</ul>");
#endif
		break;

	case eHandledPlot:
		Text = tr("<h3>Handle Usage:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>Handles</b>: %1</li>").arg(theAPI->GetTotalHandles());
		Text += tr("</ul>");
		break;

	case eDiskIoPlot:
		Text = tr("<h3>Disk I/O:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>Read</b> rate: %1</li>").arg(FormatSize(Stats.Disk.ReadRate.Get()));
		Text += tr("<li><b>Write</b> rate: %1</li>").arg(FormatSize(Stats.Disk.WriteRate.Get()));
		Text += tr("</ul>");
		break;

	case eMMapIoPlot:
		Text = tr("<h3>Memory mapped I/O:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>Read</b> rate: %1</li>").arg(FormatSize(Stats.MMapIo.ReadRate.Get()));
		Text += tr("<li><b>Write</b> rate: %1</li>").arg(FormatSize(Stats.MMapIo.WriteRate.Get()));
		Text += tr("</ul>");
		break;

	case eFileIoPlot:
		Text = tr("<h3>File I/O:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>Read</b> rate: %1</li>").arg(FormatSize(Stats.Io.ReadRate.Get()));
		Text += tr("<li><b>Write</b> rate: %1</li>").arg(FormatSize(Stats.Io.WriteRate.Get()));
		Text += tr("<li><b>Other</b> rate: %1</li>").arg(FormatSize(Stats.Io.OtherRate.Get()));
		Text += tr("</ul>");
		break;

	case eSambaPlot:
		Text = tr("<h3>Samba share:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>Client Receive</b> rate: %1</li>").arg(FormatSize(Stats.SambaClient.ReceiveRate.Get()));
		Text += tr("<li><b>Client Send</b> rate: %1</li>").arg(FormatSize(Stats.SambaClient.SendRate.Get()));
		Text += tr("<li><b>Server Receive</b> rate: %1</li>").arg(FormatSize(Stats.SambaServer.ReceiveRate.Get()));
		Text += tr("<li><b>Server Send</b> rate: %1</li>").arg(FormatSize(Stats.SambaServer.SendRate.Get()));
		Text += tr("</ul>");
		break;

	case eClientPlot:
		Text = tr("<h3>Samba client:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>Receive</b> rate: %1</li>").arg(FormatSize(Stats.SambaClient.ReceiveRate.Get()));
		Text += tr("<li><b>Send</b> rate: %1</li>").arg(FormatSize(Stats.SambaClient.SendRate.Get()));
		Text += tr("</ul>");
		break;

	case eServerPlot:
		Text = tr("<h3>Samba server:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>Receive</b> rate: %1</li>").arg(FormatSize(Stats.SambaServer.ReceiveRate.Get()));
		Text += tr("<li><b>Send</b> rate: %1</li>").arg(FormatSize(Stats.SambaServer.SendRate.Get()));
		Text += tr("</ul>");
		break;

	case eRasPlot:
		Text = tr("<h3>RAS & VPN Traffic:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>Receive</b> rate: %1</li>").arg(FormatSize(Stats.NetRas.ReceiveRate.Get()));
		Text += tr("<li><b>Send</b> rate: %1</li>").arg(FormatSize(Stats.NetRas.SendRate.Get()));
		Text += tr("</ul>");
		break;

	case eNetworkPlot:
		Text = tr("<h3>TCP/IP Traffic:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>Receive</b> rate: %1</li>").arg(FormatSize(Stats.NetIf.ReceiveRate.Get()));
		Text += tr("<li><b>Send</b> rate: %1</li>").arg(FormatSize(Stats.NetIf.SendRate.Get()));
		Text += tr("</ul>");
		break;

	case eGpuPlot:
	{
#ifdef WIN32
		Text = tr("<h3>GPU Usage:</h3>");
		Text += tr("<ul>");
		for (int i = 0; i < pGpuMonitor->GetGpuCount(); i++)
		{
			CGpuMonitor::SGpuInfo GpuInfo = pGpuMonitor->GetGpuInfo(i);
			Text += tr("<li><b>%1</b> usage: %2%</li>").arg(GpuInfo.Description).arg(int(100*pGpuMonitor->GetGpuUsage(i)));
		}
		Text += tr("</ul>");
#endif
		break;
	}
	case eCpuPlot:
		Text = tr("<h3>CPU Usage:</h3>");
		Text += tr("<ul>");
		Text += tr("<li><b>User</b> usage: %1%</li>").arg(int(100*theAPI->GetCpuUserUsage()));
		Text += tr("<li><b>Kernel</b> usage: %1%</li>").arg(int(100*theAPI->GetCpuKernelUsage()));
		Text += tr("<li><b>DPC/IRQ</b> usage: %1%</li>").arg(int(100*theAPI->GetCpuDPCUsage()));
		Text += tr("</ul>");
		break;
	}
	
	QToolTip::showText(helpEvent->globalPos(), Text);
}