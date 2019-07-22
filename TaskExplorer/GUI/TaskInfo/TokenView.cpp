#include "stdafx.h"
#include "GUI/TaskExplorer.h"
#include "../../Common/SettingsWidgets.h"
#include "TokenView.h"
#include "TaskInfoWindow.h"
#include "../API/Windows/ProcessHacker.h"
#undef GetUserName

CTokenView::CTokenView(QWidget *parent)
	:CPanelView(parent)
{

	QVBoxLayout* m_pMainLayout = new QVBoxLayout();
	//m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_GeneralWidget = new QWidget();
	m_pMainLayout->addWidget(m_GeneralWidget);

	m_pGeneralLayout = new QGridLayout();
	//m_pGeneralLayout->setMargin(0);
	m_GeneralWidget->setLayout(m_pGeneralLayout);
	int row = 0;

	m_pGeneralLayout->addWidget(new QLabel(tr("User:")), row, 0, 1, 1);
	m_pUser = new QLineEdit();
	m_pUser->setReadOnly(true);
	m_pGeneralLayout->addWidget(m_pUser, row, 1, 1, 5);
	row++;

	m_pGeneralLayout->addWidget(new QLabel(tr("User SID:")), row, 0, 1, 1);
	m_pUserSID = new QLineEdit();
	m_pUserSID->setReadOnly(true);
	m_pGeneralLayout->addWidget(m_pUserSID, row, 1, 1, 5);
	row++;

	QHBoxLayout* hBoxLayout = new QHBoxLayout();
	m_pGeneralLayout->addLayout(hBoxLayout, row, 1, 1, 5);

	m_pGeneralLayout->addWidget(new QLabel(tr("Owner:")), row, 0, 1, 1);
	m_pOwner = new QLineEdit();
	m_pOwner->setReadOnly(true);
	//m_pOwner ->setSizePolicy(QSizePolicy::Expanding, m_pOwner->sizePolicy().verticalPolicy());
	hBoxLayout->addWidget(m_pOwner); // , row, 1, 1, 2);

	hBoxLayout->addWidget(new QLabel(tr("Group:"))); // , row, 3, 1, 1);
	m_pGroup = new QLineEdit();
	m_pGroup->setReadOnly(true);
	//m_pGroup ->setSizePolicy(QSizePolicy::Expanding, m_pGroup->sizePolicy().verticalPolicy());
	hBoxLayout->addWidget(m_pGroup); // ,row , 4, 1, 2);
	row++;

	m_pGeneralLayout->addWidget(new QLabel(tr("Session:")), row, 0, 1, 1);
	m_pSession = new QLabel("0");
	m_pSession ->setSizePolicy(QSizePolicy::Expanding, m_pSession->sizePolicy().verticalPolicy());
	m_pGeneralLayout->addWidget(m_pSession, row, 1, 1, 1);

	m_pGeneralLayout->addWidget(new QLabel(tr("Elevanted:")), row, 2, 1, 1);
	m_pElevated = new QLabel("No");
	m_pElevated ->setSizePolicy(QSizePolicy::Expanding, m_pElevated->sizePolicy().verticalPolicy());
	m_pGeneralLayout->addWidget(m_pElevated, row, 3, 1, 1);

	m_pGeneralLayout->addWidget(new QLabel(tr("Virtualized:")), row, 4, 1, 1);
	m_pVirtualized = new QLabel("No");
	m_pVirtualized ->setSizePolicy(QSizePolicy::Expanding, m_pVirtualized->sizePolicy().verticalPolicy());
	m_pGeneralLayout->addWidget(m_pVirtualized, row, 5, 1, 1);
	row++;

	m_pTabWidget = new QTabWidget();
	//m_pTabWidget->setTabPosition(QTabWidget::East);
	m_pMainLayout->addWidget(m_pTabWidget);

	// Variables List
	m_pTokenList = new QTreeWidgetEx();
	m_pTokenList->setItemDelegate(theGUI->GetItemDelegate());
	m_pTokenList->setHeaderLabels(tr("Name|Status|Description").split("|"));

	m_pTokenList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTokenList->setSortingEnabled(false);

	m_pTokenList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTokenList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//connect(m_pTokenList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemDoubleClicked(QTreeWidgetItem*, int)));
	//m_pGeneralLayout->addWidget(m_pTokenList, 3, 0, 1, 6);

	m_pTabWidget->addTab(m_pTokenList, tr("General"));
	// 
	//m_pDangerousFlags = new QTreeWidgetItem(tr("Dangerous Flags").split("|"));
	//m_pTokenList->addTopLevelItem(m_pDangerousFlags);
	m_pPrivileges = new QTreeWidgetItem(tr("Privileges").split("|"));
	m_pTokenList->addTopLevelItem(m_pPrivileges);
	m_pGroups = new QTreeWidgetItem(tr("Groups").split("|"));
	m_pTokenList->addTopLevelItem(m_pGroups);
	m_pRestrictingSIDs = new QTreeWidgetItem(tr("Restricting SIDs").split("|"));
	m_pTokenList->addTopLevelItem(m_pRestrictingSIDs);
	m_pTokenList->expandAll();

	//
	m_pAdvanced = new CPanelWidget<QTreeWidgetEx>();

	m_pAdvanced->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pAdvanced->GetView())->setHeaderLabels(tr("Name|Value").split("|"));

	m_pAdvanced->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pAdvanced->GetView()->setSortingEnabled(false);

	m_pTabWidget->addTab(m_pAdvanced, tr("Advanced"));
	//

	m_pAdvGeneral = new QTreeWidgetItem(tr("General").split("|"));
	m_pAdvanced->GetTree()->addTopLevelItem(m_pAdvGeneral);
		m_pAdvType = new QTreeWidgetItem(tr("Type").split("|"));
		m_pAdvGeneral->addChild(m_pAdvType);
		m_pAdvImpersonation = new QTreeWidgetItem(tr("Impersonation").split("|"));
		m_pAdvGeneral->addChild(m_pAdvImpersonation);
	m_pAdvSource = new QTreeWidgetItem(tr("Source").split("|"));
	m_pAdvanced->GetTree()->addTopLevelItem(m_pAdvSource);
		m_pAdvName = new QTreeWidgetItem(tr("Name").split("|"));
		m_pAdvSource->addChild(m_pAdvName);
		m_pAdvLUID = new QTreeWidgetItem(tr("LUID").split("|"));
		m_pAdvSource->addChild(m_pAdvLUID);
	m_pAdvLUIDs = new QTreeWidgetItem(tr("LUIDs").split("|"));
	m_pAdvanced->GetTree()->addTopLevelItem(m_pAdvLUIDs);
		m_pAdvTokenLUID = new QTreeWidgetItem(tr("Token LUID").split("|"));
		m_pAdvLUIDs->addChild(m_pAdvTokenLUID);
		m_pAdvAuthenticationLUID = new QTreeWidgetItem(tr("Authentication LUID").split("|"));
		m_pAdvLUIDs->addChild(m_pAdvAuthenticationLUID);
		m_pAdvModifiedLUID = new QTreeWidgetItem(tr("Modified LUID").split("|"));
		m_pAdvLUIDs->addChild(m_pAdvModifiedLUID);
		m_pAdvOriginLUIDs = new QTreeWidgetItem(tr("Origin LUID").split("|"));
		m_pAdvLUIDs->addChild(m_pAdvOriginLUIDs);
	m_pAdvMemory = new QTreeWidgetItem(tr("Memory").split("|"));
	m_pAdvanced->GetTree()->addTopLevelItem(m_pAdvMemory);
		m_pAdvMemUsed = new QTreeWidgetItem(tr("Memory used").split("|"));
		m_pAdvMemory->addChild(m_pAdvMemUsed);
		m_pAdvMemAvail = new QTreeWidgetItem(tr("Memory available").split("|"));
		m_pAdvMemory->addChild(m_pAdvMemAvail);
	m_pAdvProperties = new QTreeWidgetItem(tr("Properties").split("|"));
	m_pAdvanced->GetTree()->addTopLevelItem(m_pAdvProperties);
		m_pAdvTokenPath = new QTreeWidgetItem(tr("Token object path").split("|"));
		m_pAdvProperties->addChild(m_pAdvTokenPath);
		m_pAdvTokenSDDL = new QTreeWidgetItem(tr("Token SDDL").split("|"));
		m_pAdvProperties->addChild(m_pAdvTokenSDDL);
	m_pAdvTrustLevel = new QTreeWidgetItem(tr("Trust level").split("|"));
	m_pAdvanced->GetTree()->addTopLevelItem(m_pAdvTrustLevel);
		m_pAdvTrustLevelSID = new QTreeWidgetItem(tr("Trust level SID").split("|"));
		m_pAdvTrustLevel->addChild(m_pAdvTrustLevelSID);
		m_pAdvTrustLevelName = new QTreeWidgetItem(tr("Trust level name").split("|"));
		m_pAdvTrustLevel->addChild(m_pAdvTrustLevelName);
	m_pAdvLogon = new QTreeWidgetItem(tr("Logon").split("|"));
	m_pAdvanced->GetTree()->addTopLevelItem(m_pAdvLogon);
		m_pAdvLogonSID = new QTreeWidgetItem(tr("Token login name").split("|"));
		m_pAdvLogon->addChild(m_pAdvLogonSID);
		m_pAdvLogonName = new QTreeWidgetItem(tr("Token login SID").split("|"));
		m_pAdvLogon->addChild(m_pAdvLogonName);
	m_pAdvProfile = new QTreeWidgetItem(tr("Profile").split("|"));
	m_pAdvanced->GetTree()->addTopLevelItem(m_pAdvProfile);
		m_pAdvProfilePath = new QTreeWidgetItem(tr("Folder path").split("|"));
		m_pAdvProfile->addChild(m_pAdvProfilePath);
		m_pAdvProfileKey = new QTreeWidgetItem(tr("Registry path").split("|"));
		m_pAdvProfile->addChild(m_pAdvProfileKey);

	m_pAdvanced->GetTree()->expandAll();

	//
	m_pContainer = new CPanelWidget<QTreeWidgetEx>();

	m_pContainer->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pContainer->GetView())->setHeaderLabels(tr("Name|Value").split("|"));

	m_pContainer->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pContainer->GetView()->setSortingEnabled(false);

	m_pTabWidget->addTab(m_pContainer, tr("Container"));
	//

	m_pContGeneral = new QTreeWidgetItem(tr("General").split("|"));
	m_pContainer->GetTree()->addTopLevelItem(m_pContGeneral);
		m_pContName = new QTreeWidgetItem(tr("Name").split("|"));
		m_pContGeneral->addChild(m_pContName);
		m_pContType = new QTreeWidgetItem(tr("Type").split("|"));
		m_pContGeneral->addChild(m_pContType);
		m_pContSID = new QTreeWidgetItem(tr("SID").split("|"));
		m_pContGeneral->addChild(m_pContSID);
	m_pContProperties = new QTreeWidgetItem(tr("Properties").split("|"));
	m_pContainer->GetTree()->addTopLevelItem(m_pContProperties);
		m_pContNumber = new QTreeWidgetItem(tr("Number").split("|"));
		m_pContProperties->addChild(m_pContNumber);
		m_pContLPAC = new QTreeWidgetItem(tr("LPAC").split("|"));
		m_pContProperties->addChild(m_pContLPAC);
		m_pContObjectPath = new QTreeWidgetItem(tr("Token object path").split("|"));
		m_pContProperties->addChild(m_pContObjectPath);
	m_pContParent = new QTreeWidgetItem(tr("Parent").split("|"));
	m_pContainer->GetTree()->addTopLevelItem(m_pContParent);
		m_pContParentName = new QTreeWidgetItem(tr("Name").split("|"));
		m_pContParent->addChild(m_pContParentName);
		m_pContParentSID = new QTreeWidgetItem(tr("SID").split("|"));
		m_pContParent->addChild(m_pContParentSID);
	m_pContPackage = new QTreeWidgetItem(tr("Package").split("|"));
	m_pContainer->GetTree()->addTopLevelItem(m_pContPackage);
		m_pContPackageName = new QTreeWidgetItem(tr("Name").split("|"));
		m_pContPackage->addChild(m_pContPackageName);
		m_pContPackagePath = new QTreeWidgetItem(tr("Path").split("|"));
		m_pContPackage->addChild(m_pContPackagePath);
	m_pContProfile = new QTreeWidgetItem(tr("Profile").split("|"));
	m_pContainer->GetTree()->addTopLevelItem(m_pContProfile);
		m_pContProfilePath = new QTreeWidgetItem(tr("Folder path").split("|"));
		m_pContProfile->addChild(m_pContProfilePath);
		m_pContProfileKey = new QTreeWidgetItem(tr("Registry path").split("|"));
		m_pContProfile->addChild(m_pContProfileKey);

	m_pContainer->GetTree()->expandAll();

	//
	m_pCapabilities = new CPanelWidget<QTreeWidgetEx>();

	m_pCapabilities->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pCapabilities->GetView())->setHeaderLabels(tr("Capabilities").split("|"));

	m_pCapabilities->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pCapabilities->GetView()->setSortingEnabled(false);

	m_pTabWidget->addTab(m_pCapabilities, tr("Capabilities"));
	//

	
	//
	m_pClaims = new CPanelWidget<QTreeWidgetEx>();

	m_pClaims->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pClaims->GetView())->setHeaderLabels(tr("Claims").split("|"));

	m_pClaims->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pClaims->GetView()->setSortingEnabled(false);

	m_pTabWidget->addTab(m_pClaims, tr("Claims"));
	//

	m_pUserClaims = new QTreeWidgetItem(tr("User claims").split("|"));
	m_pDeviceClaims = new QTreeWidgetItem(tr("Device claims").split("|"));
	
	//
	m_pAttributes = new CPanelWidget<QTreeWidgetEx>();

	m_pAttributes->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pAttributes->GetView())->setHeaderLabels(tr("Attributes").split("|"));

	m_pAttributes->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pAttributes->GetView()->setSortingEnabled(false);

	m_pTabWidget->addTab(m_pAttributes, tr("Attributes"));
	//


	QHBoxLayout* horizontalLayout = new QHBoxLayout();
	//horizontalLayout->setMargin(6);
    horizontalLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
	m_pMainLayout->addLayout(horizontalLayout);

    m_pDefaultToken = new QPushButton();
	m_pDefaultToken->setText(tr("Default token"));
    horizontalLayout->addWidget(m_pDefaultToken);
	connect(m_pDefaultToken, SIGNAL(pressed()), this, SLOT(OnDefaultToken()));

    m_pPermissions = new QPushButton();
	m_pPermissions->setText(tr("Permissions"));
    horizontalLayout->addWidget(m_pPermissions);
	connect(m_pPermissions, SIGNAL(pressed()), this, SLOT(OnPermissions()));

	m_pIntegrity = new QComboBox();
	m_pIntegrity->addItem(tr("Protected"), MandatorySecureProcessRID);
	m_pIntegrity->addItem(tr("System"), MandatorySystemRID);
	m_pIntegrity->addItem(tr("High"), MandatoryHighRID);
	m_pIntegrity->addItem(tr("Medium"), MandatoryMediumRID);
	m_pIntegrity->addItem(tr("Low"), MandatoryLowRID);
	m_pIntegrity->addItem(tr("Untrusted"), MandatoryUntrustedRID);
	horizontalLayout->addWidget(m_pIntegrity);
	connect(m_pIntegrity, SIGNAL(currentIndexChanged(int)), this, SLOT(OnChangeIntegrity()));

	m_pLinkedToken = new QPushButton();
	m_pLinkedToken->setText(tr("Linked Token"));
    horizontalLayout->addWidget(m_pLinkedToken);
	connect(m_pLinkedToken, SIGNAL(pressed()), this, SLOT(OnLinkedToken()));

	//m_pMenu = new QMenu();

	m_pEnable = m_pMenu->addAction(tr("Enable"), this, SLOT(OnTokenAction()));
	m_pDisable = m_pMenu->addAction(tr("Disable"), this, SLOT(OnTokenAction()));
	m_pReset = m_pMenu->addAction(tr("Reset"), this, SLOT(OnTokenAction()));
	//m_pAdd = m_pMenu->addAction(tr("Add"), this, SLOT(OnTokenAction()));
	m_pRemove = m_pMenu->addAction(tr("Remove"), this, SLOT(OnTokenAction()));
	
	AddPanelItemsToMenu();

	setObjectName(parent ? parent->objectName() : "InfoWindow");
	m_pTokenList->header()->restoreState(theConf->GetBlob(objectName() + "/TokenView_Columns"));
	m_pAdvanced->GetTree()->header()->restoreState(theConf->GetBlob(objectName() + "/TokenViewAdvanced_Columns"));
	m_pContainer->GetTree()->header()->restoreState(theConf->GetBlob(objectName() + "/TokenViewContainer_Columns"));
}

CTokenView::~CTokenView()
{
	theConf->SetBlob(objectName() + "/TokenView_Columns", m_pTokenList->header()->saveState());
	theConf->SetBlob(objectName() + "/TokenViewAdvanced_Columns", m_pAdvanced->GetTree()->header()->saveState());
	theConf->SetBlob(objectName() + "/TokenViewContainer_Columns", m_pContainer->GetTree()->header()->saveState());
}

void CTokenView::ShowProcess(const CProcessPtr& pProcess)
{
	m_pCurProcess = pProcess.objectCast<CWinProcess>();

	ShowToken(m_pCurProcess->GetToken());
}

void CTokenView__SetRowColor(QTreeWidgetItem* pItem, bool bEnabled, bool bModified)
{
	for (int i = 0; i < pItem->columnCount(); i++)
	{
		if (bEnabled)
		{
			if (bModified)
				pItem->setBackgroundColor(i, QColor(192, 240, 192));
			else
				pItem->setBackgroundColor(i, QColor(224, 240, 224));
		}
		else
		{
			if (bModified)
				pItem->setBackgroundColor(i, QColor(240, 192, 192));
			else
				pItem->setBackgroundColor(i, QColor(240, 224, 224));
		}
	}
}

void CTokenView::ShowToken(const CWinTokenPtr& pToken)
{
	if (m_pCurToken != pToken)
	{
		m_pCurToken = pToken;
	}

	Refresh();

	m_pCapabilities->GetTree()->expandAll();
	m_pClaims->GetTree()->expandAll();
	m_pAttributes->GetTree()->expandAll();
}

void CTokenView::Refresh()
{
	if (!m_pCurToken)
		return;

	m_pCurToken->UpdateExtendedData();

	m_pContainer->setEnabled(m_pCurToken->IsAppContainer());

	UpdateGeneral();
	UpdateAdvanced();
	if(m_pCurToken->IsAppContainer())
		UpdateContainer();
	UpdateCapabilities();
	UpdateClaims();
	UpdateAttributes();
}

template<class T>
void CTokenView__SetTextIfChanged(T pEdit, const QString& Text) {
	QString curText = pEdit->text();
	if (curText != Text)
		pEdit->setText(Text);
}

void CTokenView::UpdateGeneral()
{
	CTokenView__SetTextIfChanged(m_pUser, m_pCurToken->GetUserName()); // note: this is being resolved asynchroniusly
	CTokenView__SetTextIfChanged(m_pUserSID, m_pCurToken->GetSidString());

	CTokenView__SetTextIfChanged(m_pSession, QString::number(m_pCurToken->GetSessionId()));
	CTokenView__SetTextIfChanged(m_pElevated, m_pCurToken->GetElevationString());
	CTokenView__SetTextIfChanged(m_pVirtualized, m_pCurToken->GetVirtualizationString());

	CTokenView__SetTextIfChanged(m_pOwner, m_pCurToken->GetOwnerName()); // note: this is being resolved asynchroniusly
	CTokenView__SetTextIfChanged(m_pGroup, m_pCurToken->GetGroupName()); // note: this is being resolved asynchroniusly

	quint32 integrityLevelRID = m_pCurToken->GetIntegrityLevel();
	int InegrityIndex = m_pIntegrity->findData(integrityLevelRID);
	if (InegrityIndex != m_pIntegrity->currentIndex())
	{
		m_pIntegrity->setCurrentIndex(InegrityIndex);

		for (ULONG i = 0; i < m_pIntegrity->count(); i++)
        {
			bool disabled = m_pIntegrity->itemData(i).toUInt() > integrityLevelRID;

			QStandardItemModel *model = qobject_cast<QStandardItemModel *>(m_pIntegrity->model());
			QStandardItem *item = model->item(i);
			item->setFlags(disabled ? item->flags() & ~Qt::ItemIsEnabled : item->flags() | Qt::ItemIsEnabled);
        }
	}

	QMap<QString, QTreeWidgetItem*> OldPrivileges;
	for(int i = 0; i < m_pPrivileges->childCount(); ++i)
	{
		QTreeWidgetItem* pItem = m_pPrivileges->child(i);
		QString Name = pItem->data(0, Qt::UserRole).toString();
		ASSERT(!OldPrivileges.contains(Name));
		OldPrivileges.insert(Name,pItem);
	}

	QMap<QString, CWinToken::SPrivilege> Privileges = m_pCurToken->GetPrivileges();
	foreach (const CWinToken::SPrivilege& Privilege, Privileges)
	{
		QTreeWidgetItem* pItem = OldPrivileges.take(Privilege.Name);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setText(eName, Privilege.Name);
			pItem->setData(eName, Qt::UserRole, Privilege.Name);
			pItem->setText(eDescription, Privilege.Description);
			m_pPrivileges->addChild(pItem);
		}

		CTokenView__SetRowColor(pItem, CWinToken::IsPrivilegeEnabled(Privilege.Attributes), CWinToken::IsPrivilegeModified(Privilege.Attributes));
		pItem->setText(eStatus, CWinToken::GetPrivilegeAttributesString(Privilege.Attributes));
	}

	foreach(QTreeWidgetItem* pCurItem, OldPrivileges)
		delete pCurItem;



	QMap<QByteArray, QTreeWidgetItem*> OldGroups;
	for(int i = 0; i < m_pGroups->childCount(); ++i)
	{
		QTreeWidgetItem* pItem = m_pGroups->child(i);
		QByteArray Sid = pItem->data(0, Qt::UserRole).toByteArray();
		ASSERT(!OldGroups.contains(Sid));
		OldGroups.insert(Sid,pItem);
	}

	for(int i = 0; i < m_pRestrictingSIDs->childCount(); ++i)
	{
		QTreeWidgetItem* pItem = m_pRestrictingSIDs->child(i);
		QByteArray Sid = pItem->data(0, Qt::UserRole).toByteArray();
		ASSERT(!OldGroups.contains(Sid));
		OldGroups.insert(Sid,pItem);
	}

	QMap<QByteArray, CWinToken::SGroup> Groups = m_pCurToken->GetGroups();
	foreach (const CWinToken::SGroup& Group, Groups)
	{
		QTreeWidgetItem* pItem = OldGroups.take(Group.Sid);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(eName, Qt::UserRole, Group.Sid);
			pItem->setText(eDescription, CWinToken::GetGroupDescription(Group.Attributes));
			if(Group.Restricted)
				m_pRestrictingSIDs->addChild(pItem);
			else
				m_pGroups->addChild(pItem);
		}

		pItem->setText(eName, Group.Name); // note: this is being resolved asynchroniusly
		
		CTokenView__SetRowColor(pItem, CWinToken::IsGroupEnabled(Group.Attributes), CWinToken::IsGroupModified(Group.Attributes));
		pItem->setText(eStatus, CWinToken::GetGroupStatusString(Group.Attributes, Group.Restricted));
	}

	foreach(QTreeWidgetItem* pCurItem, OldGroups)
		delete pCurItem;

	//m_pDangerousFlags->setHidden(m_pDangerousFlags->childCount() == 0);
	m_pPrivileges->setHidden(m_pPrivileges->childCount() == 0);
	m_pGroups->setHidden(m_pGroups->childCount() == 0);
	m_pRestrictingSIDs->setHidden(m_pRestrictingSIDs->childCount() == 0);
}

void CTokenView::UpdateAdvanced()
{
	CWinToken::SAdvancedInfo AdvancedInfo = m_pCurToken->GetAdvancedInfo();

	m_pAdvType->setText(1, AdvancedInfo.tokenType);
	m_pAdvImpersonation->setText(1, AdvancedInfo.tokenImpersonationLevel);

	m_pAdvName->setText(1, AdvancedInfo.sourceName);
	m_pAdvLUID->setText(1, AdvancedInfo.sourceLuid);

	m_pAdvTokenLUID->setText(1, tr("0x%1").arg(AdvancedInfo.tokenLuid, 0, 16));
	m_pAdvAuthenticationLUID->setText(1, tr("0x%1").arg(AdvancedInfo.authenticationLuid, 0, 16));
	m_pAdvModifiedLUID->setText(1, tr("0x%1").arg(AdvancedInfo.tokenModifiedLuid, 0, 16));
	m_pAdvOriginLUIDs->setText(1, tr("0x%1").arg(AdvancedInfo.tokenOriginLogonSession, 0, 16));

	m_pAdvMemUsed->setText(1, FormatSize(AdvancedInfo.memoryUsed));
	m_pAdvMemAvail->setText(1, FormatSize(AdvancedInfo.memoryAvailable));

	m_pAdvTokenPath->setText(1, AdvancedInfo.tokenNamedObjectPath);
	m_pAdvTokenSDDL->setText(1, AdvancedInfo.tokenSecurityDescriptor);

	m_pAdvTrustLevelSID->setText(1, AdvancedInfo.tokenTrustLevelSid);
	m_pAdvTrustLevelName->setText(1, AdvancedInfo.tokenTrustLevelName);

	m_pAdvLogonSID->setText(1, AdvancedInfo.tokenLogonSid);
	m_pAdvLogonName->setText(1, AdvancedInfo.tokenLogonName);

	m_pAdvProfilePath->setText(1, AdvancedInfo.tokenProfilePath);
	m_pAdvProfileKey->setText(1, AdvancedInfo.tokenProfileRegistry);
}

void CTokenView::UpdateContainer()
{
	CWinToken::SContainerInfo ContainerInfo = m_pCurToken->GetContainerInfo();

	m_pContName->setText(1, ContainerInfo.appContainerName);
	m_pContType->setText(1, ContainerInfo.appContainerSid);
	m_pContSID->setText(1, ContainerInfo.appContainerSidType);

	m_pContNumber->setText(1, QString::number(ContainerInfo.appContainerNumber));
	m_pContLPAC->setText(1, ContainerInfo.isLessPrivilegedAppContainer ? tr("True") : tr("False"));
	m_pContObjectPath->setText(1, ContainerInfo.tokenNamedObjectPath);

	m_pContParentName->setText(1, ContainerInfo.parentContainerName);
	m_pContParentSID->setText(1, ContainerInfo.parentContainerSid);

	m_pContPackageName->setText(1, ContainerInfo.packageFullName);
	m_pContPackagePath->setText(1, ContainerInfo.packagePath);

	m_pContProfilePath->setText(1, ContainerInfo.appContainerFolderPath);
	m_pContProfileKey->setText(1, ContainerInfo.appContainerRegistryPath);
}

void CTokenView__UpdateCapabilityValue(QTreeWidgetItem* pItem, const QString& Name, const QString& Value)
{
	if (Value.isEmpty())
		pItem->setHidden(true);
	else {
		pItem->setHidden(false);
		pItem->setText(0, CTokenView::tr("%1: %2").arg(Name).arg(Value));
	}
}

void CTokenView::UpdateCapabilities()
{
	QMap<QByteArray, CWinToken::SCapability> Capabilities = m_pCurToken->GetCapabilities();

	QMap<QByteArray, QTreeWidgetItem*> OldCapabilities;
	for(int i = 0; i < m_pCapabilities->GetTree()->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* pItem = m_pCapabilities->GetTree()->topLevelItem(i);
		QByteArray sid = pItem->data(0, Qt::UserRole).toByteArray();
		Q_ASSERT(!OldCapabilities.contains(sid));
		OldCapabilities.insert(sid,pItem);
	}

	for(QMap<QByteArray, CWinToken::SCapability>::iterator I = Capabilities.begin(); I != Capabilities.end(); ++I)
	{
		QTreeWidgetItem* pItem = OldCapabilities.take(I.key());
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, I.key());
			pItem->setText(0, I.value().SidString);
			m_pCapabilities->GetTree()->addTopLevelItem(pItem);

			pItem->addChild(new QTreeWidgetItem());
			pItem->addChild(new QTreeWidgetItem());
			pItem->addChild(new QTreeWidgetItem());
			pItem->addChild(new QTreeWidgetItem());
		}

		CTokenView__UpdateCapabilityValue(pItem->child(0), tr("Full name"), I.value().FullName);
		CTokenView__UpdateCapabilityValue(pItem->child(1), tr("Capability"), I.value().Capability);
		CTokenView__UpdateCapabilityValue(pItem->child(2), tr("Package"), I.value().Package);
		CTokenView__UpdateCapabilityValue(pItem->child(3), tr("Guid"), I.value().Guid);
	}

	foreach(QTreeWidgetItem* pItem, OldCapabilities)
		delete pItem;
}


void CTokenView::UpdateClaims()
{
	QMap<QString, CWinToken::SAttribute> UserClaims = m_pCurToken->GetClaims(false);
	UpdateAttributes(UserClaims, m_pUserClaims);

	QMap<QString, CWinToken::SAttribute> DeviceClaims = m_pCurToken->GetClaims(true);
	UpdateAttributes(DeviceClaims, m_pDeviceClaims);
}

void CTokenView::UpdateAttributes(QMap<QString, CWinToken::SAttribute> Attributes, QTreeWidgetItem* pRoot)
{
	QMap<QString, QTreeWidgetItem*> OldAttributes;
	for(int i = 0; i < (pRoot ? pRoot->childCount() : m_pAttributes->GetTree()->topLevelItemCount()); ++i) 
	{
		QTreeWidgetItem* pItem = pRoot ? pRoot->child(i) : m_pAttributes->GetTree()->topLevelItem(i);
		QString Name = pItem->data(0, Qt::UserRole).toString();
		Q_ASSERT(!OldAttributes.contains(Name));
		OldAttributes.insert(Name,pItem);
	}

	for(QMap<QString, CWinToken::SAttribute>::iterator I = Attributes.begin(); I != Attributes.end(); ++I)
	{
		QTreeWidgetItem* pItem = OldAttributes.take(I.key());
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, I.key());
			pItem->setText(0, I.key());
			if (pRoot)
				pRoot->addChild(pItem);
			else
				m_pAttributes->GetTree()->addTopLevelItem(pItem);

			pItem->addChild(new QTreeWidgetItem());
			pItem->addChild(new QTreeWidgetItem());
		}

		pItem->child(0)->setText(0, tr("Type: %1").arg(CWinToken::GetSecurityAttributeTypeString(I.value().Type)));
		pItem->child(1)->setText(0, tr("Flags: %1 (0x%2)").arg(CWinToken::GetSecurityAttributeFlagsString(I.value().Flags)).arg(I.value().Flags, 0, 16));

		QMap<int, QTreeWidgetItem*> OldValues;
		for(int i = 2; i < pItem->childCount(); ++i)
		{
			QTreeWidgetItem* pSubItem = pItem->child(i);
			int Index = pSubItem->data(0, Qt::UserRole).toInt();
			Q_ASSERT(!OldValues.contains(Index));
			OldValues.insert(Index,pSubItem);
		}	

		for(int i = 0; i < I.value().Values.size(); i++)
		{
			QTreeWidgetItem* pSubItem = OldValues.take(i);
			if(!pSubItem)
			{
				pSubItem = new QTreeWidgetItem();
				pSubItem->setData(0, Qt::UserRole, i);
				pItem->addChild(pSubItem);
			}

			pSubItem->setText(0, tr("Value %1: %2").arg(i).arg(I.value().Values[i].toString()));
		}

		foreach(QTreeWidgetItem* pItem, OldValues)
			delete pItem;
	}

	foreach(QTreeWidgetItem* pItem, OldAttributes)
		delete pItem;
}

void CTokenView::UpdateAttributes()
{
	QMap<QString, CWinToken::SAttribute> Attributes = m_pCurToken->GetAttributes();
	UpdateAttributes(Attributes);
}

void CTokenView::OnMenu(const QPoint &point)
{
	if (!m_pCurToken)
		return;

	QMap<QString, CWinToken::SPrivilege> Privileges = m_pCurToken->GetPrivileges();
	QMap<QByteArray, CWinToken::SGroup> Groups = m_pCurToken->GetGroups();

	int SelectedPrivileges = 0;
	int SelectedGroups = 0;
	int SelectedOthers = 0;

	int ItemsEnabled = 0;
	int ItemsDisabled = 0;
	int ItemsModified = 0;

	foreach(QTreeWidgetItem* pItem, m_pTokenList->selectedItems())
	{
		if (pItem->parent() == m_pPrivileges)
		{
			QString Name = pItem->data(0, Qt::UserRole).toString();
			quint32 Attributes = Privileges[Name].Attributes;
			if (CWinToken::IsPrivilegeModified(Attributes))
				ItemsModified++;
			if (CWinToken::IsPrivilegeEnabled(Attributes))
				ItemsEnabled++;
			else
				ItemsDisabled++;
			SelectedPrivileges++;
		}
		else if (pItem->parent() == m_pGroups)
		{
			QByteArray Sid = pItem->data(0, Qt::UserRole).toByteArray();
			quint32 Attributes = Groups[Sid].Attributes;
			if (CWinToken::IsGroupModified(Attributes))
				ItemsModified++;
			if (CWinToken::IsGroupEnabled(Attributes))
				ItemsEnabled++;
			else
				ItemsDisabled++;
			SelectedGroups++;
		}
		else
			SelectedOthers++;
    }

	bool Check = SelectedOthers == 0 && ((SelectedPrivileges != 0) ^ (SelectedGroups != 0));

	m_pEnable->setEnabled(Check && ItemsDisabled > 0);
	m_pDisable->setEnabled(Check && ItemsEnabled > 0);
	m_pReset->setEnabled(Check && ItemsModified > 0);
	//m_pAdd;
	m_pRemove->setEnabled(Check && SelectedPrivileges > 0);

	CPanelView::OnMenu(point);
}

void CTokenView::OnTokenAction()
{
	QMap<QString, CWinToken::SPrivilege> Privileges = m_pCurToken->GetPrivileges();
	QMap<QByteArray, CWinToken::SGroup> Groups = m_pCurToken->GetGroups();

	QList<STATUS> Errors;
	int Force = -1;
	foreach(QTreeWidgetItem* pItem, m_pTokenList->selectedItems())
	{
		STATUS Status = OK;
retry:
		if (pItem->parent() == m_pPrivileges)
		{
			QString Name = pItem->data(0, Qt::UserRole).toString();
			CWinToken::SPrivilege& Privilege = Privileges[Name];
			
			if (sender() == m_pEnable)
				Status = m_pCurToken->PrivilegeAction(Privilege, CWinToken::eEnable);
			else if (sender() == m_pDisable)
				Status = m_pCurToken->PrivilegeAction(Privilege, CWinToken::eDisable);
			else if (sender() == m_pReset)
				Status = m_pCurToken->PrivilegeAction(Privilege, CWinToken::eReset);
			else if (sender() == m_pRemove)
				Status = m_pCurToken->PrivilegeAction(Privilege, CWinToken::eRemove, Force == 1);
		}
		else if (pItem->parent() == m_pGroups)
		{
			QByteArray Sid = pItem->data(0, Qt::UserRole).toByteArray();
			CWinToken::SGroup& Group = Groups[Sid];

			if (sender() == m_pEnable)
				Status = m_pCurToken->GroupAction(Group, CWinToken::eEnable);
			else if (sender() == m_pDisable)
				Status = m_pCurToken->GroupAction(Group, CWinToken::eDisable);
			else if (sender() == m_pReset)
				Status = m_pCurToken->GroupAction(Group, CWinToken::eReset);
		}

		if (Status.IsError())
		{
			if (Status.GetStatus() == ERROR_CONFIRM)
			{
				if (Force == -1)
				{
					switch (QMessageBox("TaskExplorer", Status.GetText(), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel | QMessageBox::Default | QMessageBox::Escape).exec())
					{
					case QMessageBox::Yes:
						Force = 1;
						goto retry;
						break;
					case QMessageBox::No:
						Force = 0;
						break;
					case QMessageBox::Cancel:
						return;
					}
				}
			}
			else
				Errors.append(Status);
		}
	}

	CTaskExplorer::CheckErrors(Errors);
}

void CTokenView::OnDefaultToken()
{
	if (m_pCurToken)
		m_pCurToken->OpenPermissions(true);
}

void CTokenView::OnPermissions()
{
	if (m_pCurToken)
		m_pCurToken->OpenPermissions();
}

void CTokenView::OnChangeIntegrity()
{
	quint32 IntegrityLevel = m_pIntegrity->currentData().toUInt();
	if (IntegrityLevel == m_pCurToken->GetIntegrityLevel())
		return;

	if (QMessageBox("TaskExplorer", tr("Once lowered, the integrity level of the token cannot be raised again."), QMessageBox::Question, QMessageBox::Apply, QMessageBox::Cancel | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Apply)
		return;

	STATUS status = m_pCurToken->SetIntegrityLevel(IntegrityLevel);

	if(status.IsError())
		QMessageBox::warning(this, "TaskExplorer", tr("Unable to set the integrity level, error: %1").arg(status.GetText()));
}

void CTokenView::OnLinkedToken()
{
	if (!m_pCurToken)
		return;

	CWinTokenPtr pLinkedToken = m_pCurToken->GetLinkedToken();
	if (pLinkedToken)
	{
		CTokenView* pTokenView = new CTokenView();
		CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(pTokenView, tr("Token"));
		pTokenView->ShowToken(pLinkedToken);
		pTaskInfoWindow->show();
	}
}