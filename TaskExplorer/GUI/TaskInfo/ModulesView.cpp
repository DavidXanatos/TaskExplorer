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

	m_pFilterWidget = new QWidget();
	m_pMainLayout->addWidget(m_pFilterWidget);

	m_pFilterLayout = new QHBoxLayout();
	m_pFilterLayout->setMargin(3);
	m_pFilterWidget->setLayout(m_pFilterLayout);

	m_pLoadModule = new QPushButton(tr("Inject DLL"));
	m_pFilterLayout->addWidget(m_pLoadModule);

	m_pFilterLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

	connect(m_pLoadModule, SIGNAL(pressed(bool)), this, SLOT(OnLoad()));

	// Module List
	m_pModuleModel = new CModuleModel();

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pModuleModel);
	m_pSortProxy->setDynamicSortFilter(true);


	m_pModuleList = new QTreeViewEx();
	m_pModuleList->setItemDelegate(theGUI->GetItemDelegate());
	m_pModuleList->setModel(m_pSortProxy);

	m_pModuleList->setSelectionMode(QAbstractItemView::ExtendedSelection);
#ifdef WIN32
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pModuleList->setStyle(pStyle);
#endif
	m_pModuleList->setSortingEnabled(true);

	m_pModuleList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pModuleList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadAll()), m_pModuleModel, SLOT(Clear()));

	m_pMainLayout->addWidget(m_pModuleList);
	// 

	//m_pMenu = new QMenu();
	m_pUnload = m_pMenu->addAction(tr("Unload"), this, SLOT(OnUnload()));
	m_pMenu->addSeparator();
	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	QByteArray Columns = theConf->GetBlob(objectName() + "/ModulesView_Columns");
	if (Columns.isEmpty())
	{
		for (int i = 0; i < m_pModuleModel->columnCount(); i++)
			m_pModuleList->setColumnHidden(i, true);

		m_pModuleList->setColumnHidden(CModuleModel::eModule, false);
		m_pModuleList->setColumnHidden(CModuleModel::eBaseAddress, false);
		m_pModuleList->setColumnHidden(CModuleModel::eSize, false);
		m_pModuleList->setColumnHidden(CModuleModel::eDescription, false);
		m_pModuleList->setColumnHidden(CModuleModel::eFileName, false);
	}
	else
		m_pModuleList->header()->restoreState(Columns);
}

CModulesView::~CModulesView()
{
	theConf->SetBlob(objectName() + "/ModulesView_Columns", m_pModuleList->header()->saveState());
}

void CModulesView::ShowProcess(const CProcessPtr& pProcess)
{
	if (m_pCurProcess != pProcess)
	{
		disconnect(this, SLOT(OnModulesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

		m_pCurProcess = pProcess;

		connect(m_pCurProcess.data(), SIGNAL(ModulesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(OnModulesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
	}

	Refresh();
}

void CModulesView::Refresh()
{
	if (!m_pCurProcess)
		return;

	QTimer::singleShot(0, m_pCurProcess.data(), SLOT(UpdateModules()));
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

void CModulesView::OnLoad()
{
	if (!m_pCurProcess)
		return;

	QStringList FilePaths = QFileDialog::getOpenFileNames(0, tr("Select DLL's"), "", tr("DLL files (*.dll)"));
	if (FilePaths.isEmpty())
		return;

	QList<STATUS> Errors;
	foreach(const QString& FileName, FilePaths)
	{
		STATUS Status = m_pCurProcess->LoadModule(FileName);
		if (Status.IsError())
			Errors.append(Status);
	}
	CTaskExplorer::CheckErrors(Errors);
}