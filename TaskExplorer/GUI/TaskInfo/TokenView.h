#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../../API/Windows/WinProcess.h"

class CTokenView : public CPanelView
{
	Q_OBJECT
public:
	CTokenView(QWidget *parent = 0);
	virtual ~CTokenView();


public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					ShowToken(const CWinTokenPtr& pToken);
	void					Refresh();

private slots:
	//void					OnMenu(const QPoint &point);

	void					OnTokenAction();

	void					OnOriginalToken();
	void					OnDefaultToken();
	void					OnPermissions();
	void					OnChangeIntegrity();
	void					OnChangeVirtualization();
	void					OnLinkedToken();

	void					SetFilter(const QRegularExpression& Exp, bool bHighLight = false, int Col = -1); // -1 = any

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pTokenList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pTokenList->model(); }

	QSharedPointer<CWinProcess>	m_pCurProcess;
	CWinTokenPtr				m_pCurToken;

private:
	void	UpdateGeneral();
	void	UpdateAdvanced();
	void	UpdateContainer();
	void	UpdateCapabilities();
	void	UpdateClaims();
	void	UpdateAttributes(QMap<QString, CWinToken::SAttribute> Attributes, QTreeWidgetItem* pRoot = NULL);
	void	UpdateAttributes();

	bool					m_LockValues;

	enum EStackColumns
	{
		eName = 0,
		eStatus,
		eDescription,
		eSID,
		eType,
		eUse,
		eCount
	};

	QTabWidget*				m_pTabWidget;

	QWidget*				m_GeneralWidget;
	QGridLayout*			m_pGeneralLayout;

	QLineEdit*				m_pUser;
	QLineEdit*				m_pUserSID;

	QLabel*					m_pSession;
	QLabel*					m_pElevated;
	QCheckBox*				m_pVirtualized;

	QLineEdit*				m_pOwner;
	QLineEdit*				m_pGroup;

	QTreeWidgetEx*			m_pTokenList;
	QTreeWidgetItem*		m_pDangerousFlags;
	QTreeWidgetItem*		m_pPrivileges;
	QTreeWidgetItem*		m_pGroups;
	QTreeWidgetItem*		m_pRestrictingSIDs;

	CFinder*				m_pFinder;

	QPushButton*			m_pOriginalToken;
	QPushButton*			m_pDefaultToken;
	QPushButton*			m_pPermissions;
	QComboBox*				m_pIntegrity;
	QPushButton*			m_pLinkedToken;


	CPanelWidgetEx*	m_pAdvanced;
	QTreeWidgetItem*			m_pAdvGeneral;
	QTreeWidgetItem*				m_pAdvType;
	QTreeWidgetItem*				m_pAdvImpersonation;
	QTreeWidgetItem*			m_pAdvSource;
	QTreeWidgetItem*				m_pAdvName;
	QTreeWidgetItem*				m_pAdvLUID;
	QTreeWidgetItem*			m_pAdvLUIDs;
	QTreeWidgetItem*				m_pAdvTokenLUID;
	QTreeWidgetItem*				m_pAdvAuthenticationLUID;
	QTreeWidgetItem*				m_pAdvModifiedLUID;
	QTreeWidgetItem*				m_pAdvOriginLUIDs;
	QTreeWidgetItem*			m_pAdvMemory;
	QTreeWidgetItem*				m_pAdvMemUsed;
	QTreeWidgetItem*				m_pAdvMemAvail;
	QTreeWidgetItem*			m_pAdvProperties;
	QTreeWidgetItem*				m_pAdvTokenPath;
	QTreeWidgetItem*				m_pAdvTokenSDDL;
	QTreeWidgetItem*			m_pAdvTrustLevel;
	QTreeWidgetItem*				m_pAdvTrustLevelSID;
	QTreeWidgetItem*				m_pAdvTrustLevelName;
	QTreeWidgetItem*			m_pAdvLogon;
	QTreeWidgetItem*				m_pAdvLogonSID;
	QTreeWidgetItem*				m_pAdvLogonName;
	QTreeWidgetItem*			m_pAdvProfile;
	QTreeWidgetItem*				m_pAdvProfilePath;
	QTreeWidgetItem*				m_pAdvProfileKey;

	CPanelWidgetEx*	m_pContainer;
	QTreeWidgetItem*			m_pContGeneral;
	QTreeWidgetItem*				m_pContName;
	QTreeWidgetItem*				m_pContType;
	QTreeWidgetItem*				m_pContSID;
	QTreeWidgetItem*			m_pContProperties;
	QTreeWidgetItem*				m_pContNumber;
	QTreeWidgetItem*				m_pContLPAC;
	QTreeWidgetItem*				m_pContObjectPath;
	QTreeWidgetItem*			m_pContParent;
	QTreeWidgetItem*				m_pContParentName;
	QTreeWidgetItem*				m_pContParentSID;
	QTreeWidgetItem*			m_pContPackage;
	QTreeWidgetItem*				m_pContPackageName;
	QTreeWidgetItem*				m_pContPackagePath;
	QTreeWidgetItem*			m_pContProfile;
	QTreeWidgetItem*				m_pContProfilePath;
	QTreeWidgetItem*				m_pContProfileKey;

	CPanelWidgetEx*	m_pCapabilities;

	CPanelWidgetEx*	m_pClaims;
	QTreeWidgetItem*					m_pUserClaims;
	QTreeWidgetItem*					m_pDeviceClaims;


	CPanelWidgetEx*	m_pAttributes;

	QAction*				m_pEnable;
	QAction*				m_pDisable;
	QAction*				m_pReset;
	//QAction*				m_pAdd;
	QAction*				m_pRemove;
};

