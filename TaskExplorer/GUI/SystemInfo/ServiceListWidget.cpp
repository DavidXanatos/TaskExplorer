#include "stdafx.h"
#include "ServiceListWidget.h"
#include "../TaskExplorer.h"
#ifdef WIN32
#include "../../API/Windows/WinService.h"	
#endif


CServiceListWidget::CServiceListWidget(bool bEditable, QWidget *parent)
	:QWidget(parent)
{
    m_pMainLayout = new QGridLayout();
	this->setLayout(m_pMainLayout);
    m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pInfoLabel = new QLabel();
    m_pMainLayout->addWidget(m_pInfoLabel, 0, 0, 1, 1);

	m_pServiceList = new QTreeWidget();
	m_pServiceList->setHeaderLabels(tr("Name|Display name|File name").split("|"));
    m_pMainLayout->addWidget(m_pServiceList, 1, 0, 1, 1);
	connect(m_pServiceList, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)), this, SLOT(OnItemSellected(QTreeWidgetItem*)));

    m_pDescription = new QLabel();
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

	if (bEditable)
	{
		m_pAddBtn = new QPushButton();
		m_pAddBtn->setText(tr("Add ..."));
		horizontalLayout->addWidget(m_pAddBtn);
		connect(m_pAddBtn, SIGNAL(pressed()), this, SLOT(OnAdd()));

		m_pRemoveBtn = new QPushButton();
		m_pRemoveBtn->setText(tr("Remove"));
		horizontalLayout->addWidget(m_pRemoveBtn);
		connect(m_pRemoveBtn, SIGNAL(pressed()), this, SLOT(OnRemove()));
	}

    m_pStartBtn = new QPushButton();
	m_pStartBtn->setText(tr("Start"));
    horizontalLayout->addWidget(m_pStartBtn);
	connect(m_pStartBtn, SIGNAL(pressed()), this, SLOT(OnStart()));

    m_pPauseBtn = new QPushButton();
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

	UpdateServices();
}

void CServiceListWidget::UpdateServices()
{
	QMap<QString, QTreeWidgetItem*> OldServices;
	for(int i = 0; i < m_pServiceList->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* pItem = m_pServiceList->topLevelItem(i);
		QString Name = pItem->data(eName, Qt::UserRole).toString();
		OldServices.insert(Name ,pItem);
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

void CServiceListWidget::SetServicesList(const QStringList& ServiceNames)
{
	QMap<QString, CServicePtr> AllServices = theAPI->GetServiceList();
	QMap<QString, CServicePtr> Services;
	foreach(const QString& ServiceName, ServiceNames)
	{
		CServicePtr pService = AllServices.value(ServiceName.toLower());
#ifdef WIN32
		if (!pService)
			pService = CServicePtr(new CWinService(ServiceName));
#endif
		Services.insert(ServiceName.toLower(), pService);
	}
	SetServices(Services);
}

QStringList CServiceListWidget::GetServicesList() const
{
	QStringList ServiceNames;
	/*for(int i = 0; i < m_pServiceList->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* pItem = m_pServiceList->topLevelItem(i);
		QString Name = pItem->data(eName, Qt::UserRole).toString();
		CServicePtr pService = m_Services.value(Name);
		if(pService)
			ServiceNames.append(pService->GetName());
	}*/
	foreach(const CServicePtr& pService, m_Services)
		ServiceNames.append(pService->GetName());
	return ServiceNames;
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

void CServiceListWidget::OnAdd()
{
	QString Value = QInputDialog::getText(this, "TaskExplorer", tr("Enter Service name"), QLineEdit::Normal);
	if (Value.isEmpty())
		return;

	if (m_Services.contains(Value.toLower()))
	{
		QMessageBox::warning(this, "TaskExplorer", tr("This service is already added."));
		return;
	}

	QMap<QString, CServicePtr> AllServices = theAPI->GetServiceList();

	CServicePtr pService = AllServices.value(Value.toLower());
	if (!pService)
	{
		QMessageBox::about(this, "TaskExplorer", tr("This service does not exist."));
		return;
	}

	m_Services.insert(Value.toLower(), pService);
	UpdateServices();
}

void CServiceListWidget::OnRemove()
{
	QTreeWidgetItem* item = m_pServiceList->currentItem();
	if (!item)
		return;
	QString Name = item->data(eName, Qt::UserRole).toString();

	if(QMessageBox("TaskExplorer", tr("Do you want to delete the sellected service"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	m_Services.remove(Name);
	UpdateServices();
}