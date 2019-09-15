#include "stdafx.h"
#include "../../TaskExplorer.h"
#include "AtomView.h"
#include "../../../Common/KeyValueInputDialog.h"
#include "../../../Common/Finder.h"
#include "../../../API/Windows/ProcessHacker.h"


CAtomView::CAtomView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	// Atom List
	m_pAtomModel = new CSimpleListModel();
	m_pAtomModel->setHeaderLabels(tr("Atom name|Ref. count").split("|"));

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pAtomModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pAtomList = new QTreeViewEx();
	m_pAtomList->setItemDelegate(theGUI->GetItemDelegate());
	
	m_pAtomList->setModel(m_pSortProxy);

	m_pAtomList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pAtomList->setSortingEnabled(true);

	m_pAtomList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pAtomList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//m_pAtomList->setColumnReset(2);
	//connect(m_pAtomList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pAtomList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	//connect(m_pAtomList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnItemDoubleClicked(const QModelIndex&)));
	m_pMainLayout->addWidget(m_pAtomList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	//m_pMenu = new QMenu();
	m_pDelete = m_pMenu->addAction(tr("Delete"), this, SLOT(OnDelete()));
	
	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	QByteArray Columns = theConf->GetBlob(objectName() + "/AtomView_Columns");
	if (Columns.isEmpty())
		m_pAtomList->OnResetColumns();
	else
		m_pAtomList->restoreState(Columns);
}

CAtomView::~CAtomView()
{
	theConf->SetBlob(objectName() + "/AtomView_Columns", m_pAtomList->saveState());
}

void CAtomView::Refresh()
{
    PATOM_TABLE_INFORMATION atomTable = NULL;

    if (!NT_SUCCESS(EnumAtomTable(&atomTable)))
        return;

	m_Atoms.clear();

    for (ULONG i = 0; i < atomTable->NumberOfAtoms; i++)
    {
		QString Name;
		USHORT RefCount = 0;

        PATOM_BASIC_INFORMATION atomInfo = NULL;

        if (!NT_SUCCESS(QueryAtomTableEntry(atomTable->Atoms[i], &atomInfo)))
        {
			Name = tr("(Error) #%1").arg(i);
        }
		else
		{
			if ((atomInfo->Flags & RTL_ATOM_PINNED) == RTL_ATOM_PINNED)
				Name = tr("%1 (Pinned)").arg(QString::fromWCharArray(atomInfo->Name));
			else
				Name = QString::fromWCharArray(atomInfo->Name);

			RefCount = atomInfo->UsageCount;

			PhFree(atomInfo);
		}


		QVariantMap Item;
		Item["ID"] = Name;
		
		QVariantMap Values;
		Values.insert(QString::number(eName), Name);
		Values.insert(QString::number(eRefCount), FormatNumber(RefCount));

		Item["Values"] = Values;
		m_Atoms.append(Item);
    }

    PhFree(atomTable);

	m_pAtomModel->Sync(m_Atoms);
}

void CAtomView::OnMenu(const QPoint &point)
{
	QModelIndex Index = m_pAtomList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	QString Name = ModelIndex.isValid() ? m_pAtomModel->Data(ModelIndex, Qt::EditRole, eName).toString() : "";

	m_pDelete->setEnabled(!Name.isEmpty());

	CPanelView::OnMenu(point);
}

void CAtomView::OnDelete()
{
	QModelIndex Index = m_pAtomList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	if (!ModelIndex.isValid())
		return;
	QString Name = m_pAtomModel->Data(ModelIndex, Qt::EditRole, eName).toString();

	if(QMessageBox("TaskExplorer", tr("Do you want to delete the atom: %1").arg(Name), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

    PATOM_TABLE_INFORMATION atomTable = NULL;

    if (!NT_SUCCESS(EnumAtomTable(&atomTable)))
        return;

    for (ULONG i = 0; i < atomTable->NumberOfAtoms; i++)
    {
        PATOM_BASIC_INFORMATION atomInfo = NULL;

        if (!NT_SUCCESS(QueryAtomTableEntry(atomTable->Atoms[i], &atomInfo)))
            continue;

        if (QString::fromWCharArray(atomInfo->Name) != Name)
            continue;

        do
        {
            if (!NT_SUCCESS(NtDeleteAtom(atomTable->Atoms[i])))
            {
                break;
            }

            PhFree(atomInfo);
            atomInfo = NULL;

            if (!NT_SUCCESS(QueryAtomTableEntry(atomTable->Atoms[i], &atomInfo)))
                break;

        } while (atomInfo->UsageCount >= 1);

        if (atomInfo)
        {
            PhFree(atomInfo);
        }
    }

    PhFree(atomTable);
}
