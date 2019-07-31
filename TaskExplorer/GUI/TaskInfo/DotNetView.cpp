#include "stdafx.h"
#include "DotNetView.h"
#include "../../Common/Finder.h"
#include "../TaskExplorer.h"
#include "../../API/Windows/WindowsAPI.h"
#include "../../API/Windows/ProcessHacker/AssemblyEnum.h"
#include "../../API/Windows/ProcessHacker.h"
#include "../../API/Windows/ProcessHacker/DotNetCounters.h"


CDotNetView::CDotNetView(QWidget *parent)
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


	m_pRefresh = new QPushButton(tr("Refresh"));
	connect(m_pRefresh, SIGNAL(pressed()), this, SLOT(OnRefresh()));
	m_pFilterLayout->addWidget(m_pRefresh);
	m_pFilterLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);
	m_pMainLayout->addWidget(m_pSplitter);

	// Assembly List
	m_pAssemblyModel = new CSimpleTreeModel();
	m_pAssemblyModel->setHeaderLabels(tr("Structure|FileName|Flags|ID|NativePath").split("|"));

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pAssemblyModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pAssemblyList = new QTreeViewEx();
	m_pAssemblyList->setItemDelegate(theGUI->GetItemDelegate());
	
	m_pAssemblyList->setModel(m_pSortProxy);

	m_pAssemblyList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pAssemblyList->setSortingEnabled(false);

	m_pAssemblyList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pAssemblyList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(m_pAssemblyList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked()));

	m_pSplitter->addWidget(CFinder::AddFinder(m_pAssemblyList, m_pSortProxy));
	m_pSplitter->setCollapsible(0, false);
	// 

	// Performance Details
	m_pPerfStats = new CPanelWidget<QTreeWidgetEx>();

	m_pPerfStats->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pPerfStats->GetView())->setHeaderLabels(tr("Counter|Value").split("|"));

	m_pPerfStats->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pPerfStats->GetView()->setSortingEnabled(false);

	m_pSplitter->addWidget(m_pPerfStats);
	//

	InitDotNetStatTree(((QTreeWidgetEx*)m_pPerfStats->GetView()), m_PerfCounters);


	//m_pMenu = new QMenu();
	
	m_pOpen = m_pMenu->addAction(tr("Open"), this, SLOT(OnDoubleClicked()));

	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	QByteArray Columns = theConf->GetBlob(objectName() + "/DotNetView_Columns");
	if (Columns.isEmpty())
	{
		m_pAssemblyList->setColumnHidden(eID, true);
		m_pAssemblyList->setColumnHidden(eNativePath, true);
	}
	else
		m_pAssemblyList->header()->restoreState(Columns);
	m_pPerfStats->GetView()->header()->restoreState(theConf->GetBlob(objectName() + "/PerfStats_Columns"));
	m_pSplitter->restoreState(theConf->GetBlob(objectName() + "/DotNetView_Splitter"));
}

CDotNetView::~CDotNetView()
{
	theConf->SetBlob(objectName() + "/DotNetView_Columns", m_pAssemblyList->header()->saveState());
	theConf->SetBlob(objectName() + "/DotNetView_Splitter",m_pSplitter->saveState());
	if(m_pPerfStats)
		theConf->SetBlob(objectName() + "/PerfStats_Columns", m_pPerfStats->GetView()->header()->saveState());
}

void CDotNetView::ShowProcess(const CProcessPtr& pProcess)
{
	m_pCurProcess = pProcess;

	OnRefresh();
	Refresh();
}

void CDotNetView::OnRefresh()
{
	QObject::disconnect(this, SLOT(OnAssemblies(const CAssemblyListPtr&)));

	if (!m_pCurProcess)
		return;

	CAssemblyEnum* pEnum = new CAssemblyEnum(m_pCurProcess->GetProcessId(), this);
	
	QObject::connect(pEnum, SIGNAL(Assemblies(const CAssemblyListPtr&)), this, SLOT(OnAssemblies(const CAssemblyListPtr&)));
	QObject::connect(pEnum, SIGNAL(Finished()), pEnum, SLOT(deleteLater()));

	pEnum->start();
}

void CDotNetView::OnAssemblies(const CAssemblyListPtr& Assemblies)
{
	QMap<QVariant, QVariantMap> List;

	for (int i = 0; i < Assemblies->GetCount(); i++)
	{
		auto& Assembly = Assemblies->GetAssembly(i);

		QVariantMap Item;
		Item["ID"] = Assembly.ID;
		Item["ParentID"] = Assembly.ParrentID;

		QVariantMap Values;
		Values.insert(QString::number(eStructure), Assembly.Structure);
		Values.insert(QString::number(eFileName), Assembly.FileName);
		Values.insert(QString::number(eFlags), Assembly.Flags);
		Values.insert(QString::number(eID), Assembly.ID);
		Values.insert(QString::number(eNativePath), Assembly.NativePath);
		Item["Values"] = Values;

		ASSERT(!List.contains(Assembly.ID));
		List.insert(Assembly.ID, Item);
	}

	m_pAssemblyModel->Sync(List);

	m_pAssemblyList->expandAll();
}

void CDotNetView::Refresh()
{
	if (!m_pCurProcess)
		return;

	if (m_pSplitter->sizes()[1] > 0)
	{
		UpdateDotNetStatTree(m_pCurProcess.objectCast<CWinProcess>().data(), m_PerfCounters);
	}
}

void CDotNetView::OnMenu(const QPoint &point)
{
	QModelIndex Index = m_pAssemblyList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	QString FileName = ModelIndex.isValid() ? m_pAssemblyModel->Data(ModelIndex, Qt::EditRole, eFileName).toString() : "";

	m_pOpen->setEnabled(!FileName.isEmpty());

	CPanelView::OnMenu(point);
}

void CDotNetView::OnDoubleClicked()
{
	QModelIndex Index = m_pAssemblyList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	QString FileName = ModelIndex.isValid() ? m_pAssemblyModel->Data(ModelIndex, Qt::EditRole, eFileName).toString() : "";

	if (!FileName.isEmpty())
	{
#ifdef WIN32
		PPH_STRING phFileName = CastQString(FileName);
		PhShellExecuteUserString(NULL, L"FileBrowseExecutable", phFileName->Buffer, FALSE, L"Make sure the Explorer executable file is present.");
		PhDereferenceObject(phFileName);
#endif
	}
}