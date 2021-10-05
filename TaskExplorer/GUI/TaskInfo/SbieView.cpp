#include "stdafx.h"
#include "../TaskExplorer.h"
#include "SbieView.h"
#include "../../../MiscHelpers/Common/KeyValueInputDialog.h"
#include "../../../MiscHelpers/Common/Finder.h"
#include "../../API/Windows/WinProcess.h"
#include "../../API/Windows/ProcessHacker.h"
#include "../../API/Windows/SandboxieAPI.h"
#include "../../API/Windows/WindowsAPI.h"


CSbieView::CSbieView(QWidget *parent)
	:QWidget(parent)
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

	QHBoxLayout* hBoxLayout = new QHBoxLayout();
	m_pGeneralLayout->addLayout(hBoxLayout, row, 1, 1, 5);

	QLabel* pBoxName = new QLabel(tr("Box Name:"));
	pBoxName->setSizePolicy(QSizePolicy::Maximum, pBoxName->sizePolicy().verticalPolicy());
	m_pGeneralLayout->addWidget(pBoxName, row, 0, 1, 1);
	m_pBoxName = new QLineEdit();
	m_pBoxName->setReadOnly(true);
	//m_pBoxName ->setSizePolicy(QSizePolicy::Expanding, m_pBoxName->sizePolicy().verticalPolicy());
	hBoxLayout->addWidget(m_pBoxName); // , row, 1, 1, 2);

	hBoxLayout->addWidget(new QLabel(tr("User SID:"))); // , row, 3, 1, 1);
	m_pUserSID = new QLineEdit();
	m_pUserSID->setReadOnly(true);
	m_pUserSID->setMinimumWidth(280);
	//m_pUserSID ->setSizePolicy(QSizePolicy::Expanding, m_pUserSID->sizePolicy().verticalPolicy());
	hBoxLayout->addWidget(m_pUserSID); // ,row , 4, 1, 2);
	row++;

	QHBoxLayout* hBoxLayout2 = new QHBoxLayout();
	m_pGeneralLayout->addLayout(hBoxLayout2, row, 1, 1, 5);

	QLabel* pImageName = new QLabel(tr("Image Name:"));
	pImageName->setSizePolicy(QSizePolicy::Maximum, pImageName->sizePolicy().verticalPolicy());
	m_pGeneralLayout->addWidget(pImageName, row, 0, 1, 1);
	m_pImageName = new QLineEdit();
	m_pImageName->setReadOnly(true);
	//m_pImageName ->setSizePolicy(QSizePolicy::Expanding, m_pImageName->sizePolicy().verticalPolicy());
	hBoxLayout2->addWidget(m_pImageName); // , row, 1, 1, 2);

	hBoxLayout2->addWidget(new QLabel(tr("Image Type:"))); // , row, 3, 1, 1);
	m_pImageType = new QLineEdit();
	m_pImageType->setReadOnly(true);
	m_pImageType->setMinimumWidth(280);
	//m_pImageType ->setSizePolicy(QSizePolicy::Expanding, m_pImageType->sizePolicy().verticalPolicy());
	hBoxLayout2->addWidget(m_pImageType); // ,row , 4, 1, 2);
	row++;


	m_pGeneralLayout->addWidget(new QLabel(tr("File Root:")), row, 0, 1, 1);
	m_pFileRoot = new QLineEdit();
	m_pFileRoot->setReadOnly(true);
	m_pGeneralLayout->addWidget(m_pFileRoot, row, 1, 1, 5);
	row++;

	m_pGeneralLayout->addWidget(new QLabel(tr("Key Root:")), row, 0, 1, 1);
	m_pKeyRoot = new QLineEdit();
	m_pKeyRoot->setReadOnly(true);
	m_pGeneralLayout->addWidget(m_pKeyRoot, row, 1, 1, 5);
	row++;

	m_pGeneralLayout->addWidget(new QLabel(tr("Ipc Root:")), row, 0, 1, 1);
	m_pIpcRoot = new QLineEdit();
	m_pIpcRoot->setReadOnly(true);
	m_pGeneralLayout->addWidget(m_pIpcRoot, row, 1, 1, 5);
	row++;

	m_pPaths = new QTabWidget();
	m_pGeneralLayout->addWidget(m_pPaths, row, 0, 1, 6);
	row++;

	m_pFile = new CPanelWidgetEx();
	m_pFile->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pFile->GetView())->setHeaderLabels(tr("Directive|Path").split("|"));
	m_pFile->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_iFile = m_pPaths->addTab(m_pFile, tr("Files"));

	m_pKey = new CPanelWidgetEx();
	m_pKey->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pKey->GetView())->setHeaderLabels(tr("Directive|Path").split("|"));
	m_pKey->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_iKey = m_pPaths->addTab(m_pKey, tr("Keys"));

	m_pIpc = new CPanelWidgetEx();
	m_pIpc->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pIpc->GetView())->setHeaderLabels(tr("Directive|Path").split("|"));
	m_pIpc->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_iIpc = m_pPaths->addTab(m_pIpc, tr("Ipc"));

	m_pWnd = new CPanelWidgetEx();
	m_pWnd->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pWnd->GetView())->setHeaderLabels(tr("Directive|Path").split("|"));
	m_pWnd->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_iWnd = m_pPaths->addTab(m_pWnd, tr("WinClass"));

	m_pConf = new CPanelWidgetEx();
	m_pConf->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pConf->GetView())->setHeaderLabels(tr("Name|Value").split("|"));
	m_pConf->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_iConf = m_pPaths->addTab(m_pConf, tr("Config"));
}

CSbieView::~CSbieView()
{

}

void CSbieView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	CProcessPtr pProcess;
	if (Processes.count() > 1)
	{
		setEnabled(false);
	}
	else if(!Processes.isEmpty())
	{
		setEnabled(true);
		pProcess = Processes.first();
	}

	m_pCurProcess = pProcess.staticCast<CWinProcess>();


	CSandboxieAPI* pSandboxieAPI = ((CWindowsAPI*)theAPI)->GetSandboxieAPI();

	if (!pSandboxieAPI || !m_pCurProcess) {
		setEnabled(false);
		return;
	}
	
	QString BoxName;
	QString ImageName;
	QString SID;
	if (!pSandboxieAPI->QueryProcess(m_pCurProcess->GetProcessId(), BoxName, ImageName, SID)) {
		m_pFile->GetTree()->clear();
		m_pKey->GetTree()->clear();
		m_pIpc->GetTree()->clear();
		m_pWnd->GetTree()->clear();
		m_pConf->GetTree()->clear();
		setEnabled(false);
		return;
	}

	m_pBoxName->setText(BoxName);
	m_pUserSID->setText(SID);

	quint32 ImageType;
	quint32 Flags = pSandboxieAPI->QueryProcessInfoEx(m_pCurProcess->GetProcessId(), NULL, &ImageType);
	m_pImageName->setText(ImageName);
	m_pImageType->setText(CSandboxieAPI::ImageTypeToStr(ImageType) + "; " + CSandboxieAPI::ImageFlagsToStr(Flags).join(", "));

	QString FilePath, KeyPath, IpcPath;
	pSandboxieAPI->GetProcessPaths(m_pCurProcess->GetProcessId(), FilePath, KeyPath, IpcPath);
	
	m_pFileRoot->setText(FilePath);
	m_pKeyRoot->setText(KeyPath);
	m_pIpcRoot->setText(IpcPath);

	auto PrepPaths = [&](QMap<QString, QTreeWidgetItem*>& OldPaths, CPanelWidgetEx* pTree) {
		for (int i = 0; i < pTree->GetTree()->topLevelItemCount(); ++i)
		{
			QTreeWidgetItem* pItem = pTree->GetTree()->topLevelItem(i);
			QString Name = pItem->data(0, Qt::UserRole).toString();
			Q_ASSERT(!OldPaths.contains(Name));
			OldPaths.insert(Name, pItem);
		}
	};

	auto UpdatePaths = [&](quint32 path_code, QMap<QString, QTreeWidgetItem*> & OldPaths, CPanelWidgetEx* pTree) {
		QStringList Paths;
		pSandboxieAPI->QueryPathList(m_pCurProcess->GetProcessId(), path_code, Paths);

		foreach(const QString& Path, Paths)
		{
			QColor Color;
			QString Directive = tr("Other");
			switch (path_code & 0xFF)
			{
			case 'n': Color = QColor(255, 255, 255); Directive = tr("Normal"); break;
			case 'o': Color = QColor(224, 240, 224); Directive = tr("Open"); break;
			case 'c': Color = QColor(240, 224, 224); Directive = tr("Closed"); break;
			case 'r': Color = QColor(240, 240, 224); Directive = tr("Read"); break;
			case 'w': Color = QColor(224, 240, 240); Directive = tr("Write"); break;
			}
			QString ID = Directive + "=" + Path;

			QTreeWidgetItem* pItem = OldPaths.take(ID);
			if(!pItem)
			{
				pItem = new QTreeWidgetItem();
				pItem->setData(0, Qt::UserRole, ID);
				pItem->setText(0, Directive);
				pItem->setText(1, Path);
				if (Color.isValid())
				{
					for (int i = 0; i < pItem->columnCount(); i++)
						pItem->setBackgroundColor(i, Color);
				}
				pTree->GetTree()->addTopLevelItem(pItem);
			}
		}
	};

	auto FinishPaths = [&](QMap<QString, QTreeWidgetItem*>& OldPaths, CPanelWidgetEx* pTree, int Index, const QString& Name) {
		foreach(QTreeWidgetItem* pItem, OldPaths)
			delete pItem;
		OldPaths.clear();
		m_pPaths->setTabText(Index, QString("%1 (%2)").arg(Name).arg(pTree->GetTree()->topLevelItemCount()));
	};

	QMap<QString, QTreeWidgetItem*> OldPaths;

	//| prefix means dont append * at the end
	//starting with *: or ?: means match for all driver letters

	PrepPaths(OldPaths, m_pFile);
	UpdatePaths('fn', OldPaths, m_pFile);
	UpdatePaths('fo', OldPaths, m_pFile); // includes OpenPipePath
	UpdatePaths('fc', OldPaths, m_pFile);
	UpdatePaths('fr', OldPaths, m_pFile);
	UpdatePaths('fw', OldPaths, m_pFile);
	FinishPaths(OldPaths, m_pFile, m_iFile, tr("Files"));

	PrepPaths(OldPaths, m_pKey);
	UpdatePaths('kn', OldPaths, m_pKey);
	UpdatePaths('ko', OldPaths, m_pKey);
	UpdatePaths('kc', OldPaths, m_pKey);
	UpdatePaths('kr', OldPaths, m_pKey);
	UpdatePaths('kw', OldPaths, m_pKey);
	FinishPaths(OldPaths, m_pKey, m_iKey, tr("Keys"));

	PrepPaths(OldPaths, m_pIpc);
	UpdatePaths('io', OldPaths, m_pIpc);
	UpdatePaths('ic', OldPaths, m_pIpc);
	FinishPaths(OldPaths, m_pIpc, m_iIpc, tr("Ipc"));
	//OpenIpcPath=$:program.exe
	//Permits a program running inside the sandbox to have full access into the address space of a 
	//target process running outside the sandbox. The process name of the target process must match the name specified 
	//in the setting. When this setting is not specified, Sandboxie allows only read-access by a sandboxed process into a 
	//process outside the sandbox. This form of the OpenIpcPath setting does not support wildcards.

	PrepPaths(OldPaths, m_pWnd);
	UpdatePaths('wo', OldPaths, m_pWnd);
	FinishPaths(OldPaths, m_pWnd, m_iWnd, tr("WinClass"));
	//OpenWinClass=$:program.exe
	//Permits a program running inside the sandbox to use the PostThreadMessage API to send a message directly 
	//to a thread in a target process running outside the sandbox. This form of the OpenWinClass setting does 
	//not support wildcards, so the process name of the target process must match the name specified in the setting.
	
	auto UpdateConfig = [&](QMap<QString, QTreeWidgetItem*> & OldPaths, CPanelWidgetEx* pTree) {
		QList<QPair<QString, QString>> Config = pSandboxieAPI->GetIniSection(BoxName);

		for(auto I = Config.begin(); I != Config.end(); ++I)
		{
			QString ID = I->first + "=" + I->second;

			QTreeWidgetItem* pItem = OldPaths.take(ID);
			if(!pItem)
			{
				pItem = new QTreeWidgetItem();
				pItem->setData(0, Qt::UserRole, ID);
				pItem->setText(0, I->first);
				pItem->setText(1, I->second);
				pTree->GetTree()->addTopLevelItem(pItem);
			}
		}
	};
	
	PrepPaths(OldPaths, m_pConf);
	UpdateConfig(OldPaths, m_pConf);
	FinishPaths(OldPaths, m_pConf, m_iConf, tr("Config"));

	//Refresh();
}

void CSbieView::Refresh()
{

}

