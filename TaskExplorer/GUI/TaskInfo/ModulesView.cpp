#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ModulesView.h"
#include "../../Common/Common.h"

CModulesView::CModulesView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);


	// Module List
	m_pModuleModel = new CModuleModel();

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pModuleModel);
	m_pSortProxy->setDynamicSortFilter(true);


	m_pModuleList = new QTreeViewEx();
	m_pModuleList->setItemDelegate(new CStyledGridItemDelegate(m_pModuleList->fontMetrics().height() + 3, this));
	m_pModuleList->setModel(m_pSortProxy);

	m_pModuleList->setSelectionMode(QAbstractItemView::ExtendedSelection);
#ifdef WIN32
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pModuleList->setStyle(pStyle);
#endif
	m_pModuleList->setSortingEnabled(true);

	m_pModuleList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pModuleList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	m_pMainLayout->addWidget(m_pModuleList);
	// 

	//m_pMenu = new QMenu();
	m_pUnload = m_pMenu->addAction(tr("Unload"), this, SLOT(OnUnload()));
	m_pMenu->addSeparator();
	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	m_pModuleList->header()->restoreState(theConf->GetBlob(objectName() + "/ModulesView_Columns"));
}

CModulesView::~CModulesView()
{
	theConf->SetBlob(objectName() + "/ModulesView_Columns", m_pModuleList->header()->saveState());
}

void CModulesView::ShowModules(const CProcessPtr& pProcess)
{
	if(!m_pCurProcess.isNull())
		disconnect(m_pCurProcess.data(), SIGNAL(ModulesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(OnModulesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
	
	m_pCurProcess = pProcess;

	connect(m_pCurProcess.data(), SIGNAL(ModulesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(OnModulesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

	m_pCurProcess->UpdateModules();
}


void CModulesView::OnModulesUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	QMap<quint64, CModulePtr> Modules = m_pCurProcess->GetModleList();

	m_pModuleModel->Sync(Modules);

	QTimer::singleShot(100, this, [this, Added]() {
		foreach(quint64 ID, Added)
			m_pModuleList->expand(m_pSortProxy->mapFromSource(m_pModuleModel->FindIndex(ID)));
	});
}

void CModulesView::OnMenu(const QPoint &point)
{
	QModelIndex Index = m_pModuleList->currentIndex();
	
	m_pUnload->setEnabled(Index.isValid());
	
	CPanelView::OnMenu(point);
}

void CModulesView::OnUnload()
{
	if(QMessageBox("TaskExplorer", tr("Do you want to unload the selected Module(s)"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	QList<STATUS> Errors;
	int Force = -1;
	foreach(const QModelIndex& Index, m_pModuleList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CModulePtr pModule = m_pModuleModel->GetModule(ModelIndex);
		if (!pModule.isNull())
		{
		retry:
			STATUS Status = pModule->Unload(Force == 1);
			if (Status.IsError())
			{
				if (Status.GetStatus() == ERROR_CONFIRM)
				{
					if (Force == -1)
					{
						switch (QMessageBox("TaskExplorer", Status.GetText(), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel | QMessageBox::Default | QMessageBox::Escape).exec())
						{
						case QMessageBox::Yes:
							Force = 1;
							goto retry;
							break;
						case QMessageBox::No:
							Force = 0;
							break;
						case QMessageBox::Cancel:
							return;
						}
					}
				}
				else 
					Errors.append(Status);
			}
		}
	}

	CTaskExplorer::CheckErrors(Errors);
}