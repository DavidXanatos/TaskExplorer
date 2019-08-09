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

	m_pInfoWidget = new QWidget();
	m_pScrollArea->setFrameShape(QFrame::NoFrame);
	m_pScrollArea->setWidgetResizable(true);
	m_pScrollArea->setWidget(m_pInfoWidget);
	QPalette pal = m_pScrollArea->palette();
	pal.setColor(QPalette::Background, Qt::transparent);
	m_pScrollArea->setPalette(pal);

	m_pInfoLayout = new QVBoxLayout();
	m_pInfoWidget->setLayout(m_pInfoLayout);

	m_pSystemBox = new QGroupBox(tr("System"));
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
	
	m_pSystemLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), row++, 1);

	m_pSystemLayout->addWidget(new QLabel(tr("Version:")), row, 1);
	m_pSystemVersion = new QLabel();
	m_pSystemVersion->setSizePolicy(QSizePolicy::Expanding, m_pSystemVersion->sizePolicy().verticalPolicy());
	m_pSystemLayout->addWidget(m_pSystemVersion, row++, 2);

	m_pSystemLayout->addWidget(new QLabel(tr("Build:")), row, 1);
	m_pSystemBuild = new QLabel();
	m_pSystemLayout->addWidget(m_pSystemBuild, row++, 2);


	m_pSystemLayout->addWidget(new QLabel(tr("Up time:")), row, 0);
	m_pUpTime = new QLabel();
	m_pSystemLayout->addWidget(m_pUpTime, row++, 1, 1, 2);

	m_pSystemLayout->addWidget(new QLabel(tr("Host name:")), row, 0);
	m_pHostName = new QLabel();
	m_pSystemLayout->addWidget(m_pHostName, row++, 1, 1, 2);

	m_pSystemLayout->addWidget(new QLabel(tr("User name:")), row, 0);
	m_pUserName = new QLabel();
	m_pSystemLayout->addWidget(m_pUserName, row++, 1, 1, 2);

	m_pSystemLayout->addWidget(new QLabel(tr("System directory:")), row++, 0, 1, 3);
	m_pSystemDir = new QLineEdit();
	m_pSystemDir->setReadOnly(true);
	m_pSystemLayout->addWidget(m_pSystemDir, row++, 0, 1, 3);


	/*m_pStatsList = new QTreeWidgetEx();

	m_pStatsList->setItemDelegate(theGUI->GetItemDelegate());
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
	if(m_pIcon->pixmap() == NULL)
		m_pIcon->setPixmap(theAPI->GetSystemIcon().scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

	m_pSystemName->setText(theAPI->GetSystemName());
	m_pSystemVersion->setText(theAPI->GetSystemVersion());
	m_pSystemBuild->setText(theAPI->GetSystemBuild());

	m_pUpTime->setText(FormatTime(theAPI->GetUpTime()));
	m_pHostName->setText(theAPI->GetHostName());
	m_pUserName->setText(theAPI->GetUserName());
	m_pSystemDir->setText(theAPI->GetSystemDir());

	m_pStatsView->ShowSystem();
}
