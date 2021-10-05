#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../Models/GDIModel.h"
#include "../../../MiscHelpers/Common/SortFilterProxyModel.h"


class CSbieView : public QWidget
{
	Q_OBJECT
public:
	CSbieView(QWidget *parent = 0);
	virtual ~CSbieView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					Refresh();

private slots:

protected:

	CProcessPtr				m_pCurProcess;

private:
	QWidget*				m_GeneralWidget;
	QGridLayout*			m_pGeneralLayout;

	QLineEdit*				m_pBoxName;
	QLineEdit*				m_pUserSID;

	QLineEdit*				m_pImageName;
	QLineEdit*				m_pImageType;

	QLineEdit*				m_pFileRoot;
	QLineEdit*				m_pKeyRoot;
	QLineEdit*				m_pIpcRoot;

	QTabWidget*				m_pPaths;

	CPanelWidgetEx*			m_pFile;
	int						m_iFile;
	CPanelWidgetEx*			m_pKey;
	int						m_iKey;
	CPanelWidgetEx*			m_pIpc;
	int						m_iIpc;
	CPanelWidgetEx*			m_pWnd;
	int						m_iWnd;
	CPanelWidgetEx*			m_pConf;
	int						m_iConf;
};

