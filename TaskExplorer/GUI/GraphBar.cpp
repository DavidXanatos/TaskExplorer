#include "stdafx.h"
#include "GraphBar.h"
#include "TaskExplorer.h"
#ifdef WIN32
#include "..\API\Windows\WindowsAPI.h"
#endif

CGraphBar::CGraphBar()
{
	this->setFixedHeight(80); /// todo add more sized

	m_pMainLayout = new QGridLayout();
	this->setLayout(m_pMainLayout);

	m_pMainLayout->setMargin(1);
	m_pMainLayout->setSpacing(2);

	QPalette pal = palette();
	pal.setColor(QPalette::Background, Qt::gray);
	this->setAutoFillBackground(true);
	this->setPalette(pal);

	// 5:30 Min's length

	int limit = 660;
#ifdef _DEBUG
	limit = 330;
#endif

	int row = 0;
	int column = 0;
	m_pMemoryPlot = new CIncrementalPlot();
	m_pMemoryPlot->SetLimit(limit);
	m_pMemoryPlot->AddPlot("Commited", Qt::green, Qt::SolidLine, true);
	m_pMemoryPlot->AddPlot("Swapped", Qt::red, Qt::SolidLine, true);
	m_pMemoryPlot->AddPlot("Cache", Qt::blue, Qt::SolidLine, true);
	m_pMemoryPlot->AddPlot("Physical", Qt::yellow, Qt::SolidLine, true);
	m_pMemoryPlot->AddPlot("Limit", Qt::white, Qt::SolidLine);
	m_pMemoryPlot->AddPlot("end", Qt::transparent, Qt::NoPen); // add a dummy curne that always stays at 0 in order to force autoscale to keep the lower bound always at 0
	m_pMainLayout->addWidget(m_pMemoryPlot, row, column++);

	m_pObjectPlot = new CIncrementalPlot();
	m_pObjectPlot->SetLimit(limit);
	m_pObjectPlot->AddPlot("Gdi", Qt::green, Qt::SolidLine);
	m_pObjectPlot->AddPlot("User", Qt::red, Qt::SolidLine);
	m_pObjectPlot->AddPlot("end", Qt::transparent, Qt::NoPen);
	m_pMainLayout->addWidget(m_pObjectPlot, row, column++);

	m_pWindowsPlot = new CIncrementalPlot();
	m_pWindowsPlot->SetLimit(limit);
	// TODO
	m_pWindowsPlot->AddPlot("end", Qt::transparent, Qt::NoPen);
	m_pMainLayout->addWidget(m_pWindowsPlot, row, column++);

	m_pHandledPlot = new CIncrementalPlot();
	m_pHandledPlot->SetLimit(limit);
	m_pHandledPlot->AddPlot("Handles", Qt::green, Qt::SolidLine);
	m_pHandledPlot->AddPlot("end", Qt::transparent, Qt::NoPen);
	m_pMainLayout->addWidget(m_pHandledPlot, row, column++);

	m_pMMapIoPlot = new CIncrementalPlot();
	m_pMMapIoPlot->SetLimit(limit);
	m_pMMapIoPlot->AddPlot("Read", Qt::green, Qt::SolidLine);
	m_pMMapIoPlot->AddPlot("Write", Qt::red, Qt::SolidLine);
	m_pMMapIoPlot->AddPlot("end", Qt::transparent, Qt::NoPen);
	m_pMainLayout->addWidget(m_pMMapIoPlot, row, column++);


	row = 1;
	column = 0;
	m_pFileIoPlot = new CIncrementalPlot();
	m_pFileIoPlot->SetLimit(limit);
	m_pFileIoPlot->AddPlot("Read", Qt::green, Qt::SolidLine);
	m_pFileIoPlot->AddPlot("Write", Qt::red, Qt::SolidLine);
	m_pFileIoPlot->AddPlot("Other", Qt::yellow, Qt::SolidLine);
	m_pFileIoPlot->AddPlot("end", Qt::transparent, Qt::NoPen);
	m_pMainLayout->addWidget(m_pFileIoPlot, row, column++);

	m_pSambaPlot = new CIncrementalPlot();
	m_pSambaPlot->SetLimit(limit);
	m_pSambaPlot->AddPlot("RecvTotal", Qt::green, Qt::SolidLine);
	m_pSambaPlot->AddPlot("SentTotal", Qt::red, Qt::SolidLine);
	m_pSambaPlot->AddPlot("RecvServer", Qt::green, Qt::DashLine);
	m_pSambaPlot->AddPlot("SentServer", Qt::red, Qt::DashLine);
	m_pSambaPlot->AddPlot("RecvClient", Qt::green, Qt::DotLine);
	m_pSambaPlot->AddPlot("SentClient", Qt::red, Qt::DotLine);
	m_pSambaPlot->AddPlot("end", Qt::transparent, Qt::NoPen);
	m_pMainLayout->addWidget(m_pSambaPlot, row, column++);

	m_pNetworkPlot = new CIncrementalPlot();
	m_pNetworkPlot->SetLimit(limit);
	m_pNetworkPlot->AddPlot("Recv", Qt::green, Qt::SolidLine);
	m_pNetworkPlot->AddPlot("Send", Qt::red, Qt::SolidLine);
	m_pNetworkPlot->AddPlot("end", Qt::transparent, Qt::NoPen);
	m_pMainLayout->addWidget(m_pNetworkPlot, row, column++);

	m_pGpuPlot = new CIncrementalPlot();
	m_pGpuPlot->SetLimit(limit);
	// TODO
	m_pGpuPlot->AddPlot("end", Qt::transparent, Qt::NoPen);
	m_pMainLayout->addWidget(m_pGpuPlot, row, column++);

	m_pCpuPlot = new CIncrementalPlot();
	m_pCpuPlot->SetLimit(limit);
	m_pCpuPlot->SetRagne(100);
	m_pCpuPlot->AddPlot("User", Qt::green, Qt::SolidLine, true);
	m_pCpuPlot->AddPlot("Kernel", Qt::red, Qt::SolidLine, true);
	m_pCpuPlot->AddPlot("DPC", Qt::blue, Qt::SolidLine, true);
	m_pCpuPlot->AddPlot("end", Qt::transparent, Qt::NoPen);
	m_pMainLayout->addWidget(m_pCpuPlot, row, column++);

}


CGraphBar::~CGraphBar()
{
}


void CGraphBar::UpdateGraphs()
{
	SProcStats Stats = theAPI->GetStats();

	m_pMemoryPlot->SetText(tr("Memory=%1%").arg((int)100*theAPI->GetPhysicalUsed()/theAPI->GetInstalledMemory()));
	m_pMemoryPlot->SetRagne(theAPI->GetMemoryLimit());
	m_pMemoryPlot->AddPlotPoint("Commited", theAPI->GetCommitedMemory());
	m_pMemoryPlot->AddPlotPoint("Swapped", theAPI->GetPhysicalUsed() + theAPI->GetPagedMemory());
	m_pMemoryPlot->AddPlotPoint("Cache", theAPI->GetPhysicalUsed());
	m_pMemoryPlot->AddPlotPoint("Physical", theAPI->GetPhysicalUsed() - theAPI->GetCacheMemory());
	m_pMemoryPlot->AddPlotPoint("Limit", theAPI->GetInstalledMemory());

#ifdef WIN32
	m_pObjectPlot->SetText(tr("Objects<%1").arg(FormatUnit(m_pObjectPlot->GetRangeMax())));
	QStringList ObjectInfo;
	ObjectInfo.append(FormatUnit(((CWindowsAPI*)theAPI)->GetTotalGuiObjects(), 1));
	ObjectInfo.append(FormatUnit(((CWindowsAPI*)theAPI)->GetTotalUserObjects(), 1));
	m_pObjectPlot->SetTexts(ObjectInfo);
	m_pObjectPlot->AddPlotPoint("Gdi", ((CWindowsAPI*)theAPI)->GetTotalGuiObjects());
	m_pObjectPlot->AddPlotPoint("User", ((CWindowsAPI*)theAPI)->GetTotalUserObjects());
#endif

	m_pHandledPlot->SetText(tr("Handles<%1").arg(FormatUnit(m_pHandledPlot->GetRangeMax())));
	m_pHandledPlot->AddPlotPoint("Handles", theAPI->GetTotalHandles());

	m_pMMapIoPlot->SetText(tr("MMapIO<%1").arg(FormatSize(m_pMMapIoPlot->GetRangeMax(), 0)));
	QStringList MMapIoInfo;
	MMapIoInfo.append(FormatUnit(Stats.Disk.ReadRate.Get(), 0));
	MMapIoInfo.append(FormatUnit(Stats.Disk.WriteRate.Get(), 0));
	m_pMMapIoPlot->SetTexts(MMapIoInfo);
	m_pMMapIoPlot->AddPlotPoint("Read", Stats.Disk.ReadRate.Get());
	m_pMMapIoPlot->AddPlotPoint("Write", Stats.Disk.WriteRate.Get());

	m_pFileIoPlot->SetText(tr("FileIO<%1").arg(FormatSize(m_pFileIoPlot->GetRangeMax(), 0)));
	QStringList FileIoInfo;
	FileIoInfo.append(FormatUnit(Stats.Io.ReadRate.Get(), 0));
	FileIoInfo.append(FormatUnit(Stats.Io.WriteRate.Get(), 0));
	FileIoInfo.append(FormatUnit(Stats.Io.OtherRate.Get(), 0));
	m_pFileIoPlot->SetTexts(FileIoInfo);
	m_pFileIoPlot->AddPlotPoint("Read", Stats.Io.ReadRate.Get());
	m_pFileIoPlot->AddPlotPoint("Write", Stats.Io.WriteRate.Get());
	m_pFileIoPlot->AddPlotPoint("Other", Stats.Io.OtherRate.Get());


	SSambaStats SambaStats = theAPI->GetSambaStats();

	m_pSambaPlot->SetText(tr("Samba<%1").arg(FormatSize(m_pSambaPlot->GetRangeMax(), 0)));
	QStringList SambaInfo;
	SambaInfo.append(FormatUnit(SambaStats.Client.ReceiveRate.Get() + SambaStats.Server.ReceiveRate.Get(), 0));
	SambaInfo.append(FormatUnit(SambaStats.Client.SendRate.Get() + SambaStats.Server.SendRate.Get(), 0));
	m_pSambaPlot->SetTexts(SambaInfo);
	m_pSambaPlot->AddPlotPoint("RecvTotal", SambaStats.Client.ReceiveRate.Get() + SambaStats.Server.ReceiveRate.Get());
	m_pSambaPlot->AddPlotPoint("SentTotal", SambaStats.Client.SendRate.Get() + SambaStats.Server.SendRate.Get());
	m_pSambaPlot->AddPlotPoint("RecvServer", SambaStats.Server.ReceiveRate.Get());
	m_pSambaPlot->AddPlotPoint("SentServer", SambaStats.Server.SendRate.Get());
	m_pSambaPlot->AddPlotPoint("RecvClient", SambaStats.Client.ReceiveRate.Get());
	m_pSambaPlot->AddPlotPoint("SentClient", SambaStats.Client.SendRate.Get() );


	m_pNetworkPlot->SetText(tr("TCP/IP<%1").arg(FormatSize(m_pNetworkPlot->GetRangeMax(), 0)));
	QStringList NetInfo;
	NetInfo.append(FormatUnit(Stats.Net.ReceiveRate.Get(), 0));
	NetInfo.append(FormatUnit(Stats.Net.SendRate.Get(), 0));
	m_pNetworkPlot->SetTexts(NetInfo);
	m_pNetworkPlot->AddPlotPoint("Recv", Stats.Net.ReceiveRate.Get());
	m_pNetworkPlot->AddPlotPoint("Send", Stats.Net.SendRate.Get());

	m_pCpuPlot->SetText(tr("Cpu=%1%").arg((int)100*theAPI->GetCpuUsage()));
	QStringList CpuInfo;
	CpuInfo.append(tr("%1%").arg((int)100*theAPI->GetCpuUserUsage(), 0));
	CpuInfo.append(tr("%1%").arg((int)100*theAPI->GetCpuKernelUsage(), 0));
	CpuInfo.append(tr("%1%").arg((int)100*theAPI->GetCpuDPCUsage(), 0));
	m_pCpuPlot->SetTexts(FileIoInfo);
	m_pCpuPlot->AddPlotPoint("User", theAPI->GetCpuUsage()*100);
	m_pCpuPlot->AddPlotPoint("Kernel", theAPI->GetCpuKernelUsage()*100 + theAPI->GetCpuDPCUsage()*100);
	m_pCpuPlot->AddPlotPoint("DPC", theAPI->GetCpuDPCUsage()*100);
}