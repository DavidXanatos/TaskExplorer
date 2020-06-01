#include "stdafx.h"
#include "GraphBar.h"
#include "TaskExplorer.h"
#include "../../MiscHelpers/Common/ItemChooser.h"
#ifdef WIN32
#include "../API/Windows/WindowsAPI.h"
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
		Graphs.append(eDiskIoPlot);
		Graphs.append(eMMapIoPlot);
		Graphs.append(eFileIoPlot);
		//Graphs.append(eSambaPlot);
		Graphs.append(eClientPlot);
		Graphs.append(eServerPlot);
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

	m_PlotLimit = theGUI->GetGraphLimit();
	connect(theGUI, SIGNAL(ReloadPlots()), this, SLOT(ReConfigurePlots()));

	AddGraphs(Graphs, Rows);
}

CGraphBar::~CGraphBar()
{
	QStringList GraphList;
	foreach(const SGraph& Graph, m_Graphs)
		GraphList.append(QString::number(Graph.Type));
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

void CGraphBar::ReConfigurePlots()
{
	m_PlotLimit = theGUI->GetGraphLimit();
	QColor Back = theGUI->GetColor(CTaskExplorer::eGraphBack);
	QColor Front = theGUI->GetColor(CTaskExplorer::eGraphFront);

	foreach(const SGraph& Graph, m_Graphs) {
		Graph.pPlot->SetLimit(m_PlotLimit);
		Graph.pPlot->SetColors(Back);
		Graph.pPlot->SetTextColor(Front);
	}
}

void CGraphBar::AddGraph(EGraph Graph, int row, int column)
{
	QColor Back = theGUI->GetColor(CTaskExplorer::eGraphBack);
	QColor Front = theGUI->GetColor(CTaskExplorer::eGraphFront);

	CIncrementalPlot* pPlot = new CIncrementalPlot(Back);

	pPlot->SetLimit(m_PlotLimit);
	pPlot->SetTextColor(Front);

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
		pPlot->SetRagne(100);
		pPlot->AddPlot("Dedicated", Qt::green, Qt::SolidLine, true);
		pPlot->AddPlot("Shared", Qt::red, Qt::SolidLine, true);
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
		pPlot->AddPlot("RecvL", Qt::blue, Qt::SolidLine);
		pPlot->AddPlot("SendL", Qt::yellow, Qt::SolidLine);
		break;
	case eGpuPlot:
		pPlot->SetRagne(100);
		break;
	case eCpuPlot:
		pPlot->SetRagne(100);
		pPlot->AddPlot("User", Qt::green, Qt::SolidLine, true);
		pPlot->AddPlot("Kernel", Qt::red, Qt::SolidLine, true);
		pPlot->AddPlot("DPC", Qt::blue, Qt::SolidLine, true);
		break;
	}

	FixPlotScale(pPlot);

	SGraph GraphData;
	GraphData.Type = Graph;
	GraphData.pPlot = pPlot;
	m_Graphs.append(GraphData);
	
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
	foreach(const SGraph& Graph, m_Graphs)
		delete Graph.pPlot;
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
	SSysStats SysStats = theAPI->GetStats();
	CGpuMonitor* pGpuMonitor = theAPI->GetGpuMonitor();
	CDiskMonitor* pDiskMonitor = theAPI->GetDiskMonitor();
	CNetMonitor* pNetMonitor = theAPI->GetNetMonitor();

	for(QList<SGraph>::iterator I = m_Graphs.begin(); I != m_Graphs.end(); ++I)
	{
		CIncrementalPlot* pPlot = I->pPlot;
		QString Text;
		QStringList Texts;
		switch (I->Type)
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
			CGpuMonitor::SGpuMemory GpuMemory = pGpuMonitor->GetGpuMemory();
			
			Text = tr("Gpu Memory");

			Texts.append(FormatSize(GpuMemory.DedicatedUsage, 0));
			pPlot->AddPlotPoint("Dedicated", GpuMemory.DedicatedLimit ? 100 * GpuMemory.DedicatedUsage / GpuMemory.DedicatedLimit : 0);

			Texts.append(FormatSize(GpuMemory.SharedUsage, 0));
			pPlot->AddPlotPoint("Shared", GpuMemory.SharedLimit ? 100 * GpuMemory.SharedUsage / GpuMemory.SharedLimit : 0);
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
		{
			CDiskMonitor::SDataRates DiskRates = pDiskMonitor->GetAllDiskDataRates();
			int DiskUsage = theConf->GetInt("Options/DiskUsageMode", 2);

			int DiskPlotCount = I->Params["DiskPlotCount"].toInt();
			if(DiskUsage != 0 && DiskRates.DiskCount > 0 && (DiskRates.Supported == DiskRates.DiskCount || DiskUsage == 1))
			{
				QMap<QString, CDiskMonitor::SDiskInfo> DiskList = pDiskMonitor->GetDiskList();

				if (DiskPlotCount != DiskRates.Supported)
				{
					DiskPlotCount = 0;
					pPlot->Clear();

					pPlot->SetRagne(100);
					QVector<QColor> Colors = theGUI->GetPlotColors();
					foreach(const CDiskMonitor::SDiskInfo& Disk, DiskList)
					{
						if (!Disk.DeviceSupported)
							continue;

						pPlot->AddPlot("Disk_" + QString::number(DiskPlotCount), Colors[DiskPlotCount % Colors.size()], Qt::SolidLine, true);
						DiskPlotCount++;
					}
					I->Params["DiskPlotCount"] = DiskPlotCount;
				}

				int MaxDiskUsage = 0;
				int i = 0;
				foreach(const CDiskMonitor::SDiskInfo& Disk, DiskList)
				{
					if (!Disk.DeviceSupported)
						continue;

					int DiskUsage = Disk.ActiveTime;
					Texts.append(tr("%1%").arg(DiskUsage));
					pPlot->AddPlotPoint("Disk_" + QString::number(i), DiskUsage);
					if (DiskUsage > MaxDiskUsage)
						MaxDiskUsage = DiskUsage;
					i++;
				}
				Text = tr("Disk=%1%").arg(MaxDiskUsage);
			}
			else
			{
				if (DiskPlotCount != 0)
				{
					I->Params["DiskPlotCount"] = 0;
					pPlot->Clear();
					pPlot->AddPlot("Read", Qt::green, Qt::SolidLine);
					pPlot->AddPlot("Write", Qt::red, Qt::SolidLine);
				}

				Text = tr("DiskIO<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));

				quint64 ReadRate = 0;
				quint64 WriteRate = 0;
#ifdef WIN32
				if (((CWindowsAPI*)theAPI)->IsMonitoringETW())
				{
					ReadRate = SysStats.Disk.ReadRate.Get();
					WriteRate = SysStats.Disk.WriteRate.Get();
				}
				else
#endif
				{
					ReadRate = DiskRates.ReadRate;
					WriteRate = DiskRates.WriteRate;
				}

				Texts.append(FormatSize(SysStats.Disk.ReadRate.Get(), 0));
				pPlot->AddPlotPoint("Read", SysStats.Disk.ReadRate.Get());

				Texts.append(FormatSize(SysStats.Disk.WriteRate.Get(), 0));
				pPlot->AddPlotPoint("Write", SysStats.Disk.WriteRate.Get());
			}
			break;
		}
		case eMMapIoPlot:
			Text = tr("MMapIO<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));

			Texts.append(FormatSize(SysStats.MMapIo.ReadRate.Get(), 0));
			pPlot->AddPlotPoint("Read", SysStats.MMapIo.ReadRate.Get());

			Texts.append(FormatSize(SysStats.MMapIo.WriteRate.Get(), 0));
			pPlot->AddPlotPoint("Write", SysStats.MMapIo.WriteRate.Get());

			break;

		case eFileIoPlot:
			Text = tr("FileIO<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));

			Texts.append(FormatSize(SysStats.Io.ReadRate.Get(), 0));
			pPlot->AddPlotPoint("Read", SysStats.Io.ReadRate.Get());

			Texts.append(FormatSize(SysStats.Io.WriteRate.Get(), 0));
			pPlot->AddPlotPoint("Write", SysStats.Io.WriteRate.Get());

			Texts.append(FormatSize(SysStats.Io.OtherRate.Get(), 0));
			pPlot->AddPlotPoint("Other", SysStats.Io.OtherRate.Get());

			break;

#ifdef WIN32
		case eSambaPlot:
			Text = tr("Samba<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));
			
			Texts.append(FormatSize(SysStats.SambaClient.ReceiveRate.Get() + SysStats.SambaServer.ReceiveRate.Get(), 0));
			Texts.append(FormatSize(SysStats.SambaClient.SendRate.Get() + SysStats.SambaServer.SendRate.Get(), 0));

			pPlot->AddPlotPoint("RecvTotal", SysStats.SambaClient.ReceiveRate.Get() + SysStats.SambaServer.ReceiveRate.Get());
			pPlot->AddPlotPoint("SentTotal", SysStats.SambaClient.SendRate.Get() + SysStats.SambaServer.SendRate.Get());
			pPlot->AddPlotPoint("RecvServer", SysStats.SambaServer.ReceiveRate.Get());
			pPlot->AddPlotPoint("SentServer", SysStats.SambaServer.SendRate.Get());
			pPlot->AddPlotPoint("RecvClient", SysStats.SambaClient.ReceiveRate.Get());
			pPlot->AddPlotPoint("SentClient", SysStats.SambaClient.SendRate.Get() );

			break;

		case eClientPlot:
			Text = tr("Client<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));

			Texts.append(FormatSize(SysStats.SambaClient.ReceiveRate.Get(), 0));
			pPlot->AddPlotPoint("RecvClient", SysStats.SambaClient.ReceiveRate.Get());

			Texts.append(FormatSize(SysStats.SambaClient.SendRate.Get(), 0));
			pPlot->AddPlotPoint("SentClient", SysStats.SambaClient.SendRate.Get() );

			break;

		case eServerPlot:
			Text = tr("Server<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));
	
			Texts.append(FormatSize(SysStats.SambaServer.ReceiveRate.Get(), 0));
			pPlot->AddPlotPoint("RecvServer", SysStats.SambaServer.ReceiveRate.Get());

			Texts.append(FormatSize(SysStats.SambaServer.SendRate.Get(), 0));
			pPlot->AddPlotPoint("SentServer", SysStats.SambaServer.SendRate.Get() );

			break;
#endif

		case eRasPlot:
		{
			CNetMonitor::SDataRates RasRates = pNetMonitor->GetTotalDataRate(CNetMonitor::eRas);

			Text = tr("RAS/VPN<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));

			Texts.append(FormatSize(RasRates.ReceiveRate, 0));
			pPlot->AddPlotPoint("Recv", RasRates.ReceiveRate);

			Texts.append(FormatSize(RasRates.SendRate, 0));
			pPlot->AddPlotPoint("Send", RasRates.SendRate);

			break;
		}
		case eNetworkPlot:
		{
			CNetMonitor::SDataRates NetRates = pNetMonitor->GetTotalDataRate(CNetMonitor::eNet);

			Text = tr("TCP/IP<%1").arg(FormatSize(pPlot->GetRangeMax(), 0));

			Texts.append(FormatSize(NetRates.ReceiveRate, 0));
			pPlot->AddPlotPoint("Recv", NetRates.ReceiveRate);

			Texts.append(FormatSize(NetRates.SendRate, 0));
			pPlot->AddPlotPoint("Send", NetRates.SendRate);

#ifdef WIN32
			if (((CWindowsAPI*)theAPI)->IsMonitoringETW() && theConf->GetBool("Options/ShowLanPlot", false))
			{
				Texts.append(FormatSize(SysStats.Lan.ReceiveRate.Get(), 0));
				pPlot->AddPlotPoint("RecvL", SysStats.Lan.ReceiveRate.Get());

				Texts.append(FormatSize(SysStats.Lan.SendRate.Get(), 0));
				pPlot->AddPlotPoint("SendL", SysStats.Lan.SendRate.Get());
			}
			else 
			{
				pPlot->AddPlotPoint("RecvL", 0);
				pPlot->AddPlotPoint("SendL", 0);
			}
#endif
			break;
		}
		case eGpuPlot:
		{
			QMap<QString, CGpuMonitor::SGpuInfo> GpuList = pGpuMonitor->GetAllGpuList();

			int GpuPlotCount = I->Params["GpuPlotCount"].toInt();
			if (GpuPlotCount != GpuList.size())
			{
				GpuPlotCount = 0;
				pPlot->Clear();

				pPlot->SetRagne(100);
				QVector<QColor> Colors = theGUI->GetPlotColors();
				foreach(const CGpuMonitor::SGpuInfo &GpuInfo, GpuList)
				{
					pPlot->AddPlot("Gpu_" + QString::number(GpuPlotCount), Colors[GpuPlotCount % Colors.size()], Qt::SolidLine, true);
					GpuPlotCount++;
				}
				I->Params["GpuPlotCount"] = GpuPlotCount;
			}

			int MaxGpuUsage = 0;
			int i = 0;
			foreach(const CGpuMonitor::SGpuInfo &GpuInfo, GpuList)
			{
				int GpuUsage = 100 * GpuInfo.TimeUsage;
				Texts.append(tr("%1%").arg(GpuUsage));
				pPlot->AddPlotPoint("Gpu_" + QString::number(i), GpuUsage);
				if (GpuUsage > MaxGpuUsage)
					MaxGpuUsage = GpuUsage;
				i++;
			}
			Text = tr("GPU=%1%").arg(MaxGpuUsage);
			break;
		}
		case eCpuPlot:
			Text = tr("CPU=%1%").arg(int(100*theAPI->GetCpuUsage()));
			
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
		foreach(const SGraph& Graph, m_Graphs)
		{
			Graph.pPlot->Reset();
			FixPlotScale(Graph.pPlot);
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
	foreach(const SGraph& Graph, m_Graphs)
		ChoosenItems.append(Graph.Type);
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
	QVariantMap Params;
	foreach(const SGraph& Graph, m_Graphs)
	{
		if (Graph.pPlot == m_pLastTipGraph)
		{
			Type = Graph.Type;
			Params = Graph.Params;
			break;
		}
	}
	if (Type == eCount)
		return;

	SSysStats SysStats = theAPI->GetStats();
	CGpuMonitor* pGpuMonitor = theAPI->GetGpuMonitor();
	CDiskMonitor* pDiskMonitor = theAPI->GetDiskMonitor();
	CNetMonitor* pNetMonitor = theAPI->GetNetMonitor();

	QStringList TextLines;
	switch (Type)
	{
	case eMemoryPlot:
		TextLines.append(tr("System Memory Usage:"));
		TextLines.append(tr("    Commited memory: %1").arg(FormatSize(theAPI->GetCommitedMemory())));
		TextLines.append(tr("    Swapped memory: %1").arg(FormatSize(theAPI->GetSwapedOutMemory())));
		TextLines.append(tr("    Cache memory: %1").arg(FormatSize(theAPI->GetCacheMemory())));
		TextLines.append(tr("    Physical memory used: %1/%2").arg(FormatSize(theAPI->GetPhysicalUsed())).arg(FormatSize(theAPI->GetInstalledMemory())));
		break;
	case eGpuMemPlot:
	{
		QMap<QString, CGpuMonitor::SGpuInfo> GpuList = pGpuMonitor->GetAllGpuList();

		foreach(const CGpuMonitor::SGpuInfo &GpuInfo, GpuList)
		{
			TextLines.append(tr("%1 Memory Usage:").arg(GpuInfo.Description));
			TextLines.append(tr("    Dedicated memory: %1/%2").arg(FormatSize(GpuInfo.Memory.DedicatedUsage)).arg(FormatSize(GpuInfo.Memory.DedicatedLimit)));
			TextLines.append(tr("    Shared memory: %1/%2").arg(FormatSize(GpuInfo.Memory.SharedUsage)).arg(FormatSize(GpuInfo.Memory.SharedLimit)));

		}
		break;
	}
	case eObjectPlot:
#ifdef WIN32
		TextLines.append(tr("Object Usage:"));
		TextLines.append(tr("    Gdi objects: %1").arg(((CWindowsAPI*)theAPI)->GetTotalGuiObjects()));
		TextLines.append(tr("    User objects: %1").arg(((CWindowsAPI*)theAPI)->GetTotalUserObjects()));
#endif
		break;

	case eWindowsPlot:
#ifdef WIN32
		TextLines.append(tr("Window Usage:"));
		TextLines.append(tr("    Window objects: %1").arg(((CWindowsAPI*)theAPI)->GetTotalWndObjects()));
#endif
		break;

	case eHandledPlot:
		TextLines.append(tr("Handle Usage:"));
		TextLines.append(tr("    Handles: %1").arg(theAPI->GetTotalHandles()));
		break;

	case eDiskIoPlot:
		if(Params["DiskPlotCount"].toInt() > 0)
		{
			QMap<QString, CDiskMonitor::SDiskInfo> DiskList = pDiskMonitor->GetDiskList();

			TextLines.append(tr("Disk Usage:"));
			foreach(const CDiskMonitor::SDiskInfo& Disk, DiskList)
				TextLines.append(tr("    %1 usage: %2%").arg(Disk.DeviceName).arg(int(Disk.ActiveTime)));
		}
		else
		{
			TextLines.append(tr("Disk I/O:"));
			TextLines.append(tr("    Read rate: %1").arg(FormatSize(SysStats.Disk.ReadRate.Get())));
			TextLines.append(tr("    Write rate: %1").arg(FormatSize(SysStats.Disk.WriteRate.Get())));
		}
		break;

	case eMMapIoPlot:
		TextLines.append(tr("Memory mapped I/O:"));
		TextLines.append(tr("    Read rate: %1").arg(FormatSize(SysStats.MMapIo.ReadRate.Get())));
		TextLines.append(tr("    Write rate: %1").arg(FormatSize(SysStats.MMapIo.WriteRate.Get())));
		break;

	case eFileIoPlot:
		TextLines.append(tr("File I/O:"));
		TextLines.append(tr("    Read rate: %1").arg(FormatSize(SysStats.Io.ReadRate.Get())));
		TextLines.append(tr("    Write rate: %1").arg(FormatSize(SysStats.Io.WriteRate.Get())));
		TextLines.append(tr("    Other rate: %1").arg(FormatSize(SysStats.Io.OtherRate.Get())));
		break;

#ifdef WIN32
	case eSambaPlot:
		TextLines.append(tr("Samba client:"));
		TextLines.append(tr("    Receive rate: %1").arg(FormatSize(SysStats.SambaClient.ReceiveRate.Get())));
		TextLines.append(tr("    Send rate: %1").arg(FormatSize(SysStats.SambaClient.SendRate.Get())));
		TextLines.append(tr("Samba server:"));
		TextLines.append(tr("    Receive rate: %1").arg(FormatSize(SysStats.SambaServer.ReceiveRate.Get())));
		TextLines.append(tr("    Send rate: %1").arg(FormatSize(SysStats.SambaServer.SendRate.Get())));
		break;

	case eClientPlot:
		TextLines.append(tr("Samba client:"));
		TextLines.append(tr("    Receive rate: %1").arg(FormatSize(SysStats.SambaClient.ReceiveRate.Get())));
		TextLines.append(tr("    Send rate: %1").arg(FormatSize(SysStats.SambaClient.SendRate.Get())));
		break;

	case eServerPlot:
		TextLines.append(tr("Samba server:"));
		TextLines.append(tr("    Receive rate: %1").arg(FormatSize(SysStats.SambaServer.ReceiveRate.Get())));
		TextLines.append(tr("    Send rate: %1").arg(FormatSize(SysStats.SambaServer.SendRate.Get())));
        break;
#endif

	case eRasPlot:
	{
		CNetMonitor::SDataRates RasRates = pNetMonitor->GetTotalDataRate(CNetMonitor::eRas);

		TextLines.append(tr("RAS & VPN Traffic:"));
		TextLines.append(tr("    Receive rate: %1").arg(FormatSize(RasRates.ReceiveRate)));
		TextLines.append(tr("    Send rate: %1").arg(FormatSize(RasRates.SendRate)));
		break;
	}
	case eNetworkPlot:
	{
		CNetMonitor::SDataRates NetRates = pNetMonitor->GetTotalDataRate(CNetMonitor::eNet);

		TextLines.append(tr("TCP/IP Traffic:"));
		TextLines.append(tr("    Receive rate: %1").arg(FormatSize(NetRates.ReceiveRate)));
		TextLines.append(tr("    Send rate: %1").arg(FormatSize(NetRates.SendRate)));
#ifdef WIN32
		if (((CWindowsAPI*)theAPI)->IsMonitoringETW() && theConf->GetBool("Options/ShowLanPlot", false))
		{
			TextLines.append(tr("    LAN Receive rate: %1").arg(FormatSize(SysStats.Lan.ReceiveRate.Get())));
			TextLines.append(tr("    LAN Send rate: %1").arg(FormatSize(SysStats.Lan.SendRate.Get())));
		}
#endif
		break;
	}
	case eGpuPlot:
	{
		QMap<QString, CGpuMonitor::SGpuInfo> GpuList = pGpuMonitor->GetAllGpuList();

		TextLines.append(tr("GPU Usage:"));
		foreach(const CGpuMonitor::SGpuInfo &GpuInfo, GpuList)
			TextLines.append(tr("    %1 usage: %2%").arg(GpuInfo.Description).arg(int(100*GpuInfo.TimeUsage)));
		break;
	}
	case eCpuPlot:
		TextLines.append(tr("CPU Usage:"));
		TextLines.append(tr("    User usage: %1%").arg(int(100*theAPI->GetCpuUserUsage())));
		TextLines.append(tr("    Kernel usage: %1%").arg(int(100*theAPI->GetCpuKernelUsage())));
		TextLines.append(tr("    DPC/IRQ usage: %1%").arg(int(100*theAPI->GetCpuDPCUsage())));
		break;
	}
	
	QToolTip::showText(helpEvent->globalPos(), TextLines.join("\n"));
}
