#pragma once
#include <qwidget.h>
#include "..\..\API\ProcessInfo.h"
#include "..\..\Common\PanelView.h"
#include "..\..\Common\TreeWidgetEx.h"
#include "..\StatsView.h"

class CProcessView : public QWidget //CPanelView
{
	Q_OBJECT
public:
	CProcessView(QWidget *parent = 0);
	virtual ~CProcessView();

public slots:
	void					ShowProcess(const CProcessPtr& pProcess);

protected:
	//virtual void				OnMenu(const QPoint& Point);
	//virtual QTreeView*			GetView()	{ return m_pStatsList; }
	//virtual QAbstractItemModel* GetModel()	{ return m_pStatsList->model(); }

	CProcessPtr				m_pCurProcess;

private:
	QVBoxLayout*			m_pMainLayout;

	QScrollArea*			m_pScrollArea;

	QWidget*				m_pInfoWidget;
	QVBoxLayout*			m_pInfoLayout;

	QGroupBox*				m_pFileBox;
	QGridLayout*			m_pFileLayout;
	QLabel*					m_pIcon;
	QLabel*					m_pProcessName;
	QLabel*					m_pCompanyName;
	//QLabel*					m_pVersionLabel;
	QLabel*					m_pProcessVersion;
	//QLabel*					m_pFilePathLabel;
	QLineEdit*				m_pFilePath;


	QGroupBox*				m_pProcessBox;
	QGridLayout*			m_pProcessLayout;
	//QLabel*					m_pCmdLineLabel;
	QLineEdit*				m_pCmdLine;
	//QLabel*					m_pCurDirLabel;
	QLineEdit*				m_pCurDir;
	//QLabel*					m_pUserNameLabel;
	QLineEdit*				m_pUserName;
	//QLabel*					m_pProcessIdLabel;
	QLineEdit*				m_pProcessId;
	//QLabel*				m_pStartedByLabel;
	QLineEdit*				m_pStartedBy;

#ifdef WIN32
	//QLabel*					m_pPEBAddressLabel;
	QLineEdit*				m_pPEBAddress;
	QLabel*					m_ImageType;
	//QLabel*					m_pMitigationLabel;
	QLineEdit*				m_pMitigation;
	QLabel*					m_Protecetion;
#endif

	//QTreeWidgetEx*			m_pStatsList;
	CStatsView*				m_pStatsView;
};

