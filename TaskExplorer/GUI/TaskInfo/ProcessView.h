#pragma once
#include <qwidget.h>
#include "../../API/ProcessInfo.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../StatsView.h"

class CProcessModel;
class QSortFilterProxyModel;
class CServicesView;
class CEnvironmentView;

class CProcessView : public QWidget //CPanelView
{
	Q_OBJECT
public:
	CProcessView(QWidget *parent = 0);
	virtual ~CProcessView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					ShowProcess(const CProcessPtr& pProcess);
	void					Refresh();

private slots:
	void					OnResetColumns();
	void					OnColumnsChanged();


	//void					OnClicked(const QModelIndex& Index);
	void					OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

	void					OnCertificate(const QString& Link);
	void					OnPolicy();
	void					OnPermissions();

protected:
	//virtual void				OnMenu(const QPoint& Point);
	//virtual QTreeView*			GetView()	{ return m_pStatsList; }
	//virtual QAbstractItemModel* GetModel()	{ return m_pStatsList->model(); }

	void					SyncModel();

	CProcessPtr				GetCurrentProcess();

	QList<CProcessPtr>		m_Processes;

private:
	QVBoxLayout*			m_pMainLayout;

	/*QScrollArea*			m_pScrollArea;

	QWidget*				m_pInfoWidget;
	QVBoxLayout*			m_pInfoLayout;*/

	QWidget*				m_pStackedWidget;
	QStackedLayout*			m_pStackedLayout;

	QWidget*				m_pOneProcWidget;
	QVBoxLayout*			m_pOneProcLayout;

	QGroupBox*				m_pFileBox;
	QGridLayout*			m_pFileLayout;
	QLabel*					m_pIcon;
	QLabel*					m_pProcessName;
	QLabel*					m_pCompanyName;
	QLabel*					m_pProcessVersion;
	QLabel*					m_pSubSystem;
	QLineEdit*				m_pFilePath;
#ifdef WIN32
	QLineEdit*				m_pFilePathNt;
#endif

	QTabWidget*				m_pTabWidget;

	QScrollArea*			m_pProcessArea;
	//QGroupBox*				m_pProcessBox;
	QWidget*				m_pProcessBox;
	QGridLayout*			m_pProcessLayout;
	QLineEdit*				m_pCmdLine;
	QLineEdit*				m_pCurDir;
#ifdef WIN32
	QLineEdit*				m_pDesktop;
	QLabel*					m_pDPIAware;
#else
	QLineEdit*				m_pUserName;
#endif
	QLineEdit*				m_pProcessId;
	QLineEdit*				m_pStartedBy;

	QWidget*				m_pMultiProcWidget;
	QVBoxLayout*			m_pMultiProcLayout;

	CProcessModel*			m_pProcessModel;
	QSortFilterProxyModel*	m_pSortProxy;

	QTreeViewEx*			m_pProcessList;

#ifdef WIN32
	QLineEdit*				m_pPEBAddress;
	QLabel*					m_ImageType;

	//QGroupBox*				m_pSecurityBox;
	QWidget*				m_pSecurityBox;
	QGridLayout*			m_pSecurityLayout;

	QLabel*					m_pVerification;
	QLabel*					m_pSigner;

	CPanelWidgetEx*			m_pMitigation;
	QLabel*					m_Protecetion;
	QCheckBox*				m_pNoWriteUp;
	QCheckBox*				m_pNoReadUp;
	QCheckBox*				m_pNoExecuteUp;
	QPushButton*			m_pPermissions;

	//QGroupBox*				m_pAppBox;
	QWidget*				m_pAppBox;
	QGridLayout*			m_pAppLayout;

	QLineEdit*				m_pAppID;
	QLineEdit*				m_pPackageName;
	//QLineEdit*				m_pPackageDataDir;

	CServicesView*			m_pServiceView;
#endif
	CEnvironmentView*		m_pEnvironmentView;


	CStatsView*				m_pStatsView;
};

