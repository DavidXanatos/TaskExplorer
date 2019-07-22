#pragma once

#include "../../API/ProcessInfo.h"
#include "StringView.h"
#include "SearchWindow.h"

class CMemorySearch : public CSearchWindow
{
	Q_OBJECT
public:
	CMemorySearch(const CProcessPtr& pProcess = CProcessPtr(), QWidget *parent = Q_NULLPTR);
	virtual ~CMemorySearch();

private slots:
	virtual void			OnType();

	virtual void			OnResults(QList<QSharedPointer<QObject>> List);

protected:
	virtual CAbstractFinder* NewFinder();

	CProcessPtr			m_CurProcess;

	QMap<quint64, CStringInfoPtr> m_Strings;

	QWidget*			m_pStringWidget;
	QHBoxLayout*		m_pStringLayout;

	QSpinBox*			m_pMinLength;
	QCheckBox*			m_pUnicode;
	QCheckBox*			m_pExtUnicode;

	QCheckBox*			m_pPrivate;
	QCheckBox*			m_pImage;
	QCheckBox*			m_pMapped;

	CStringView*		m_pStringView;
};