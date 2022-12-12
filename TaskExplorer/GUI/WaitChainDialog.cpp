/*
 * Task Explorer -
 *   qt port of Wait Chain Traversal (WCT) Plugin
 *
 * Copyright (C) 2013-2015 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 * 
 */

#include "stdafx.h"
#include "WaitChainDialog.h"
#include "../../MiscHelpers/Common/Settings.h"
#include "../API/Windows/ProcessHacker.h"
#include "../API/Windows/WindowsAPI.h"

#include <wct.h>
// Wait Chain Traversal Documentation:
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms681622.aspx

#define WCT_GETINFO_ALL_FLAGS (WCT_OUT_OF_PROC_FLAG|WCT_OUT_OF_PROC_COM_FLAG|WCT_OUT_OF_PROC_CS_FLAG|WCT_NETWORK_IO_FLAG)

struct SWaitChainTraversal
{
	SWaitChainTraversal()
	{
		ProcessId = NULL;
		QueryHandle = NULL;
		ThreadId = NULL;

		ThreadNodes = NULL;
		ThreadNodeCount = 0;
	}

	HANDLE ProcessId;
	HANDLE QueryHandle;
	HANDLE ThreadId;

    HWCT WctSessionHandle;
    HMODULE Ole32ModuleHandle;

	struct SThreadNodes
	{
		SThreadNodes()
		{
			ThreadId = NULL;
			BOOL isDeadLocked = FALSE;
			nodeInfoLength = WCT_MAX_NODE_COUNT;
			memset(nodeInfoArray, 0, sizeof(nodeInfoArray));
		}

		quint64 ThreadId;
		BOOL isDeadLocked;
		ULONG nodeInfoLength;
		WAITCHAIN_NODE_INFO nodeInfoArray[WCT_MAX_NODE_COUNT];
	};

	SThreadNodes*	ThreadNodes;
	volatile int	ThreadNodeCount;
};

BOOLEAN WaitChainRegisterCallbacks(_Inout_ SWaitChainTraversal* Context)
{
    PCOGETCALLSTATE coGetCallStateCallback = NULL;
    PCOGETACTIVATIONSTATE coGetActivationStateCallback = NULL;

    if (!(Context->Ole32ModuleHandle = LoadLibrary(L"ole32.dll")))
        return FALSE;

    if (!(coGetCallStateCallback = (PCOGETCALLSTATE)PhGetProcedureAddress(Context->Ole32ModuleHandle, "CoGetCallState", 0)))
        return FALSE;

    if (!(coGetActivationStateCallback = (PCOGETACTIVATIONSTATE)PhGetProcedureAddress(Context->Ole32ModuleHandle, "CoGetActivationState", 0)))
        return FALSE;

    RegisterWaitChainCOMCallback(coGetCallStateCallback, coGetActivationStateCallback);
    return TRUE;
}

/*class CWaitChainItem : public QTreeWidgetItem
{
public:
	CWaitChainItem(QTreeWidget* parent = NULL) : QTreeWidgetItem(parent) {}

private:
	virtual bool operator< (const QTreeWidgetItem &other) const
	{
		int column = treeWidget()->sortColumn();
		return data(column, Qt::UserRole).toUInt() < other.data(column, Qt::UserRole).toUInt();
	}
};*/

CWaitChainDialog::CWaitChainDialog(const CProcessPtr& pProcess, QWidget *parent)
	: QMainWindow(parent)
{
	this->setWindowTitle("Wait Chain Traversal");

	m_pMainWidget = new QWidget();
    m_pMainWidget->setMinimumSize(QSize(430, 210));

    m_pMainLayout = new QGridLayout();
	m_pMainWidget->setLayout(m_pMainLayout);

	m_pWaitTree = new CPanelWidgetEx();
	m_pWaitTree->GetTree()->setHeaderLabels(tr("Type|ThreadId|ProcessId|Status|Context Switches|WaitTime|Timeout|Alertable|Name").split("|"));
	m_pWaitTree->GetTree()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pWaitTree->GetTree()->setSortingEnabled(true);
    m_pMainLayout->addWidget(m_pWaitTree, 0, 0, 1, 4);

    m_pButtonBox = new QDialogButtonBox();
    m_pButtonBox->setStandardButtons(QDialogButtonBox::Close);
    m_pMainLayout->addWidget(m_pButtonBox, 1, 0, 1, 4);

	this->setCentralWidget(m_pMainWidget);

	//connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));

	restoreGeometry(theConf->GetBlob("WaitChainWindow/Window_Geometry"));
	m_pWaitTree->GetTree()->header()->restoreState(theConf->GetBlob("WaitChainWindow/WaitTree_Columns"));

	m_TimerId = -1;

	m = new SWaitChainTraversal();
	m->ProcessId = (HANDLE)pProcess->GetProcessId();
	m->QueryHandle = ((CWinProcess*)pProcess.data())->GetQueryHandle();

	STATUS status = InitWCT();
	if (status.IsError())
	{
		QMessageBox::critical(this, tr("Wait Chain Traversal"), status.GetText());
		m_TimerId = -1;
		m_pWorker = NULL;
	}
	else
	{
		m_pWorker = new QThread();
		m_pWorker->moveToThread(m_pWorker);
		m_pWorker->start();

		m_TimerId = startTimer(1000);
		Refresh();
	}
}

CWaitChainDialog::~CWaitChainDialog()
{
	theConf->SetBlob("WaitChainWindow/Window_Geometry", saveGeometry());
	theConf->SetBlob("WaitChainWindow/WaitTree_Columns", m_pWaitTree->GetTree()->header()->saveState());

	if (m_TimerId != -1)
	{
		killTimer(m_TimerId);
		UnInitWCT();
	}

	m->ThreadNodeCount = 0;
	m_pWorker->quit();
	if (!m_pWorker->wait(5 * 1000))
		m_pWorker->terminate();
	delete m_pWorker;

	delete [] m->ThreadNodes;
	delete m;
}

void CWaitChainDialog::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

/*void CWaitChainDialog::accept()
{

}*/

void CWaitChainDialog::reject()
{
	this->close();
}

void CWaitChainDialog::timerEvent(QTimerEvent *e)
{
	if (e->timerId() != m_TimerId) 
	{
		QMainWindow::timerEvent(e);
		return;
	}

	Refresh();
}

STATUS CWaitChainDialog::InitWCT()
{
    if (!WaitChainRegisterCallbacks(m))
        return ERR(tr("Failed to WaitChainRegisterCallbacks"), NTSTATUS_FROM_WIN32(GetLastError()));

    // Synchronous WCT session
    if (!(m->WctSessionHandle = OpenThreadWaitChainSession(0, NULL)))
        return ERR(tr("Failed to OpenThreadWaitChainSession"), NTSTATUS_FROM_WIN32(GetLastError()));

    return OK;
}

void CWaitChainDialog::UnInitWCT()
{
    if (m->WctSessionHandle)
        CloseThreadWaitChainSession(m->WctSessionHandle);

    if (m->Ole32ModuleHandle)
        FreeLibrary(m->Ole32ModuleHandle);
}

/*void CWaitChainDialog::UpdateThread(void* ThreadId, QMap<quint64, QTreeWidgetItem*>& OldNodes)
{
    BOOL isDeadLocked = FALSE;
    ULONG nodeInfoLength = WCT_MAX_NODE_COUNT;
    WAITCHAIN_NODE_INFO nodeInfoArray[WCT_MAX_NODE_COUNT];

    memset(nodeInfoArray, 0, sizeof(nodeInfoArray));

    // Retrieve the thread wait chain.
    if (!GetThreadWaitChain(m->WctSessionHandle, 0, WCT_GETINFO_ALL_FLAGS, HandleToUlong(ThreadId), &nodeInfoLength, nodeInfoArray, &isDeadLocked))
        return;

    // Check if the wait chain is too big for the array we passed in.
    if (nodeInfoLength > WCT_MAX_NODE_COUNT)
        nodeInfoLength = WCT_MAX_NODE_COUNT;
	*/

void CWaitChainDialog::UpdateThread(ulong nodeInfoLength, struct _WAITCHAIN_NODE_INFO* nodeInfoArray, bool isDeadLocked, QMap<quint64, QTreeWidgetItem*>& OldNodes)
{
	QTreeWidgetItem* pRootNode = NULL;
	int ChildNodeCounter = 0;

	auto CleanUpChildNodes = [&pRootNode, &ChildNodeCounter]() {
		if (pRootNode)
		{
			for (; ChildNodeCounter < pRootNode->childCount(); )
				delete pRootNode->child(ChildNodeCounter);
			ChildNodeCounter = 0;
		}
	};

    for (ULONG i = 0; i < nodeInfoLength; i++)
    {
        PWAITCHAIN_NODE_INFO wctNode = &nodeInfoArray[i];
		QTreeWidgetItem* pNode = NULL;

        if (wctNode->ObjectType == WctThreadType || pRootNode == NULL)
        {
			CleanUpChildNodes();

			pRootNode = OldNodes.take(wctNode->ThreadObject.ThreadId);
			if (pRootNode == NULL)
			{
				pRootNode = new QTreeWidgetItem();
				m_pWaitTree->GetTree()->addTopLevelItem(pRootNode);
				pRootNode->setExpanded(true);
				m_pThreadNodes.insert(wctNode->ThreadObject.ThreadId, pRootNode);
			}
			
			pNode = pRootNode;
        }
        else
        {
			if (pRootNode->childCount() > ChildNodeCounter)
				pNode = pRootNode->child(ChildNodeCounter);
			else
			{
				pNode = new QTreeWidgetItem();
				pRootNode->addChild(pNode);
			}
			ChildNodeCounter++;
        }

		bool bRed = isDeadLocked || pNode != pRootNode;
		for(int j=0; j < m_pWaitTree->GetTree()->columnCount(); j++)
			pNode->setForeground(j, bRed ? Qt::red : Qt::black);

		pNode->setText(eProcessId, QString::number(wctNode->ThreadObject.ProcessId));
		pNode->setText(eThreadId, QString::number(wctNode->ThreadObject.ThreadId));
		pNode->setText(eType, GetWCTObjType(wctNode->ObjectType));
		pNode->setText(eStatus, GetWCTObjStatus(wctNode->ObjectStatus));
		pNode->setText(eAlertable, wctNode->LockObject.Alertable ? tr("True") : tr("False"));
		pNode->setText(eWaitTime, FormatTime(wctNode->ThreadObject.WaitTime, true));
		pNode->setText(eContextSwitches, FormatNumber(wctNode->ThreadObject.ContextSwitches));

		if (wctNode->LockObject.ObjectName[0] != '\0')
		{
			// -- ProcessID --
			//wctNode->LockObject.ObjectName[0]
			// -- ThreadID --
			//wctNode->LockObject.ObjectName[2]
			// -- Unknown --
			//wctNode->LockObject.ObjectName[4]
			// -- ContextSwitches --
			//wctNode->LockObject.ObjectName[6]

			if (PhIsDigitCharacter(wctNode->LockObject.ObjectName[0]))
			{
				pNode->setText(eName, QString::fromWCharArray(wctNode->LockObject.ObjectName));
			}
			//else
			//{
			//    rootNode->ObjectNameString = PhFormatString(L"[%lu, %lu]",
			//        wctNode.LockObject.ObjectName[0],
			//        wctNode.LockObject.ObjectName[2]
			//        );
			//}
		}

		if (wctNode->LockObject.Timeout.QuadPart > 0)
			pNode->setText(eTimeout, QDateTime::fromSecsSinceEpoch(FILETIME2time(wctNode->LockObject.Timeout.QuadPart)).toString("dd.MM.yyyy hh:mm:ss"));
    }

	CleanUpChildNodes();
}


bool CWaitChainDialog::Refresh()
{
	if (m->ThreadNodes != NULL)
		return false; // previouse update did not finisched

	QList<quint64> ThreadIds;
	if (m->ThreadId != 0)
	{
		ThreadIds.append((quint64)m->ThreadId);
		//UpdateThread(m->ThreadId, OldNodes);
	}
	else
    {
		HANDLE threadHandle;
        NTSTATUS status = NtGetNextThread(m->QueryHandle, NULL, THREAD_QUERY_LIMITED_INFORMATION, 0, 0, &threadHandle);

        while (NT_SUCCESS(status))
        {
			THREAD_BASIC_INFORMATION basicInfo;
			if (NT_SUCCESS(PhGetThreadBasicInformation(threadHandle, &basicInfo)))
			{
				ThreadIds.append((quint64)basicInfo.ClientId.UniqueThread);
				//UpdateThread(basicInfo.ClientId.UniqueThread, OldNodes);
			}

			HANDLE newThreadHandle;
            status = NtGetNextThread(m->QueryHandle, threadHandle, THREAD_QUERY_LIMITED_INFORMATION, 0, 0, &newThreadHandle);

            NtClose(threadHandle);

            threadHandle = newThreadHandle;
        }
    }
    
	m->ThreadNodeCount = ThreadIds.size();
	m->ThreadNodes = new SWaitChainTraversal::SThreadNodes[m->ThreadNodeCount];
	for(int i=0; i < m->ThreadNodeCount; i++)
		m->ThreadNodes[i].ThreadId = ThreadIds.at(i);

	SWaitChainTraversal* M = m;
	CWaitChainDialog* That = this;
	QTimer::singleShot(0, m_pWorker, [That, M]() {

		for (int i = 0; i < M->ThreadNodeCount; i++)
		{
			SWaitChainTraversal::SThreadNodes* N = &M->ThreadNodes[i];

			// Retrieve the thread wait chain.
			if (!GetThreadWaitChain(M->WctSessionHandle, 0, WCT_GETINFO_ALL_FLAGS, N->ThreadId, &N->nodeInfoLength, N->nodeInfoArray, &N->isDeadLocked))
			{
				N->nodeInfoLength = 0;
				continue;
			}

			// Check if the wait chain is too big for the array we passed in.
			if (N->nodeInfoLength > WCT_MAX_NODE_COUNT)
				N->nodeInfoLength = WCT_MAX_NODE_COUNT;
		}

		QMetaObject::invokeMethod(That, "UpdateThreads", Qt::QueuedConnection);
	});

	return true;
}

void CWaitChainDialog::UpdateThreads()
{
	QMap<quint64, QTreeWidgetItem*>	OldNodes = m_pThreadNodes;

	for (int i = 0; i < m->ThreadNodeCount; i++)
	{
		SWaitChainTraversal::SThreadNodes* N = &m->ThreadNodes[i];
		UpdateThread(N->nodeInfoLength, N->nodeInfoArray, N->isDeadLocked, OldNodes);
	}

	foreach(quint64 ThreadId, OldNodes.keys())
		delete m_pThreadNodes.take(ThreadId);

	m->ThreadNodeCount = 0;
	delete[] m->ThreadNodes;
	m->ThreadNodes = NULL;
}

QString CWaitChainDialog::GetWCTObjType(int ObjectType)
{
    switch (ObjectType)
    {
    case WctCriticalSectionType:	return tr("CriticalSection");
    case WctSendMessageType:		return tr("SendMessage");
    case WctMutexType:				return tr("Mutex");
    case WctAlpcType:				return tr("Alpc");
    case WctComType:				return tr("Com");
    case WctComActivationType:		return tr("ComActivation");
	case WctProcessWaitType:		return tr("ProcWait");
    case WctThreadType:				return tr("Thread");
    case WctThreadWaitType:			return tr("ThreadWait");
    case WctSocketIoType:			return tr("Socket I/O");
    case WctSmbIoType:				return tr("SMB I/O");
    case WctUnknownType:
    case WctMaxType:
    default:						return tr("Unknown %1").arg(ObjectType);
    }
}

QString CWaitChainDialog::GetWCTObjStatus(int ObjectStatus)
{
    switch (ObjectStatus)
    {
    case WctStatusNoAccess:		return tr("No Access");
    case WctStatusRunning:		return tr("Running");
    case WctStatusBlocked:		return tr("Blocked");
    case WctStatusPidOnly:		return tr("Pid Only");
    case WctStatusPidOnlyRpcss:	return tr("Pid Only (Rpcss)");
    case WctStatusOwned:		return tr("Owned");
    case WctStatusNotOwned:		return tr("Not Owned");
    case WctStatusAbandoned:	return tr("Abandoned");
    case WctStatusError:		return tr("Error");
    case WctStatusUnknown:
    case WctStatusMax:
    default:					return tr("Unknown %1").arg(ObjectStatus);
    }
}