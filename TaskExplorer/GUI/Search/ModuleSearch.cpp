#include "stdafx.h"
#include "ModuleSearch.h"
#include "../TaskExplorer.h"
#include "../../API/Finders/AbstractFinder.h"
#ifdef WIN32
#include "../../API/Windows/ProcessHacker.h"
#endif

CModuleSearch::CModuleSearch(QWidget *parent) 
	: CSearchWindow(parent)
{
	setObjectName("ModuleSearch");

	this->setWindowTitle(tr("Module search..."));

	m_pType->addItem(tr("DLLs and Mapped Files"), 0);
	m_pType->addItem(tr("DLLs Only"), 1);

	m_pModuleView = new CModulesView(true, this);
	m_pMainLayout->addWidget(m_pModuleView);

	m_pType->setCurrentIndex(m_pType->findText(theConf->GetString("ModuleSearch/Type", "")));

	restoreGeometry(theConf->GetBlob("ModuleSearch/Window_Geometry"));
}

CModuleSearch::~CModuleSearch()
{
	theConf->SetValue("ModuleSearch/Type", m_pType->currentText());

	theConf->SetBlob("ModuleSearch/Window_Geometry",saveGeometry());
}

CAbstractFinder* CModuleSearch::NewFinder()
{
	m_Modules.clear();
	m_pModuleView->ShowModules(m_Modules);

	QRegExp RegExp = QRegExp(m_pSearch->text(), Qt::CaseInsensitive, m_pRegExp->isChecked() ? QRegExp::RegExp : QRegExp::FixedString);
	bool bOk;
	int Type = m_pType->currentData().toInt(&bOk);
	if (!bOk)
		Type = -1;
	return CAbstractFinder::FindModules(Type, RegExp);
}

void CModuleSearch::OnResults(QList<QSharedPointer<QObject>> List)
{
	foreach(const QSharedPointer<QObject>& pObject, List)
	{
		CModulePtr pModule = pObject.objectCast<CModuleInfo>();
		m_Modules.insert(m_Modules.count(), pModule);
	}

	if (!CheckCountAndAbbort(m_Modules.count()))
		return;

	m_pModuleView->ShowModules(m_Modules);
}
