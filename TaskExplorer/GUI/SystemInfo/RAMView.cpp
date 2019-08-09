#include "stdafx.h"
#include "RAMView.h"
#include "../TaskExplorer.h"
#ifdef WIN32
#include "../../API/Windows/ProcessHacker.h"
#endif


CRAMView::CRAMView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QGridLayout();
	//m_pMainLayout->setMargin(3);
	this->setLayout(m_pMainLayout);

	QLabel* pLabel = new QLabel(tr("Memory"));
	m_pMainLayout->addWidget(pLabel, 0, 0);
	QFont font = pLabel->font();
	font.setPointSize(font.pointSize() * 1.5);
	pLabel->setFont(font);

	m_pMainLayout->addItem(new QSpacerItem(20, 30, QSizePolicy::Minimum, QSizePolicy::Minimum), 0, 1);
	m_pRAMSize = new QLabel();
	m_pMainLayout->addWidget(m_pRAMSize, 0, 2);
	m_pRAMSize->setFont(font);

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

	m_pRAMPlot = new CIncrementalPlot(Back, Front, Grid);
	m_pRAMPlot->setMinimumHeight(120);
	m_pRAMPlot->setMinimumWidth(50);
	m_pRAMPlot->SetupLegend(Front, tr("Memory Usage"), CIncrementalPlot::eDate, CIncrementalPlot::eBytes);
	m_pScrollLayout->addWidget(m_pRAMPlot, 0, 0, 1, 3);

	m_pRAMPlot->AddPlot("Commited", Qt::green, Qt::SolidLine, false, tr("Commit charge"));
	m_pRAMPlot->AddPlot("Swapped", Qt::red, Qt::SolidLine, false, tr("Swap memory"));
	m_pRAMPlot->AddPlot("Cache", Qt::blue, Qt::SolidLine, false, tr("Cache"));
	m_pRAMPlot->AddPlot("Physical", Qt::yellow, Qt::SolidLine, false, tr("Physical memory"));

	m_pInfoTabs = new QTabWidget();
	//m_pInfoTabs->setDocumentMode(true);
	m_pScrollLayout->addWidget(m_pInfoTabs, 1, 0, 1, 3);


	////////////////////////////////////////

	m_pMemoryWidget = new QWidget();
	m_pMemoryLayout = new QGridLayout();
	m_pMemoryLayout->setMargin(3);
	m_pMemoryWidget->setLayout(m_pMemoryLayout);
	m_pInfoTabs->addTab(m_pMemoryWidget, tr("Memory"));

	m_pVMemBox = new QGroupBox(tr("Commit charge"));
	m_pVMemBox->setMinimumWidth(150);
	m_pVMemLayout = new QGridLayout();
	m_pVMemBox->setLayout(m_pVMemLayout);
	m_pMemoryLayout->addWidget(m_pVMemBox, 0, 0);
	//m_pMemoryLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 1, 0, 2, 1);
	m_pMemoryLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 1, 0);
	

	int row = 0;
	m_pVMemLayout->addWidget(new QLabel(tr("Usage")), row, 0);
	m_pVMemCurrent = new QLabel();
	m_pVMemLayout->addWidget(m_pVMemCurrent, row++, 1);

	m_pVMemLayout->addWidget(new QLabel(tr("Peak usage")), row, 0);
	m_pVMemPeak = new QLabel();
	m_pVMemLayout->addWidget(m_pVMemPeak, row++, 1);

	m_pVMemLayout->addWidget(new QLabel(tr("Limit")), row, 0);
	m_pVMemLimit = new QLabel();
	m_pVMemLayout->addWidget(m_pVMemLimit, row++, 1);

	m_pVMemLayout->addWidget(new QLabel(tr("Swap space")), row, 0);
	m_pSwapSize = new QLabel();
	m_pVMemLayout->addWidget(m_pSwapSize, row++, 1);


	m_pRAMBox = new QGroupBox(tr("Physical memory"));
	m_pRAMBox->setMinimumWidth(150);
	m_pRAMLayout = new QGridLayout();
	m_pRAMBox->setLayout(m_pRAMLayout);
	m_pMemoryLayout->addWidget(m_pRAMBox, 0, 1, 2, 1);
	//m_pMemoryLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 2, 0);

	row = 0;
	m_pRAMLayout->addWidget(new QLabel(tr("Usage")), row, 0);
	m_pRAMUsed = new QLabel();
	m_pRAMLayout->addWidget(m_pRAMUsed, row++, 1);

	m_pRAMLayout->addWidget(new QLabel(tr("Total usage")), row, 0);
	m_pRAMTotal = new QLabel();
	m_pRAMLayout->addWidget(m_pRAMTotal, row++, 1);

	m_pRAMLayout->addWidget(new QLabel(tr("HW Reserved")), row, 0);
	m_pRAMReserved = new QLabel();
	m_pRAMLayout->addWidget(m_pRAMReserved, row++, 1);

	m_pRAMLayout->addWidget(new QLabel(tr("Cache WS")), row, 0);
	m_pRAMCacheWS = new QLabel();
	m_pRAMLayout->addWidget(m_pRAMCacheWS, row++, 1);

	m_pRAMLayout->addWidget(new QLabel(tr("Kernel WS")), row, 0);
	m_pRAMKernelWS = new QLabel();
	m_pRAMLayout->addWidget(m_pRAMKernelWS, row++, 1);

	m_pRAMLayout->addWidget(new QLabel(tr("Driver WS")), row, 0);
	m_pRAMDriverWS = new QLabel();
	m_pRAMLayout->addWidget(m_pRAMDriverWS, row++, 1);


	m_pMemoryLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 3);
	////////////////////////////////////////
	
	m_pPoolWidget = new QWidget();
	m_pPoolLayout = new QGridLayout();
	m_pPoolLayout->setMargin(3);
	m_pPoolWidget->setLayout(m_pPoolLayout);
	m_pInfoTabs->addTab(m_pPoolWidget, tr("Pool"));


	m_pPPoolBox = new QGroupBox(tr("Paged pool"));
	m_pPPoolBox->setMinimumWidth(150);
	m_pPPoolLayout = new QGridLayout();
	m_pPPoolBox->setLayout(m_pPPoolLayout);
	m_pPoolLayout->addWidget(m_pPPoolBox, 0, 0);
	m_pPoolLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 1, 0);

	row = 0;
	m_pPPoolLayout->addWidget(new QLabel(tr("Working set")), row, 0);
	m_pPPoolWS = new QLabel();
	m_pPPoolLayout->addWidget(m_pPPoolWS, row++, 1);

	m_pPPoolLayout->addWidget(new QLabel(tr("Virtual size")), row, 0);
	m_pPPoolVSize = new QLabel();
	m_pPPoolLayout->addWidget(m_pPPoolVSize, row++, 1);

	//m_pPPoolLayout->addWidget(new QLabel(tr("Paged limit")), row, 0);
	//m_pPPoolLimit = new QLabel();
	//m_pPPoolLayout->addWidget(m_pPPoolLimit, row++, 1);

	////////////////////////////////////////

	m_pNPPoolBox = new QGroupBox(tr("Non-paged pool"));
	m_pNPPoolBox->setMinimumWidth(150);
	m_pNPPoolLayout = new QGridLayout();
	m_pNPPoolBox->setLayout(m_pNPPoolLayout);
	m_pPoolLayout->addWidget(m_pNPPoolBox, 1, 0);
	m_pPoolLayout->addWidget(m_pNPPoolBox, 0, 1);
	m_pPoolLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 1, 1);
	//m_pPoolLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 2, 0);

	row = 0;
	m_pNPPoolLayout->addWidget(new QLabel(tr("Usage")), row, 0);
	m_pNPPoolUsage = new QLabel();
	m_pNPPoolLayout->addWidget(m_pNPPoolUsage, row++, 1);

	m_pNPPoolLayout->addWidget(new QLabel(), row++, 0);

	//m_pNPPoolLayout->addWidget(new QLabel(tr("Limit")), row, 0);
	//m_pNPPoolLimit = new QLabel();
	//m_pNPPoolLayout->addWidget(m_pNPPoolLimit, row++, 1);

	m_pPoolLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 3);

	// Note: Winsows 10 starting with 1809 does not longer provide a value for:
	//			PagedPoolAllocs, PagedPoolFrees, NonPagedPoolAllocs, NonPagedPoolFrees

	////////////////////////////////////////

	m_pPageWidget = new QWidget();
	m_pPageLayout = new QGridLayout();
	m_pPageLayout->setMargin(3);
	m_pPageWidget->setLayout(m_pPageLayout);
	m_pInfoTabs->addTab(m_pPageWidget, tr("Paging"));

	m_pPagingBox = new QGroupBox(tr("Paging"));
	m_pPagingBox->setMinimumWidth(150);
	m_pPagingLayout = new QGridLayout();
	m_pPagingBox->setLayout(m_pPagingLayout);
	m_pPageLayout->addWidget(m_pPagingBox, 0, 0);

	row = 0;
	m_pPagingLayout->addWidget(new QLabel(tr("Page faults")), row, 0);
	m_pPagingFault = new QLabel();
	m_pPagingLayout->addWidget(m_pPagingFault, row++, 1);

	m_pPagingLayout->addWidget(new QLabel(tr("Page reads")), row, 0);
	m_pPagingReads = new QLabel();
	m_pPagingLayout->addWidget(m_pPagingReads, row++, 1);

	m_pPagingLayout->addWidget(new QLabel(tr("Pagefile writes")), row, 0);
	m_pPagingFileWrites = new QLabel();
	m_pPagingLayout->addWidget(m_pPagingFileWrites, row++, 1);

	m_pPagingLayout->addWidget(new QLabel(tr("Mapped writes")), row, 0);
	m_pMappedWrites = new QLabel();
	m_pPagingLayout->addWidget(m_pMappedWrites, row++, 1);

	m_pPageLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 1, 0);

	////////////////////////////////////////

	// Page Files
	m_pSwapList = new CPanelWidget<QTreeWidgetEx>();

	m_pSwapList->GetTree()->setItemDelegate(theGUI->GetItemDelegate());
	m_pSwapList->GetTree()->setHeaderLabels(tr("File name|Usag|Peak usage|Total size").split("|"));
	m_pSwapList->GetTree()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pSwapList->GetTree()->setSortingEnabled(true);
	m_pSwapList->GetTree()->setMinimumHeight(60);
	m_pSwapList->GetTree()->setAutoFitMax(200 * theGUI->GetDpiScale());

	m_pSwapBox = new QGroupBox(tr("Swap Files"));
	m_pSwapBox->setMinimumWidth(150);
	m_pSwapLayout = new QGridLayout();
	m_pSwapBox->setLayout(m_pSwapLayout);
	m_pPageLayout->addWidget(m_pSwapBox, 0, 1, 2, 1);

	m_pSwapLayout->addWidget(m_pSwapList, 0, 0);
	//

	////////////////////////////////////////

#ifdef WIN32
	m_pListsWidget = new QWidget();
	m_pListsLayout = new QGridLayout();
	m_pListsLayout->setMargin(3);
	m_pListsWidget->setLayout(m_pListsLayout);
	m_pInfoTabs->addTab(m_pListsWidget, tr("Memory lists"));

	m_pListBox = new QGroupBox(tr("Paging"));
	m_pListBox->setMinimumWidth(150);
	m_pListLayout = new QGridLayout();
	m_pListBox->setLayout(m_pListLayout);
	m_pListsLayout->addWidget(m_pListBox, 0, 0);
	m_pListsLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 1, 0);

	row = 0;
	m_pListLayout->addWidget(new QLabel(tr("Zeroed")), row, 0);
	m_pZeroed = new QLabel();
	m_pListLayout->addWidget(m_pZeroed, row++, 1);

	m_pListLayout->addWidget(new QLabel(tr("Free")), row, 0);
	m_pFree = new QLabel();
	m_pListLayout->addWidget(m_pFree, row++, 1);

	m_pListLayout->addWidget(new QLabel(tr("Modified")), row, 0);
	m_pModified = new QLabel();
	m_pListLayout->addWidget(m_pModified, row++, 1);

	m_pListLayout->addWidget(new QLabel(tr("Modified no write")), row, 0);
	m_pModifiedNoWrite = new QLabel();
	m_pListLayout->addWidget(m_pModifiedNoWrite, row++, 1);

	m_pListLayout->addWidget(new QLabel(tr("Modified paged")), row, 0);
	m_pModifiedPaged = new QLabel();
	m_pListLayout->addWidget(m_pModifiedPaged, row++, 1);


	m_pStanbyBox = new QGroupBox(tr("Standby"));
	m_pStanbyBox->setMinimumWidth(150);
	m_pStanbyLayout = new QGridLayout();
	m_pStanbyBox->setLayout(m_pStanbyLayout);
	m_pListsLayout->addWidget(m_pStanbyBox, 0, 1);
	m_pListsLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 1, 1);

	row = 0;
	m_pStanbyLayout->addWidget(new QLabel(tr("Standby (Repurposed)")), row, 0, 1, 2);
	m_pStandby = new QLabel(tr("0MB"));
	m_pStanbyLayout->addWidget(m_pStandby, row++, 2, 1, 3);

	QFrame* vFrame = new QFrame;
	vFrame->setFrameShape(QFrame::VLine);
	m_pStanbyLayout->addWidget(vFrame, row, 2, 4, 1);

	m_pStanbyLayout->addWidget(new QLabel(tr("Priority 0")), row, 0);
	m_pPriority0 = new QLabel(tr("0MB"));
	m_pStanbyLayout->addWidget(m_pPriority0, row, 1);

	m_pStanbyLayout->addWidget(new QLabel(tr("Priority 4")), row, 3);
	m_pPriority4 = new QLabel(tr("0MB"));
	m_pStanbyLayout->addWidget(m_pPriority4, row++, 4);

	m_pStanbyLayout->addWidget(new QLabel(tr("Priority 1")), row, 0);
	m_pPriority1 = new QLabel(tr("0MB"));
	m_pStanbyLayout->addWidget(m_pPriority1, row, 1);

	m_pStanbyLayout->addWidget(new QLabel(tr("Priority 5")), row, 3);
	m_pPriority5 = new QLabel(tr("0MB"));
	m_pStanbyLayout->addWidget(m_pPriority5, row++, 4);

	m_pStanbyLayout->addWidget(new QLabel(tr("Priority 2")), row, 0);
	m_pPriority2 = new QLabel(tr("0MB"));
	m_pStanbyLayout->addWidget(m_pPriority2, row, 1);

	m_pStanbyLayout->addWidget(new QLabel(tr("Priority 6")), row, 3);
	m_pPriority6 = new QLabel(tr("0MB"));
	m_pStanbyLayout->addWidget(m_pPriority6, row++, 4);

	m_pStanbyLayout->addWidget(new QLabel(tr("Priority 3")), row, 0);
	m_pPriority3 = new QLabel(tr("0MB"));
	m_pStanbyLayout->addWidget(m_pPriority3, row, 1);

	m_pStanbyLayout->addWidget(new QLabel(tr("Priority 7")), row, 3);
	m_pPriority7 = new QLabel(tr("0MB"));
	m_pStanbyLayout->addWidget(m_pPriority7, row++, 4);

	m_pListsLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 3);
#endif

	////////////////////////////////////////

	setObjectName(parent->objectName());
	QByteArray Columns = theConf->GetBlob(objectName() + "/RAMView_Columns");
	if (!Columns.isEmpty())
		m_pSwapList->GetView()->header()->restoreState(Columns);

	m_pInfoTabs->setCurrentIndex(theConf->GetValue(objectName() + "/RAMView_Tab").toInt());
}


CRAMView::~CRAMView()
{
	theConf->SetBlob(objectName() + "/RAMView_Columns", m_pSwapList->GetView()->header()->saveState());
	theConf->SetValue(objectName() + "/RAMView_Tab", m_pInfoTabs->currentIndex());
}


void CRAMView::Refresh()
{
	m_pRAMSize->setText(tr("%1 installed").arg(FormatSize(theAPI->GetInstalledMemory())));

	m_pVMemCurrent->setText(FormatSize(theAPI->GetCommitedMemory()));
	m_pVMemPeak->setText(FormatSize(theAPI->GetCommitedMemoryPeak()));
	m_pVMemLimit->setText(FormatSize(theAPI->GetMemoryLimit()));
	m_pSwapSize->setText(FormatSize(theAPI->GetTotalSwapMemory()));

	m_pRAMUsed->setText(FormatSize(theAPI->GetPhysicalUsed()));
	m_pRAMTotal->setText(FormatSize(theAPI->GetAvailableMemory()));
	m_pRAMReserved->setText(FormatSize(theAPI->GetReservedMemory()));
	m_pRAMCacheWS->setText(FormatSize(theAPI->GetCacheMemory()));
	m_pRAMKernelWS->setText(FormatSize(theAPI->GetKernelMemory()));
	m_pRAMDriverWS->setText(FormatSize(theAPI->GetDriverMemory()));

	m_pPPoolWS->setText(FormatSize(theAPI->GetPersistentPagedPool()));
	m_pPPoolVSize->setText(FormatSize(theAPI->GetPagedPool()));

	m_pNPPoolUsage->setText(FormatSize(theAPI->GetNonPagedPool()));

	// todo: add limti valoues make them work on win10
	//m_pPPoolLimit;
	//m_pNPPoolLimit;

	SCpuStatsEx CpuStats = theAPI->GetCpuStats();

	m_pPagingFault->setText(tr("%2 / %1").arg(FormatUnit(CpuStats.PageFaultsDelta.Value)).arg(CpuStats.PageFaultsDelta.Delta));
	m_pPagingReads->setText(tr("%2 / %1").arg(FormatUnit(CpuStats.PageReadsDelta.Value)).arg(CpuStats.PageReadsDelta.Delta));
	m_pPagingFileWrites->setText(tr("%2 / %1").arg(FormatUnit(CpuStats.PageFileWritesDelta.Value)).arg(CpuStats.PageFileWritesDelta.Delta));
	m_pMappedWrites->setText(tr("%2 / %1").arg(FormatUnit(CpuStats.MappedWritesDelta.Value)).arg(CpuStats.MappedWritesDelta.Delta));


	QMap<QString, QTreeWidgetItem*> OldFiles;
	for (int i = 0; i < m_pSwapList->GetTree()->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem* pItem = m_pSwapList->GetTree()->topLevelItem(i);
		QString Device = pItem->data(0, Qt::UserRole).toString();
		Q_ASSERT(!OldFiles.contains(Device));
		OldFiles.insert(Device, pItem);
	}

	QList<SPageFile> PageFiles = theAPI->GetPageFiles();

	for (int i=0; i < PageFiles.size(); i++)
	{
		SPageFile& PageFile = PageFiles[i];

		QTreeWidgetItem* pItem = OldFiles.take(PageFile.Path);
		if (!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, PageFile.Path);
			pItem->setText(eFileName, PageFile.Path);
			m_pSwapList->GetTree()->addTopLevelItem(pItem);
		}

		pItem->setText(eUsage,FormatSize(PageFile.PeakUsage));
		pItem->setText(ePeakUsage,FormatSize(PageFile.PeakUsage));
		pItem->setText(eTotalSize,FormatSize(PageFile.TotalSize));
	}

	foreach(QTreeWidgetItem* pItem, OldFiles)
		delete pItem;


#ifdef WIN32
    SYSTEM_MEMORY_LIST_INFORMATION memoryListInfo;

	if (NT_SUCCESS(NtQuerySystemInformation(SystemMemoryListInformation, &memoryListInfo, sizeof(SYSTEM_MEMORY_LIST_INFORMATION), NULL)))
	{
		ULONG_PTR standbyPageCount = 0;
		ULONG_PTR repurposedPageCount = 0;
		for (int i = 0; i < 8; i++)
		{
			standbyPageCount += memoryListInfo.PageCountByPriority[i];
			repurposedPageCount += memoryListInfo.RepurposedPagesByPriority[i];
		}

		m_pZeroed->setText(FormatSize((ULONG64)memoryListInfo.ZeroPageCount * PAGE_SIZE));
		m_pFree->setText(FormatSize((ULONG64)memoryListInfo.FreePageCount * PAGE_SIZE));
		m_pModified->setText(FormatSize((ULONG64)memoryListInfo.ModifiedPageCount * PAGE_SIZE));
		m_pModifiedNoWrite->setText(FormatSize((ULONG64)memoryListInfo.ModifiedNoWritePageCount * PAGE_SIZE));
		m_pModifiedPaged->setText(FormatSize((ULONG64)memoryListInfo.BadPageCount * PAGE_SIZE));

		m_pStandby->setText(tr("%1 (%2)").arg(FormatSize((ULONG64)standbyPageCount * PAGE_SIZE))
			.arg(FormatSize((ULONG64)repurposedPageCount * PAGE_SIZE)));
		m_pPriority0->setText(tr("%1 (%2)").arg(FormatSize((ULONG64)memoryListInfo.PageCountByPriority[0] * PAGE_SIZE))
			.arg(FormatSize((ULONG64)memoryListInfo.RepurposedPagesByPriority[0] * PAGE_SIZE)));
		m_pPriority1->setText(tr("%1 (%2)").arg(FormatSize((ULONG64)memoryListInfo.PageCountByPriority[1] * PAGE_SIZE))
			.arg(FormatSize((ULONG64)memoryListInfo.RepurposedPagesByPriority[1] * PAGE_SIZE)));
		m_pPriority2->setText(tr("%1 (%2)").arg(FormatSize((ULONG64)memoryListInfo.PageCountByPriority[2] * PAGE_SIZE))
			.arg(FormatSize((ULONG64)memoryListInfo.RepurposedPagesByPriority[2] * PAGE_SIZE)));
		m_pPriority3->setText(tr("%1 (%2)").arg(FormatSize((ULONG64)memoryListInfo.PageCountByPriority[3] * PAGE_SIZE))
			.arg(FormatSize((ULONG64)memoryListInfo.RepurposedPagesByPriority[3] * PAGE_SIZE)));
		m_pPriority4->setText(tr("%1 (%2)").arg(FormatSize((ULONG64)memoryListInfo.PageCountByPriority[4] * PAGE_SIZE))
			.arg(FormatSize((ULONG64)memoryListInfo.RepurposedPagesByPriority[4] * PAGE_SIZE)));
		m_pPriority5->setText(tr("%1 (%2)").arg(FormatSize((ULONG64)memoryListInfo.PageCountByPriority[5] * PAGE_SIZE))
			.arg(FormatSize((ULONG64)memoryListInfo.RepurposedPagesByPriority[5] * PAGE_SIZE)));
		m_pPriority6->setText(tr("%1 (%2)").arg(FormatSize((ULONG64)memoryListInfo.PageCountByPriority[6] * PAGE_SIZE))
			.arg(FormatSize((ULONG64)memoryListInfo.RepurposedPagesByPriority[6] * PAGE_SIZE)));
		m_pPriority7->setText(tr("%1 (%2)").arg(FormatSize((ULONG64)memoryListInfo.PageCountByPriority[7] * PAGE_SIZE))
			.arg(FormatSize((ULONG64)memoryListInfo.RepurposedPagesByPriority[7] * PAGE_SIZE)));
	}
#endif
}

void CRAMView::UpdateGraphs()
{
	m_pRAMPlot->AddPlotPoint("Commited", theAPI->GetCommitedMemory());
	m_pRAMPlot->AddPlotPoint("Swapped", theAPI->GetSwapedOutMemory());
	m_pRAMPlot->AddPlotPoint("Cache", theAPI->GetCacheMemory());
	m_pRAMPlot->AddPlotPoint("Physical", theAPI->GetPhysicalUsed());
}