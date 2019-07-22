#pragma once

#include "../TaskInfo/ModulesView.h"
#include "SearchWindow.h"

class CModuleSearch : public CSearchWindow
{
	Q_OBJECT
public:
	CModuleSearch(QWidget *parent = Q_NULLPTR);
	virtual ~CModuleSearch();

private slots:
	virtual void			OnResults(QList<QSharedPointer<QObject>> List);

protected:
	virtual CAbstractFinder* NewFinder();

	QMap<quint64, CModulePtr> m_Modules;

	CModulesView*		m_pModuleView;
};