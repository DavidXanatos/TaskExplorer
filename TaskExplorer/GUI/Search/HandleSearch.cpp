#include "stdafx.h"
#include "HandleSearch.h"
#include "../TaskExplorer.h"
#include "../../API/Finders/AbstractFinder.h"
#ifdef WIN32
#include "../../API/Windows/ProcessHacker.h"
#endif

CHandleSearch::CHandleSearch(QWidget *parent) 
	: CSearchWindow(parent)
{
	setObjectName("HandleSearch");

	this->setWindowTitle(tr("Handle search..."));

#ifdef WIN32
	m_pType->addItem(tr("All"), -1);

	POBJECT_TYPES_INFORMATION objectTypes;
	if (NT_SUCCESS(PhEnumObjectTypes(&objectTypes)))
	{
		POBJECT_TYPE_INFORMATION objectType = (POBJECT_TYPE_INFORMATION)PH_FIRST_OBJECT_TYPE(objectTypes);
		for (ULONG i = 0; i < objectTypes->NumberOfTypes; i++)
		{
			QString Type = QString::fromWCharArray(objectType->TypeName.Buffer, objectType->TypeName.Length / sizeof(wchar_t));

			int objectIndex;
			if (WindowsVersion >= WINDOWS_8_1)
                objectIndex = objectType->TypeIndex;
            else
                objectIndex = i + 2;

			m_pType->addItem(Type, objectIndex);

			objectType = (POBJECT_TYPE_INFORMATION)PH_NEXT_OBJECT_TYPE(objectType);
		}
		PhFree(objectTypes);
	}

	//m_pType->setEditable(true); // just in case we forgot a type
#endif

	m_pHandleView = new CHandlesView(2, this);
	m_pMainLayout->addWidget(m_pHandleView);

	m_pType->setCurrentIndex(m_pType->findText(theConf->GetString("HandleSearch/Type", "")));

	restoreGeometry(theConf->GetBlob("HandleSearch/Window_Geometry"));
}

CHandleSearch::~CHandleSearch()
{
	theConf->SetValue("HandleSearch/Type", m_pType->currentText());

	theConf->SetBlob("HandleSearch/Window_Geometry",saveGeometry());
}

CAbstractFinder* CHandleSearch::NewFinder()
{
	m_Handles.clear();
	m_pHandleView->ShowHandles(m_Handles);

	QRegExp RegExp = QRegExp(m_pSearch->text(), Qt::CaseInsensitive, m_pRegExp->isChecked() ? QRegExp::RegExp : QRegExp::FixedString);
	bool bOk;
	int Type = m_pType->currentData().toInt(&bOk);
	if (!bOk)
		Type = -1;
	return CAbstractFinder::FindHandles(Type, RegExp);
}

void CHandleSearch::OnResults(QList<QSharedPointer<QObject>> List)
{
	foreach(const QSharedPointer<QObject>& pObject, List)
	{
		CHandlePtr pHandle = pObject.objectCast<CHandleInfo>();
		m_Handles.insert(pHandle->GetHandleId(), pHandle);
	}

	if (!CheckCountAndAbbort(m_Handles.count()))
		return;

	m_pHandleView->ShowHandles(m_Handles);
}
