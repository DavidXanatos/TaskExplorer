#include "stdafx.h"
#include "../TaskExplorer.h"
#include "StringView.h"
#include "../../../MiscHelpers/Common/Common.h"
#include "../../API/MemoryInfo.h"
#include "../MemoryEditor.h"
#include "../../../MiscHelpers/Common/Finder.h"
#ifdef WIN32
#include "../../API/Windows/WinMemIO.h"
#endif

CStringView::CStringView(bool bGlobal, QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);


	m_pStringModel = new CStringModel();
	
	m_pSortProxy = new CSortFilterProxyModel(this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pStringModel);
	m_pSortProxy->setDynamicSortFilter(true);


	// String List
	m_pStringList = new QTreeViewEx();
	m_pStringList->setItemDelegate(theGUI->GetItemDelegate());

	m_pStringList->setModel(m_pSortProxy);

	m_pStringList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pStringList->setSortingEnabled(true);

	m_pStringList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pStringList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(m_pStringList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked()));

	connect(m_pStringList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	//connect(theGUI, SIGNAL(ReloadPanels()), m_pStringModel, SLOT(Clear()));

	m_pMainLayout->addWidget(m_pStringList);
	// 

	if (!bGlobal)
		m_pStringList->SetColumnHidden(CStringModel::eProcess, true, true);
	else
		m_pStringModel->SetUseIcons(true);

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));


	//m_pMenu = new QMenu();

	m_pMenuEdit = m_pMenu->addAction(tr("Edit memory"), this, SLOT(OnDoubleClicked()));
	m_pMenu->addSeparator();
	m_pMenuSave = m_pMenu->addAction(tr("Save string(s)"), this, SLOT(OnSaveString()));

	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	m_pStringList->restoreState(theConf->GetBlob(objectName() + "/StringsView_Columns"));
}


CStringView::~CStringView()
{
	theConf->SetBlob(objectName() + "/StringsView_Columns", m_pStringList->saveState());
}

void CStringView::OnColumnsChanged()
{
	m_pStringModel->Sync(m_String);
}

void CStringView::OnMenu(const QPoint &point)
{
	QModelIndex Index = m_pStringList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CStringInfoPtr pString = m_pStringModel->GetString(ModelIndex);

	QModelIndexList selectedRows = m_pStringList->selectedRows();

	m_pMenuEdit->setEnabled(selectedRows.count() == 1);
	m_pMenuSave->setEnabled(selectedRows.count() > 0);

	CPanelView::OnMenu(point);
}

void CStringView::ShowStrings(const QMap<quint64, CStringInfoPtr>& String)
{
	m_String = String;

	m_pStringModel->Sync(m_String);
}

void CStringView::OnDoubleClicked()
{
	QModelIndex Index = m_pStringList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CStringInfoPtr pString = m_pStringModel->GetString(ModelIndex);
	if (!pString)
		return;

#ifdef WIN32
	QIODevice* pDevice = new CWinMemIO(pString->GetBaseAddress(), pString->GetRegionSize(), pString->GetProcess()->GetProcessId());
#else
	// linux-todo:
    QIODevice* pDevice = NULL;
#endif

	CMemoryEditor* pEditor = new CMemoryEditor();
	pEditor->setWindowTitle(tr("Memory Editor: %1 (%2) 0x%3").arg(pString->GetProcess()->GetName()).arg(pString->GetProcess()->GetProcessId()).arg(pString->GetBaseAddress(),0,16));
	pEditor->setDevice(pDevice, pString->GetBaseAddress());
	pEditor->show();
	pEditor->sellect(pString->GetAddress(), pString->GetSize());
}

void CStringView::OnSaveString()
{
	QFile DumpFile;
	QString DumpPath = QFileDialog::getSaveFileName(this, tr("Dump String"), "", tr("Dump files (*.dmp);;All files (*.*)"));
	if (DumpPath.isEmpty())
		return;
	DumpFile.setFileName(DumpPath);
	DumpFile.open(QFile::WriteOnly);

	QList<STATUS> Errors;
	int Force = -1;
	foreach(const QModelIndex& Index, m_pStringList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CStringInfoPtr pString = m_pStringModel->GetString(ModelIndex);

		DumpFile.write((pString->GetString() + "\r\n").toUtf8());
	}
}
