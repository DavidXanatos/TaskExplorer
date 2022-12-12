#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ModulesView.h"
#include "../../../MiscHelpers/Common/Common.h"
#include "../../../MiscHelpers/Common/Finder.h"
#ifdef WIN32
#include "../../API/Windows/ProcessHacker.h"
#include "../../API/Windows/ProcessHacker/appsup.h"	
#endif

CModulesView::CModulesView(bool bGlobal, QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);


	m_bGlobal = bGlobal;
	if (!bGlobal)
	{
		m_pFilterWidget = new QWidget();
		m_pMainLayout->addWidget(m_pFilterWidget);

		m_pFilterLayout = new QHBoxLayout();
		m_pFilterLayout->setContentsMargins(3, 3, 3, 3);
		m_pFilterWidget->setLayout(m_pFilterLayout);

		m_pLoadModule = new QPushButton(tr("Inject DLL"));
		connect(m_pLoadModule, SIGNAL(pressed()), this, SLOT(OnLoad()));
		m_pFilterLayout->addWidget(m_pLoadModule);

		m_pFilterLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
	}

	// Module List
	m_pModuleModel = new CModuleModel();

	m_pSortProxy = new CSortFilterProxyModel(this);
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
	connect(m_pModuleList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked()));

	connect(theGUI, SIGNAL(ReloadPanels()), m_pModuleModel, SLOT(Clear()));

	m_pModuleList->setColumnReset(2);
	connect(m_pModuleList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	connect(m_pModuleList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	m_pMainLayout->addWidget(m_pModuleList);
	// 

	if (bGlobal)
	{
		m_pModuleModel->SetUseIcons(true);
		m_pModuleModel->SetTree(false);
	}
	else
	{
		m_pModuleList->SetColumnHidden(CModuleModel::eModuleFile, true, true);
	}

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));


	//m_pMenu = new QMenu();
	m_pOpen = m_pMenu->addAction(tr("Open"), this, SLOT(OnDoubleClicked()));
	
	m_pMenu->addSeparator();
	
	m_pUnload = m_pMenu->addAction(tr("Unload"), this, SLOT(OnUnload()));
	m_pMenu->addSeparator();
	AddPanelItemsToMenu();

	m_ViewMode = eNone;
	setObjectName(parent->objectName());
	SwitchView(bGlobal ? eMulti : eSingle);
}

CModulesView::~CModulesView()
{
	SwitchView(eNone);
}

void CModulesView::SwitchView(EView ViewMode)
{
	switch (m_ViewMode)
	{
		case eSingle:	theConf->SetBlob(objectName() + "/ModulesView_Columns", m_pModuleList->saveState()); break;
		case eMulti:	theConf->SetBlob(objectName() + "/ModulesMultiView_Columns", m_pModuleList->saveState()); break;
	}

	m_ViewMode = ViewMode;

	QByteArray Columns;
	switch (m_ViewMode)
	{
		case eSingle:	Columns = theConf->GetBlob(objectName() + "/ModulesView_Columns"); break;
		case eMulti:	Columns = theConf->GetBlob(objectName() + "/ModulesMultiView_Columns"); break;
		default:
			return;
	}
	
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pModuleList->restoreState(Columns);
}

void CModulesView::OnResetColumns()
{
	for (int i = 0; i < m_pModuleModel->columnCount(); i++)
		m_pModuleList->SetColumnHidden(i, true);

	m_pModuleList->SetColumnHidden(CModuleModel::eModule, false);
	m_pModuleList->SetColumnHidden(CModuleModel::eBaseAddress, false);
	m_pModuleList->SetColumnHidden(CModuleModel::eSize, false);
#ifdef WIN32
	if(!m_bGlobal)
		m_pModuleList->SetColumnHidden(CModuleModel::eDescription, false);
#endif
	m_pModuleList->SetColumnHidden(CModuleModel::eFileName, false);
}

void CModulesView::OnColumnsChanged()
{
	m_pModuleModel->Sync(m_Modules);
}

void CModulesView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	CProcessPtr pProcess;
	if (Processes.count() > 1)
	{
		setEnabled(false);
	}
	else if(!Processes.isEmpty())
	{
		setEnabled(true);
		pProcess = Processes.first();
	}

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
	m_Modules = m_pCurProcess->GetModuleList();

	Added = m_pModuleModel->Sync(m_Modules);

	QTimer::singleShot(100, this, [this, Added]() {
		foreach(quint64 ID, Added)
			m_pModuleList->expand(m_pSortProxy->mapFromSource(m_pModuleModel->FindIndex(ID)));
	});
}

void CModulesView::ShowModules(const QMap<quint64, CModulePtr>& Modules)
{
	m_Modules = Modules;

	m_pModuleModel->Sync(m_Modules);

	QTimer::singleShot(100, this, [this, Modules]() {
		foreach(quint64 ID, Modules.keys())
			m_pModuleList->expand(m_pSortProxy->mapFromSource(m_pModuleModel->FindIndex(ID)));
	});
}

void CModulesView::OnMenu(const QPoint &point)
{
	QModelIndexList selectedRows = m_pModuleList->selectedRows();

	int Loaded = 0;
	foreach(const QModelIndex& Index, selectedRows)
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CModulePtr pModule = m_pModuleModel->GetModule(ModelIndex);
		if (pModule->IsLoaded())
			Loaded++;
	}

	m_pOpen->setEnabled(selectedRows.count() == 1);
	m_pUnload->setEnabled(Loaded > 0);
	
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

void CModulesView::OnDoubleClicked()
{
	QModelIndex Index = m_pModuleList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CModulePtr pModule = m_pModuleModel->GetModule(ModelIndex);
	if (!pModule)
		return;

#ifdef WIN32
	PPH_STRING phFileName = CastQString(pModule->GetFileName());
	PhShellExecuteUserString(NULL, L"FileBrowseExecutable", phFileName->Buffer, FALSE, L"Make sure the Explorer executable file is present." );
	PhDereferenceObject(phFileName);
#endif
}
