#include "stdafx.h"
#include "GPUView.h"
#include "../TaskExplorer.h"

CGPUView::CGPUView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QGridLayout();
	//m_pMainLayout->setMargin(3);
	this->setLayout(m_pMainLayout);

	QLabel* pLabel = new QLabel(tr("GPU"));
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


	QColor Back = QColor(0, 0, 64);
	QColor Front = QColor(187, 206, 239);
	QColor Grid = QColor(46, 44, 119);

	m_pGraphTabs = new QTabWidget();
	m_pGraphTabs->setTabPosition(QTabWidget::South);
	m_pGraphTabs->setDocumentMode(true);
	m_pScrollLayout->addWidget(m_pGraphTabs, 0, 0, 1, 3);

	m_pPlotWidget = new QWidget();
	m_pPlotLayout = new QVBoxLayout();
	m_pPlotLayout->setMargin(0);
	m_pPlotWidget->setLayout(m_pPlotLayout);
	m_pGraphTabs->addTab(m_pPlotWidget, tr("GPU Usage"));

	m_pGPUPlot = new CIncrementalPlot(Back, Front, Grid);
	m_pGPUPlot->setMinimumHeight(120);
	m_pGPUPlot->setMinimumWidth(50);
	m_pGPUPlot->SetupLegend(Front, tr("GPU Usage"), CIncrementalPlot::eDate, CIncrementalPlot::eAU);
	m_pGPUPlot->SetRagne(100);
	m_pPlotLayout->addWidget(m_pGPUPlot);
	

	m_pVRAMPlot = new CIncrementalPlot(Back, Front, Grid);
	m_pVRAMPlot->setMinimumHeight(120);
	m_pVRAMPlot->setMinimumWidth(50);
	m_pVRAMPlot->SetupLegend(Front, tr("VRAM Usage"), CIncrementalPlot::eDate, CIncrementalPlot::eBytes);
	m_pPlotLayout->addWidget(m_pVRAMPlot);
	
	// GPU Details
	m_pGPUList = new CPanelWidget<QTreeWidgetEx>();

	m_pGPUList->GetTree()->setItemDelegate(theGUI->GetItemDelegate());
	m_pGPUList->GetTree()->setHeaderLabels(tr("Model|Location|Driver Version|HwID|Dedicated Usage|Dedicated Limit|Shared Usage|Shared Limit|Device Interface").split("|"));
	m_pGPUList->GetTree()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pGPUList->GetTree()->setSortingEnabled(true);
	m_pGPUList->GetTree()->setMinimumHeight(100);
	m_pGPUList->GetTree()->setAutoFitMax(200 * theGUI->GetDpiScale());

	m_pScrollLayout->addWidget(m_pGPUList, 1, 0, 1, 3);
	//

	setObjectName(parent->objectName());
	QByteArray Columns = theConf->GetBlob(objectName() + "/GPUView_Columns");
	if (Columns.isEmpty())
	{
		m_pGPUList->GetView()->setColumnHidden(eDriverVersion, true);
		m_pGPUList->GetView()->setColumnHidden(eHwID, true);
		m_pGPUList->GetView()->setColumnHidden(eDeviceInterface, true);
	}
	else
		m_pGPUList->GetView()->header()->restoreState(Columns);
}

CGPUView::~CGPUView()
{
	theConf->SetBlob(objectName() + "/GPUView_Columns", m_pGPUList->GetView()->header()->saveState());
}

void CGPUView::Refresh()
{
	CGpuMonitor* pGpuMonitor = theAPI->GetGpuMonitor();
	
	QMap<QString, CGpuMonitor::SGpuInfo> GpuList = pGpuMonitor->GetAllGpuList();

	QMap<QString, QTreeWidgetItem*> OldAdapters;
	for (int i = 0; i < m_pGPUList->GetTree()->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem* pItem = m_pGPUList->GetTree()->topLevelItem(i);
		QString Device = pItem->data(0, Qt::UserRole).toString();
		Q_ASSERT(!OldAdapters.contains(Device));
		OldAdapters.insert(Device, pItem);
	}

	foreach(const CGpuMonitor::SGpuInfo &GpuInfo, GpuList)
	{
		QTreeWidgetItem* pItem = OldAdapters.take(GpuInfo.DeviceInterface);
		if (!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, GpuInfo.DeviceInterface);
			pItem->setText(eModelName, GpuInfo.Description);
			m_pGPUList->GetTree()->addTopLevelItem(pItem);

			pItem->setText(eLocation, GpuInfo.LocationInfo);
			pItem->setText(eDriverVersion, GpuInfo.DriverVersion);
			pItem->setText(eHwID, tr("VEN_%1&DEV_%2").arg(GpuInfo.VendorID, 2, 16).arg(GpuInfo.DeviceID, 2, 16));
		}

		pItem->setText(eDedicatedUsage,FormatSize(GpuInfo.Memory.DedicatedUsage));
		pItem->setText(eDedicatedLimit,FormatSize(GpuInfo.Memory.DedicatedLimit));
		pItem->setText(eSharedUsage,FormatSize(GpuInfo.Memory.SharedUsage));
		pItem->setText(eSharedLimit,FormatSize(GpuInfo.Memory.SharedLimit));

		pItem->setText(eDeviceInterface,GpuInfo.DeviceInterface);
	}

	foreach(QTreeWidgetItem* pItem, OldAdapters)
		delete pItem;
}

void CGPUView::UpdateGraphs()
{
	CGpuMonitor* pGpuMonitor = theAPI->GetGpuMonitor();

	QMap<QString, CGpuMonitor::SGpuInfo> GpuList = pGpuMonitor->GetAllGpuList();

	if (m_Adapters.count() != GpuList.count())
	{
		QColor Back = QColor(0, 0, 64);
		QColor Front = QColor(187, 206, 239);
		QColor Grid = QColor(46, 44, 119);

		QSet<QString> OldAdapters = m_Adapters;

		QVector<QColor> Colors = theGUI->GetPlotColors();
		foreach(const CGpuMonitor::SGpuInfo &GpuInfo, GpuList)
		{
			if (OldAdapters.contains(GpuInfo.DeviceInterface))
			{
				OldAdapters.remove(GpuInfo.DeviceInterface);
				continue;
			}
			m_Adapters.insert(GpuInfo.DeviceInterface);

			m_pGPUPlot->AddPlot("Gpu_" + GpuInfo.DeviceInterface, Colors[m_Adapters.size() % Colors.size()], Qt::SolidLine, false, GpuInfo.Description);
			m_pVRAMPlot->AddPlot("Gpu_" + GpuInfo.DeviceInterface, Colors[m_Adapters.size() % Colors.size()], Qt::SolidLine, false, GpuInfo.Description);
			//m_pVRAMPlot->AddPlot("GpuL_" + GpuInfo.DeviceInterface, Colors[m_Adapters.size() % Colors.size()], Qt::DotLine, false);
			m_pVRAMPlot->AddPlot("GpuS_" + GpuInfo.DeviceInterface, Colors[m_Adapters.size() % Colors.size()], Qt::DashLine, false);


			CIncrementalPlot* pPlot = new CIncrementalPlot(Back, Front, Grid);
			pPlot->setMinimumHeight(120);
			pPlot->setMinimumWidth(50);
			pPlot->SetRagne(100);
			pPlot->SetupLegend(Front, tr("Node Usage"), CIncrementalPlot::eDate, CIncrementalPlot::eAU);

			m_pGraphTabs->addTab(pPlot, GpuInfo.Description);
			m_NodePlots.insert(GpuInfo.DeviceInterface, pPlot);

			for (int j = 0; j < GpuInfo.Nodes.count(); j++)
				pPlot->AddPlot("Node_" + QString::number(j), Colors[j % Colors.size()], Qt::SolidLine, false, GpuInfo.Nodes[j].Name);
		}

		foreach(const QString& DeviceInterface, OldAdapters)
		{
			m_Adapters.remove(DeviceInterface);

			m_pGPUPlot->RemovePlot("Gpu_" + DeviceInterface);
			m_pVRAMPlot->RemovePlot("Gpu_" + DeviceInterface);
			//m_pVRAMPlot->RemovePlot("GpuL_" + DeviceInterface);
			m_pVRAMPlot->RemovePlot("GpuS_" + DeviceInterface);

			delete m_NodePlots.take(DeviceInterface);
		}
	}

	foreach(const CGpuMonitor::SGpuInfo &GpuInfo, GpuList)
	{
		m_pGPUPlot->AddPlotPoint("Gpu_" + GpuInfo.DeviceInterface, 100 * GpuInfo.TimeUsage);
	
		m_pVRAMPlot->AddPlotPoint("Gpu_" + GpuInfo.DeviceInterface, GpuInfo.Memory.DedicatedUsage);
		//m_pVRAMPlot->AddPlotPoint("GpuL_" + GpuInfo.DeviceInterface, GpuInfo.Memory.DedicatedLimit);
		m_pVRAMPlot->AddPlotPoint("GpuS_" + GpuInfo.DeviceInterface, GpuInfo.Memory.SharedUsage);

		CIncrementalPlot* pPlot = m_NodePlots.value(GpuInfo.DeviceInterface);
		if (pPlot)
		{
			for (int j = 0; j < GpuInfo.Nodes.count(); j++)
				pPlot->AddPlotPoint("Node_" + QString::number(j), 100 * GpuInfo.Nodes[j].TimeUsage);
		}
	}
}