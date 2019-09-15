#include "stdafx.h"
#include "DiskView.h"
#include "../TaskExplorer.h"
#ifdef WIN32
#include "../../API/Windows/WindowsAPI.h"
#endif

CDiskView::CDiskView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QGridLayout();
	//m_pMainLayout->setMargin(3);
	this->setLayout(m_pMainLayout);

	QLabel* pLabel = new QLabel(tr("Disks"));
	m_pMainLayout->addWidget(pLabel, 0, 0);
	QFont font = pLabel->font();
	font.setPointSize(font.pointSize() * 1.5);
	pLabel->setFont(font);

	m_pMainLayout->addItem(new QSpacerItem(20, 30, QSizePolicy::Minimum, QSizePolicy::Minimum), 0, 1);
	//m_pGPUModel = new QLabel();
	//m_pMainLayout->addWidget(m_pGPUModel, 0, 2);
	//m_pGPUModel->setFont(font);

	m_pScrollWidget = new QWidget();
	m_pScrollArea = new QScrollArea();
	m_pScrollLayout = new QGridLayout();
	m_pScrollLayout->setMargin(0);
	m_pScrollWidget->setLayout(m_pScrollLayout);
	m_pScrollArea->setFrameShape(QFrame::NoFrame);
	m_pScrollArea->setWidgetResizable(true);
	m_pScrollArea->setWidget(m_pScrollWidget);
	m_pMainLayout->addWidget(m_pScrollArea, 1, 0, 1, 3);
	QPalette pal = m_pScrollArea->palette();
	pal.setColor(QPalette::Background, Qt::transparent);
	m_pScrollArea->setPalette(pal);

	m_PlotLimit = theGUI->GetGraphLimit(true);
	connect(theGUI, SIGNAL(ReloadPlots()), this, SLOT(ReConfigurePlots()));
	QColor Back = theGUI->GetColor(CTaskExplorer::ePlotBack);
	QColor Front = theGUI->GetColor(CTaskExplorer::ePlotFront);
	QColor Grid = theGUI->GetColor(CTaskExplorer::ePlotGrid);

	m_pGraphTabs = new QTabWidget();
	m_pGraphTabs->setTabPosition(QTabWidget::South);
	m_pGraphTabs->setDocumentMode(true);
	m_pScrollLayout->addWidget(m_pGraphTabs, 0, 0, 1, 3);

	m_pDiskPlot = new CIncrementalPlot(Back, Front, Grid);
	m_pDiskPlot->setMinimumHeight(120);
	m_pDiskPlot->setMinimumWidth(50);
	m_pDiskPlot->SetupLegend(Front, tr("Disk Usage"), CIncrementalPlot::eDate);
	m_pDiskPlot->SetRagne(100);
	m_pDiskPlot->SetLimit(m_PlotLimit);
	m_pGraphTabs->addTab(m_pDiskPlot, tr("Disk Usage"));
	
	m_pWRPlotWidget = new QWidget();
	m_pWRPlotLayout = new QVBoxLayout();
	m_pWRPlotLayout->setMargin(0);
	m_pWRPlotWidget->setLayout(m_pWRPlotLayout);
	m_pGraphTabs->addTab(m_pWRPlotWidget, tr("Data Rates"));

	m_pReadPlot = new CIncrementalPlot(Back, Front, Grid);
	m_pReadPlot->setMinimumHeight(120);
	m_pReadPlot->setMinimumWidth(50);
	m_pReadPlot->SetupLegend(Front, tr("Read Rate"), CIncrementalPlot::eDate, CIncrementalPlot::eBytes);
	m_pReadPlot->SetLimit(m_PlotLimit);
	m_pWRPlotLayout->addWidget(m_pReadPlot);

	m_pWritePlot = new CIncrementalPlot(Back, Front, Grid);
	m_pWritePlot->setMinimumHeight(120);
	m_pWritePlot->setMinimumWidth(50);
	m_pWritePlot->SetupLegend(Front, tr("Write Rate"), CIncrementalPlot::eDate, CIncrementalPlot::eBytes);
	m_pWritePlot->SetLimit(m_PlotLimit);
	m_pWRPlotLayout->addWidget(m_pWritePlot);

	m_bHasUnSupported = false;

	m_pIOPlotWidget = new QWidget();
	m_pIOPlotLayout = new QVBoxLayout();
	m_pIOPlotLayout->setMargin(0);
	m_pIOPlotWidget->setLayout(m_pIOPlotLayout);
	m_pGraphTabs->addTab(m_pIOPlotWidget, tr("IO Rates"));

	m_pFileIOPlot = new CIncrementalPlot(Back, Front, Grid);
	m_pFileIOPlot->setMinimumHeight(120);
	m_pFileIOPlot->setMinimumWidth(50);
	m_pFileIOPlot->SetupLegend(Front, tr("File IO"), CIncrementalPlot::eDate, CIncrementalPlot::eBytes);
	m_pFileIOPlot->SetLimit(m_PlotLimit);
	m_pIOPlotLayout->addWidget(m_pFileIOPlot);

	m_pFileIOPlot->AddPlot("FileIO_Read", Qt::green, Qt::SolidLine, false, tr("Read Rate"));
	m_pFileIOPlot->AddPlot("FileIO_Write", Qt::red, Qt::SolidLine, false, tr("Write Rate"));
	m_pFileIOPlot->AddPlot("FileIO_Other", Qt::blue, Qt::SolidLine, false, tr("Other Rate"));

	m_pMMapIOPlot = new CIncrementalPlot(Back, Front, Grid);
	m_pMMapIOPlot->setMinimumHeight(120);
	m_pMMapIOPlot->setMinimumWidth(50);
	m_pMMapIOPlot->SetupLegend(Front, tr("MMap IO"), CIncrementalPlot::eDate, CIncrementalPlot::eBytes);
	m_pMMapIOPlot->SetLimit(m_PlotLimit);
	m_pIOPlotLayout->addWidget(m_pMMapIOPlot);

	m_pMMapIOPlot->AddPlot("MMapIO_Read", Qt::green, Qt::SolidLine, false, tr("Read Rate"));
	m_pMMapIOPlot->AddPlot("MMapIO_Write", Qt::red, Qt::SolidLine, false, tr("Write Rate"));


	// Disk Details
	m_pDiskList = new CPanelWidgetEx();

	m_pDiskList->GetTree()->setItemDelegate(theGUI->GetItemDelegate());
	m_pDiskList->GetTree()->setHeaderLabels(tr("Disk Name|Usage|Latency|Queue|Read Rate|Bytes Read Delta|Bytes Read|Reads Delta|Reads|Write Rate|Bytes Writen Delta|Bytes Writen|Writes Delta|Writes|Device Path").split("|"));
	m_pDiskList->GetTree()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pDiskList->GetTree()->setSortingEnabled(true);
	m_pDiskList->GetTree()->setMinimumHeight(100);
	m_pDiskList->GetTree()->setAutoFitMax(200 * theGUI->GetDpiScale());

	m_pDiskList->GetTree()->setColumnReset(2);
	connect(m_pDiskList->GetTree(), SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));

	m_pScrollLayout->addWidget(m_pDiskList, 2, 0, 1, 3);
	//

	setObjectName(parent->objectName());
	QByteArray Columns = theConf->GetBlob(objectName() + "/DiskView_Columns");
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pDiskList->GetView()->header()->restoreState(Columns);

	m_pGraphTabs->setCurrentIndex(theConf->GetInt("Options/DiskTab", 0));
}

CDiskView::~CDiskView()
{
	theConf->SetValue("Options/DiskTab", m_pGraphTabs->currentIndex());
	//theConf->SetValue("Options/CurrentDisk", m_pDiskDrives->currentIndex());
	theConf->SetBlob(objectName() + "/DiskView_Columns", m_pDiskList->GetView()->header()->saveState());
}

void CDiskView::OnResetColumns()
{
	for (int i = 0; i < eCount; i++)
		m_pDiskList->GetView()->setColumnHidden(i, true);

	m_pDiskList->GetView()->setColumnHidden(eDiskName, false);
	m_pDiskList->GetView()->setColumnHidden(eActivityTime, false);
	m_pDiskList->GetView()->setColumnHidden(eResponseTime, false);

	m_pDiskList->GetView()->setColumnHidden(eReadRate, false);
	m_pDiskList->GetView()->setColumnHidden(eBytesRead, false);
	m_pDiskList->GetView()->setColumnHidden(eWriteRate, false);
	m_pDiskList->GetView()->setColumnHidden(eBytesWriten, false);

	m_pDiskList->GetView()->setColumnHidden(eDevicePath, false);
}

void CDiskView::ReConfigurePlots()
{
	m_PlotLimit = theGUI->GetGraphLimit(true);
	QColor Back = theGUI->GetColor(CTaskExplorer::ePlotBack);
	QColor Front = theGUI->GetColor(CTaskExplorer::ePlotFront);
	QColor Grid = theGUI->GetColor(CTaskExplorer::ePlotGrid);

	m_pDiskPlot->SetLimit(m_PlotLimit);
	m_pDiskPlot->SetColors(Back, Front, Grid);
	m_pReadPlot->SetLimit(m_PlotLimit);
	m_pReadPlot->SetColors(Back, Front, Grid);
	m_pWritePlot->SetLimit(m_PlotLimit);
	m_pWritePlot->SetColors(Back, Front, Grid);
	m_pFileIOPlot->SetLimit(m_PlotLimit);
	m_pFileIOPlot->SetColors(Back, Front, Grid);
	m_pMMapIOPlot->SetLimit(m_PlotLimit);
	m_pMMapIOPlot->SetColors(Back, Front, Grid);
}

void CDiskView::Refresh()
{
	CDiskMonitor* pDiskMonitor = theAPI->GetDiskMonitor();

	QMap<QString, CDiskMonitor::SDiskInfo> DiskList = pDiskMonitor->GetDiskList();

	QMap<QString, QTreeWidgetItem*> OldDisks;
	for (int i = 0; i < m_pDiskList->GetTree()->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem* pItem = m_pDiskList->GetTree()->topLevelItem(i);
		QString DevicePath = pItem->data(0, Qt::UserRole).toString();
		Q_ASSERT(!OldDisks.contains(DevicePath));
		OldDisks.insert(DevicePath, pItem);
	}

	for (QMap<QString, CDiskMonitor::SDiskInfo>::iterator I = DiskList.begin(); I != DiskList.end(); ++I)
	{
		CDiskMonitor::SDiskInfo &DiskInfo = I.value();

		QTreeWidgetItem* pItem = OldDisks.take(I.key());
		if (!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, I.key());
			pItem->setText(eDiskName, DiskInfo.DeviceMountPoints);
			m_pDiskList->GetTree()->addTopLevelItem(pItem);
		}

		if (DiskInfo.DeviceSupported)
		{
			pItem->setText(eActivityTime, tr("%1 %").arg((int)DiskInfo.ActiveTime));
			pItem->setText(eResponseTime, tr("%1 ms").arg(DiskInfo.ResponseTime, 2, 'g', 2));
			pItem->setText(eQueueLength, QString::number(DiskInfo.QueueDepth));

			pItem->setText(eReadRate, FormatRate(DiskInfo.ReadRate.Get()));
			pItem->setText(eBytesRead, FormatSize(DiskInfo.ReadRawDelta.Value));
			pItem->setText(eBytesReadDelta, FormatSize(DiskInfo.ReadRawDelta.Delta));
			pItem->setText(eReads, FormatNumber(DiskInfo.ReadDelta.Value));
			pItem->setText(eReadsDelta, FormatNumber(DiskInfo.ReadDelta.Delta));
			pItem->setText(eWriteRate, FormatRate(DiskInfo.WriteRate.Get()));
			pItem->setText(eBytesWriten, FormatSize(DiskInfo.WriteRawDelta.Value));
			pItem->setText(eBytesWritenDelta, FormatSize(DiskInfo.WriteRawDelta.Delta));
			pItem->setText(eWrites, FormatNumber(DiskInfo.WriteDelta.Value));
			pItem->setText(eWritesDelta, FormatNumber(DiskInfo.WriteDelta.Delta));
		}
		else
		{
			for(int i=eActivityTime; i <= eWritesDelta; i++ )
				pItem->setText(i, tr("N/A"));
		}

		pItem->setText(eDevicePath, DiskInfo.DevicePath);
	}

	foreach(QTreeWidgetItem* pItem, OldDisks)
		delete pItem;
}

void CDiskView::UpdateGraphs()
{
	CDiskMonitor* pDiskMonitor = theAPI->GetDiskMonitor();

	CDiskMonitor::SDataRates DiskRates = pDiskMonitor->GetAllDiskDataRates();
	QMap<QString, CDiskMonitor::SDiskInfo> DiskList = pDiskMonitor->GetDiskList();

	if (m_Disks.size() != DiskRates.Supported)
	{
		QSet<QString> OldDisks = m_Disks;

		QVector<QColor> Colors = theGUI->GetPlotColors();
		foreach(const CDiskMonitor::SDiskInfo& Disk, DiskList)
		{
			if (!Disk.DeviceSupported)
				continue;

			if (OldDisks.contains(Disk.DevicePath))
			{
				OldDisks.remove(Disk.DevicePath);
				continue;
			}
			m_Disks.insert(Disk.DevicePath);

			m_pDiskPlot->AddPlot("Disk_" + Disk.DevicePath, Colors[m_Disks.size() % Colors.size()], Qt::SolidLine, false, Disk.DeviceName);

			m_pReadPlot->AddPlot("Disk_" + Disk.DevicePath, Colors[m_Disks.size() % Colors.size()], Qt::SolidLine, false, Disk.DeviceName);
			m_pWritePlot->AddPlot("Disk_" + Disk.DevicePath, Colors[m_Disks.size() % Colors.size()], Qt::SolidLine, false, Disk.DeviceName);
		}

		foreach(const QString& DevicePath, OldDisks)
		{
			m_Disks.remove(DevicePath);

			m_pDiskPlot->RemovePlot("Disk_" + DevicePath);

			m_pReadPlot->RemovePlot("Disk_" + DevicePath);
			m_pWritePlot->RemovePlot("Disk_" + DevicePath);
		}

		m_pDiskPlot->SetRagne(100);
	}

	if ((DiskRates.Supported != DiskRates.DiskCount) != m_bHasUnSupported)
	{
		m_bHasUnSupported = (DiskRates.Supported != DiskRates.DiskCount);

		if (!m_bHasUnSupported)
		{
			m_pReadPlot->RemovePlot("Disk_UnSupported");
			m_pWritePlot->RemovePlot("Disk_UnSupported");
		}
		else
		{
			QVector<QColor> Colors = theGUI->GetPlotColors();
			m_pReadPlot->AddPlot("Disk_UnSupported", Colors[(m_Disks.size() + 1) % Colors.size()], Qt::SolidLine, false, tr("Other Disks (unsupported)"));
			m_pWritePlot->AddPlot("Disk_UnSupported", Colors[(m_Disks.size() + 1) % Colors.size()], Qt::SolidLine, false, tr("Other Disks (unsupported)"));
		}
	}

	bool useDiskCounters = false;

	quint64 SummReadRate = 0;
	quint64 SummWriteRate = 0;

	foreach(const CDiskMonitor::SDiskInfo& Disk, DiskList)
	{
		if (!Disk.DeviceSupported)
			continue;

		m_pDiskPlot->AddPlotPoint("Disk_" + Disk.DevicePath, Disk.ActiveTime);

		m_pReadPlot->AddPlotPoint("Disk_" + Disk.DevicePath, Disk.ReadRate.Get());
		m_pWritePlot->AddPlotPoint("Disk_" + Disk.DevicePath, Disk.WriteRate.Get());

		SummReadRate += Disk.ReadRate.Get();
		SummWriteRate += Disk.WriteRate.Get();
	}

	SSysStats SysStats = theAPI->GetStats();

	m_pFileIOPlot->AddPlotPoint("FileIO_Read", SysStats.Io.ReadRate.Get());
	m_pFileIOPlot->AddPlotPoint("FileIO_Write", SysStats.Io.WriteRate.Get());
	m_pFileIOPlot->AddPlotPoint("FileIO_Other", SysStats.Io.OtherRate.Get());

	m_pMMapIOPlot->AddPlotPoint("MMapIO_Read", SysStats.MMapIo.ReadRate.Get());
	m_pMMapIOPlot->AddPlotPoint("MMapIO_Write", SysStats.MMapIo.WriteRate.Get());

	if(m_bHasUnSupported)
	{
		quint64 OtherReadRate = SysStats.Disk.ReadRate.Get();
		if (OtherReadRate > SummReadRate)
			OtherReadRate -= SummReadRate;
		else
			OtherReadRate = 0;

		quint64 OtherWriteRate = SysStats.Disk.WriteRate.Get();
		if (OtherWriteRate > SummWriteRate)
			OtherWriteRate -= SummWriteRate;
		else
			OtherWriteRate = 0;

		m_pReadPlot->AddPlotPoint("Disk_UnSupported", OtherReadRate);
		m_pWritePlot->AddPlotPoint("Disk_UnSupported", OtherWriteRate);
	}
}