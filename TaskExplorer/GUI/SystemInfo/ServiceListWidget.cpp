#include "stdafx.h"
#include "ServiceListWidget.h"
#include "../TaskExplorer.h"
#ifdef WIN32
#include "../../API/Windows/WinService.h"	
#endif


CServiceListWidget::CServiceListWidget(QWidget *parent)
	:QWidget(parent)
{
    m_pMainWidget = new QWidget(this);
    m_pMainWidget->setGeometry(QRect(10, 10, 461, 311));
    m_pMainLayout = new QGridLayout(m_pMainWidget);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pInfoLabel = new QLabel(m_pMainWidget);
    m_pMainLayout->addWidget(m_pInfoLabel, 0, 0, 1, 1);

	m_pServiceList = new QTreeWidget(m_pMainWidget);
	m_pServiceList->setHeaderLabels(tr("Name|Display name|File name").split("|"));
    m_pMainLayout->addWidget(m_pServiceList, 1, 0, 1, 1);
	connect(m_pServiceList, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)), this, SLOT(OnItemSellected(QTreeWidgetItem*)));

    m_pDescription = new QLabel(m_pMainWidget);
    QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    sizePolicy.setHorizontalStretch(0);
    sizePolicy.setVerticalStretch(0);
    sizePolicy.setHeightForWidth(m_pDescription->sizePolicy().hasHeightForWidth());
    m_pDescription->setSizePolicy(sizePolicy);
    m_pDescription->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
    m_pDescription->setWordWrap(true);
    m_pMainLayout->addWidget(m_pDescription, 2, 0, 1, 1);

    QHBoxLayout* horizontalLayout = new QHBoxLayout();
    horizontalLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_pStartBtn = new QPushButton(m_pMainWidget);
	m_pStartBtn->setText(tr("Start"));
    horizontalLayout->addWidget(m_pStartBtn);
	connect(m_pStartBtn, SIGNAL(pressed()), this, SLOT(OnStart()));

    m_pPauseBtn = new QPushButton(m_pMainWidget);
	m_pPauseBtn->setText(tr("Pause"));
    horizontalLayout->addWidget(m_pPauseBtn);
	connect(m_pPauseBtn, SIGNAL(pressed()), this, SLOT(OnPause()));

    m_pMainLayout->addLayout(horizontalLayout, 3, 0, 1, 1);
}

void CServiceListWidget::SetLabel(const QString& text)
{
	m_pInfoLabel->setText(text);
}

void CServiceListWidget::SetServices(const QMap<QString, CServicePtr>& Services)
{
	m_Services = Services;

	QMap<QString, QTreeWidgetItem*> OldServices;
	for(int i = 0; i < m_pServiceList->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* pItem = m_pServiceList->topLevelItem(i);
		QString TypeName = pItem->data(eName, Qt::UserRole).toString();
		OldServices.insert(TypeName ,pItem);
	}

	foreach(const CServicePtr& pService, m_Services)
	{
		QTreeWidgetItem* pItem = OldServices.take(pService->GetName());
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setText(eName, pService->GetName());
			pItem->setData(eName, Qt::UserRole, pService->GetName().toLower());
			m_pServiceList->addTopLevelItem(pItem);
		}

		pItem->setText(eDisplayName, pService->GetDisplayName());
		pItem->setText(eFileName, pService->GetBinaryPath());
	}

	foreach(QTreeWidgetItem* pItem, OldServices)
		delete pItem;
}

void CServiceListWidget::SetServices(const QStringList& ServiceNames)
{
	QMap<QString, CServicePtr> AllServices = theAPI->GetServiceList();
	QMap<QString, CServicePtr> Services;
	foreach(const QString& ServiceName, ServiceNames)
	{
		CServicePtr pService = AllServices[ServiceName.toLower()];
#ifdef WIN32
		if (!pService)
			pService = CServicePtr(new CWinService(ServiceName));
#endif
		Services.insert(ServiceName.toLower(), pService);
	}
	SetServices(Services);
}

void CServiceListWidget::OnItemSellected(QTreeWidgetItem* item)
{
	QString Name = item->data(eName, Qt::UserRole).toString();
	CServicePtr pService = m_Services.value(Name);
	if (!pService)
		return;

	if (pService->IsStopped()) {
		m_pStartBtn->setText(tr("Start"));
	}
	else {
		m_pStartBtn->setText(tr("Stop"));
	}

	if (pService->IsPaused()) {
		m_pPauseBtn->setEnabled(true);
		m_pPauseBtn->setText(tr("Continue"));
	}
	else{
		m_pPauseBtn->setEnabled(pService->IsRunning());
		m_pStartBtn->setText(tr("Pause"));
	}

#ifdef WIN32
	m_pDescription->setText(pService.objectCast<CWinService>()->GetDescription());
#endif
}

void CServiceListWidget::OnStart()
{
	QTreeWidgetItem* item = m_pServiceList->currentItem();
	if (!item)
		return;
	QString Name = item->data(eName, Qt::UserRole).toString();
	CServicePtr pService = m_Services.value(Name);
	if (!pService)
		return;

	if (pService->IsStopped())
		pService->Start();
	else
		pService->Stop();
}

void CServiceListWidget::OnPause()
{
	QTreeWidgetItem* item = m_pServiceList->currentItem();
	if (!item)
		return;
	QString Name = item->data(eName, Qt::UserRole).toString();
	CServicePtr pService = m_Services.value(Name);
	if (!pService)
		return;

	if (pService->IsPaused())
		pService->Continue();
	else
		pService->Pause();
}