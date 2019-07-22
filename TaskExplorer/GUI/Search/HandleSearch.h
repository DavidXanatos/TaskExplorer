#pragma once

#include "../TaskInfo/HandlesView.h"
#include "SearchWindow.h"

class CHandleSearch : public CSearchWindow
{
	Q_OBJECT
public:
	CHandleSearch(QWidget *parent = Q_NULLPTR);
	virtual ~CHandleSearch();

private slots:
	virtual void			OnResults(QList<QSharedPointer<QObject>> List);

protected:
	virtual CAbstractFinder* NewFinder();

	QMap<quint64, CHandlePtr> m_Handles;

	CHandlesView*		m_pHandleView;
};