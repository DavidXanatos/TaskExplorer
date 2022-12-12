/*
 * Task Explorer -
 *   qt port of the Running Object Table Plugin
 *
 * Copyright (C) 2015 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 * 
 */

#include "stdafx.h"
#include "../../TaskExplorer.h"
#include "RunObjView.h"
#include "../../../../MiscHelpers/Common/KeyValueInputDialog.h"
#include "../../../../MiscHelpers/Common/Finder.h"
#include "../../../API/Windows/ProcessHacker.h"


CRunObjView::CRunObjView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	// RunObj List
	m_pRunObjModel = new CSimpleListModel();
	m_pRunObjModel->setHeaderLabels(tr("Display name").split("|"));

	m_pSortProxy = new CSortFilterProxyModel(this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pRunObjModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pRunObjList = new QTreeViewEx();
	m_pRunObjList->setItemDelegate(theGUI->GetItemDelegate());
	
	m_pRunObjList->setModel(m_pSortProxy);

	m_pRunObjList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pRunObjList->setSortingEnabled(true);

	m_pRunObjList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pRunObjList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//m_pRunObjList->setColumnReset(2);
	//connect(m_pRunObjList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pRunObjList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	//connect(m_pRunObjList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnItemDoubleClicked(const QModelIndex&)));
	m_pMainLayout->addWidget(m_pRunObjList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	//m_pMenu = new QMenu();
	
	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	QByteArray Columns = theConf->GetBlob(objectName() + "/RunObjView_Columns");
	if (Columns.isEmpty())
		m_pRunObjList->OnResetColumns();
	else
		m_pRunObjList->restoreState(Columns);
}

CRunObjView::~CRunObjView()
{
	theConf->SetBlob(objectName() + "/RunObjView_Columns", m_pRunObjList->saveState());
}

void CRunObjView::Refresh()
{
	IRunningObjectTable* iRunningObjectTable = NULL;
    IEnumMoniker* iEnumMoniker = NULL;
    IMoniker* iMoniker = NULL;
    IBindCtx* iBindCtx = NULL;
    IMalloc* iMalloc = NULL;
    ULONG count = 0;

    if (!SUCCEEDED(CoGetMalloc(1, &iMalloc)))
        return;

	m_RunObjs.clear();

    // Query the running object table address
    if (SUCCEEDED(GetRunningObjectTable(0, &iRunningObjectTable)))
    {
        // Enum the objects registered
        if (SUCCEEDED(IRunningObjectTable_EnumRunning(iRunningObjectTable, &iEnumMoniker)))
        {
            while (IEnumMoniker_Next(iEnumMoniker, 1, &iMoniker, &count) == S_OK)
            {
                if (SUCCEEDED(CreateBindCtx(0, &iBindCtx)))
                {
                    OLECHAR* displayName = NULL;

                    // Query the object name
                    if (SUCCEEDED(IMoniker_GetDisplayName(iMoniker, iBindCtx, NULL, &displayName)))
                    {
						QString DisplayName = QString::fromWCharArray(displayName);

						QVariantMap Item;
						Item["ID"] = m_RunObjs.size();
		
						QVariantMap Values;
						Values.insert(QString::number(eName), DisplayName);

						Item["Values"] = Values;
						m_RunObjs.append(Item);

                        // Free the object name
                        IMalloc_Free(iMalloc, displayName);
                    }

                    IBindCtx_Release(iBindCtx);
                }

                IEnumMoniker_Release(iMoniker);
            }

            IEnumMoniker_Release(iEnumMoniker);
        }

        IRunningObjectTable_Release(iRunningObjectTable);
    }

    IMalloc_Release(iMalloc);

	m_pRunObjModel->Sync(m_RunObjs);
}
