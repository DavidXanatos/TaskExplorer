#include "stdafx.h"
#include "SystemView.h"
#include "../TaskExplorer.h"


CSystemView::CSystemView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pScrollArea = new QScrollArea();
	m_pMainLayout->addWidget(m_pScrollArea);

	QWidget* m_pInfoWidget = new QWidget();
	m_pScrollArea->setWidgetResizable(true);
	m_pScrollArea->setWidget(m_pInfoWidget);

	m_pInfoLayout = new QVBoxLayout();
	m_pInfoWidget->setLayout(m_pInfoLayout);

	m_pSystemBox = new QGroupBox(tr("System"));
	m_pInfoLayout->addWidget(m_pSystemBox);

	m_pSystemLayout = new QGridLayout();
	m_pSystemLayout->setSpacing(2);
	m_pSystemBox->setLayout(m_pSystemLayout);
	int row = 0;

	m_pSystemLayout->addWidget(new QLabel(tr("Cpu Model:")), row, 0);
	m_pCpuModel = new QLabel();
	m_pCpuModel->setSizePolicy(QSizePolicy::Expanding, m_pCpuModel->sizePolicy().verticalPolicy());
	m_pSystemLayout->addWidget(m_pCpuModel, row++, 1);

	m_pSystemLayout->addWidget(new QLabel(tr("TotalMemory:")), row, 0);
	m_pSystemMemory = new QLabel();
	m_pSystemLayout->addWidget(m_pSystemMemory, row++, 1);

	// todo
	/*
		system host name
		ram info dim count
		cpu features: virtualization etc
	*/

	/*m_pStatsList = new QTreeWidgetEx();

	m_pStatsList->setItemDelegate(new CStyledGridItemDelegate(m_pStatsList->fontMetrics().height() + 3, this));
	m_pStatsList->setHeaderLabels(tr("Name|Value").split("|"));
	m_pStatsList->header()->hide();

	m_pStatsList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pStatsList->setSortingEnabled(false);

	for (int i = 0; i < 100; i++) {
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		//pItem->setData(0, Qt::UserRole, i);
		pItem->setText(0, QString::number(i));
		m_pStatsList->addTopLevelItem(pItem);
	}

	m_pStatsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pStatsList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	m_pInfoLayout->addWidget(m_pStatsList);*/

	m_pStatsView = new CStatsView(CStatsView::eSystem, this);
	m_pStatsView->setSizePolicy(m_pStatsView->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
	m_pInfoLayout->addWidget(m_pStatsView);

	/*QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pProcessLayout->addWidget(pSpacer, row, 1);*/
	//m_pInfoLayout->addWidget(pSpacer);
}


CSystemView::~CSystemView()
{
}


void CSystemView::Refresh()
{
	m_pCpuModel->setText(theAPI->GetCpuModel());
	m_pSystemMemory->setText(FormatSize(theAPI->GetInstalledMemory()));

	m_pStatsView->ShowSystem();
}
