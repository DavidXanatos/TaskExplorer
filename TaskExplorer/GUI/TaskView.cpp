#include "stdafx.h"
#include "TaskView.h"
#ifdef WIN32
#include "../API/Windows/WinProcess.h"
#include "../API/Windows/WinThread.h"
#endif
#include "TaskExplorer.h"
#include "AffinityDialog.h"

CTaskView::CTaskView(QWidget *parent) : CPanelView(parent)
{
}

CTaskView::~CTaskView()
{
}

void CTaskView::AddTaskItemsToMenu()
{
	m_pTerminate = m_pMenu->addAction(tr("Terminate"), this, SLOT(OnTaskAction()));
	m_pSuspend = m_pMenu->addAction(tr("Suspend"), this, SLOT(OnTaskAction()));
	m_pResume = m_pMenu->addAction(tr("Resume"), this, SLOT(OnTaskAction()));
}

#define INVALID_PRIORITY INT_MAX
#define UNDEFINED_PRIORITY INT_MIN

#ifdef WIN32
#define THREAD_BASE_PRIORITY_LOWRT  15  // value that gets a thread to LowRealtime-1
#define THREAD_BASE_PRIORITY_MAX    2   // maximum thread base priority boost
#define THREAD_BASE_PRIORITY_MIN    (-2)  // minimum thread base priority boost
#define THREAD_BASE_PRIORITY_IDLE   (-15) // value that gets a thread to idle

#define THREAD_PRIORITY_LOWEST          THREAD_BASE_PRIORITY_MIN
#define THREAD_PRIORITY_BELOW_NORMAL    (THREAD_PRIORITY_LOWEST+1)
#define THREAD_PRIORITY_NORMAL          0
#define THREAD_PRIORITY_HIGHEST         THREAD_BASE_PRIORITY_MAX
#define THREAD_PRIORITY_ABOVE_NORMAL    (THREAD_PRIORITY_HIGHEST-1)

#define THREAD_PRIORITY_TIME_CRITICAL   THREAD_BASE_PRIORITY_LOWRT
#define THREAD_PRIORITY_IDLE            THREAD_BASE_PRIORITY_IDLE
#endif

void CTaskView::AddPriority(EPriorityType Type, const QString& Name, int Value)
{
	ASSERT(Value != INVALID_PRIORITY && Value != UNDEFINED_PRIORITY);

	QAction* pAction;
	switch (Type)
	{
	case eProcess:
	case eThread:
					pAction = m_pPriority->addAction(Name, this, SLOT(OnPriority()));  break;
	case eIO:		pAction = m_pIoPriority->addAction(Name, this, SLOT(OnPriority())); break;
	case ePage:		pAction = m_pPagePriority->addAction(Name, this, SLOT(OnPriority())); break;
	}
	pAction->setCheckable(true);
	m_pPriorityLevels.insert(pAction, SPriority(Type, Value));
}

void CTaskView::AddPriorityItemsToMenu(EPriorityType Style, bool bAddSeparator)
{
	if(bAddSeparator)
		m_pMenu->addSeparator();

	m_pPermissions = m_pMenu->addAction(tr("Permissions"), this, SLOT(OnPermissions()));

	m_pMenu->addSeparator();

	m_pAffinity = m_pMenu->addAction(tr("Affinity"), this, SLOT(OnAffinity()));

	m_pPriority = m_pMenu->addMenu(tr("Priority"));
	switch (Style)
	{
	default:
#ifdef WIN32
		AddPriority(eProcess, tr("Real time"), 4);		// PROCESS_PRIORITY_CLASS_REALTIME
		AddPriority(eProcess, tr("High"), 3);			// PROCESS_PRIORITY_CLASS_HIGH
		AddPriority(eProcess, tr("Above normal"), 6);	// PROCESS_PRIORITY_CLASS_ABOVE_NORMAL
		AddPriority(eProcess, tr("Normal"), 2);			// PROCESS_PRIORITY_CLASS_NORMAL
		AddPriority(eProcess, tr("Below normal"), 5);	// PROCESS_PRIORITY_CLASS_BELOW_NORMAL
		AddPriority(eProcess, tr("Idle"), 1);			// PROCESS_PRIORITY_CLASS_IDLE
		
#endif
		break;
	case eThread:
#ifdef WIN32
		AddPriority(eThread, tr("Time critical"), THREAD_PRIORITY_TIME_CRITICAL); // THREAD_BASE_PRIORITY_LOWRT // THREAD_BASE_PRIORITY_LOWRT + 1
		AddPriority(eThread, tr("Highest"), THREAD_PRIORITY_HIGHEST);
		AddPriority(eThread, tr("Above normal"), THREAD_PRIORITY_ABOVE_NORMAL);
		AddPriority(eThread, tr("Normal"), THREAD_PRIORITY_NORMAL);
		AddPriority(eThread, tr("Below normal"), THREAD_PRIORITY_BELOW_NORMAL);
		AddPriority(eThread, tr("Lowest"), THREAD_PRIORITY_LOWEST);
		AddPriority(eThread, tr("Idle"), THREAD_PRIORITY_IDLE); // THREAD_BASE_PRIORITY_IDLE // THREAD_BASE_PRIORITY_IDLE - 1
#endif
		break;
	}

	m_pIoPriority = m_pMenu->addMenu(tr("I/O Priority"));
#ifdef WIN32
	AddPriority(eIO, tr("Critical"), 4);				// IoPriorityCritical, // Used by memory manager. Not available for applications.
	AddPriority(eIO, tr("High"), 3);					// IoPriorityHigh, // Used by filesystems for checkpoint I/O.
	AddPriority(eIO, tr("Normal"), 2);					// IoPriorityNormal, // Normal I/Os.
	AddPriority(eIO, tr("Low"), 1);						// IoPriorityLow, // Prefetching for applications.
	AddPriority(eIO, tr("Very low"), 0);				// IoPriorityVeryLow = 0, // Defragging, content indexing and other background I/Os.
#endif

	m_pPagePriority = m_pMenu->addMenu(tr("Page Priority"));
#ifdef WIN32
	AddPriority(ePage, tr("Normal"), 5);				// MEMORY_PRIORITY_NORMAL
	AddPriority(ePage, tr("Below normal"), 4);			// MEMORY_PRIORITY_BELOW_NORMAL
	AddPriority(ePage, tr("Medium"), 3);				// MEMORY_PRIORITY_MEDIUM
	AddPriority(ePage, tr("Low"), 2);					// MEMORY_PRIORITY_LOW
	AddPriority(ePage, tr("Very low"), 1);				// MEMORY_PRIORITY_VERY_LOW
	AddPriority(ePage, tr("Lowest"), 0);				// MEMORY_PRIORITY_LOWEST
#endif
}

void CTaskView::OnMenu(const QPoint& Point)
{
	QList<CTaskPtr>	Tasks = GetSellectedTasks();

	int SuspendedCount = 0;
	int Priority = INVALID_PRIORITY;
	int IoPriority = INVALID_PRIORITY;
	int PagePriority = INVALID_PRIORITY;
	foreach(const CTaskPtr& pTask, Tasks)
	{
		if (pTask->IsSuspended())
			SuspendedCount++;

		if (Priority == INVALID_PRIORITY)
			Priority = pTask->GetPriority();
		else if (Priority != UNDEFINED_PRIORITY 
		 && Priority != pTask->GetPriority())
			Priority = UNDEFINED_PRIORITY;

		if (IoPriority == INVALID_PRIORITY)
			IoPriority = pTask->GetIOPriority();
		else if (IoPriority != UNDEFINED_PRIORITY 
		 && IoPriority != pTask->GetIOPriority())
			IoPriority = UNDEFINED_PRIORITY;

		if (PagePriority == INVALID_PRIORITY)
			PagePriority = pTask->GetPagePriority();
		else if (PagePriority != UNDEFINED_PRIORITY 
		 && PagePriority != pTask->GetPagePriority())
			PagePriority = UNDEFINED_PRIORITY;
	}
	
	m_pTerminate->setEnabled(Tasks.count() > 0);
	m_pSuspend->setVisible(Tasks.count() - SuspendedCount > 0);
	m_pResume->setVisible(SuspendedCount > 0);

	// Priority Items
	m_pAffinity->setEnabled(Tasks.count() > 0);


	m_pPriority->setEnabled(Tasks.count() > 0);
	foreach(QAction* pAction, m_pPriority->actions())
		pAction->setChecked(m_pPriorityLevels[pAction].Value == Priority);
	
	m_pIoPriority->setEnabled(Tasks.count() > 0);
	foreach(QAction* pAction, m_pIoPriority->actions())
		pAction->setChecked(m_pPriorityLevels[pAction].Value == IoPriority);

	m_pPagePriority->setEnabled(Tasks.count() > 0);
	foreach(QAction* pAction, m_pPagePriority->actions())
		pAction->setChecked(m_pPriorityLevels[pAction].Value == PagePriority);

	m_pPermissions->setEnabled(Tasks.count() == 1);

	CPanelView::OnMenu(Point);
}

void CTaskView::OnTaskAction()
{
	if(QMessageBox("TaskExplorer", tr("Do you want to %1 the selected task(s)").arg(((QAction*)sender())->text().toLower())
	 , QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	QList<CTaskPtr>	Tasks = GetSellectedTasks();

	QList<STATUS> Errors;
	foreach(const CTaskPtr& pTask, Tasks)
	{
		STATUS Status = OK;
		if (sender() == m_pTerminate)
			Status = pTask->Terminate();
		else if (sender() == m_pSuspend)
			Status = pTask->Suspend();
		else if (sender() == m_pResume)
			Status = pTask->Resume();

		if(Status.IsError())
			Errors.append(Status);
	}

	CTaskExplorer::CheckErrors(Errors);
}

void CTaskView::OnAffinity()
{
	QList<CTaskPtr>	Tasks = GetSellectedTasks();
	if (Tasks.isEmpty())
		return;

	QVector<int> Affinity(64, 0);

	for (int i = 0; i < Tasks.size(); i++)
	{
		const CTaskPtr& pTask = Tasks.at(i);
		quint64 AffinityMask = pTask->GetAffinityMask();

		for (int j = 0; j < Affinity.size(); j++)
		{
			int curBit = (AffinityMask >> j) & 1ui64;
			if (i == 0)
				Affinity[j] = curBit;
			else if (curBit != Affinity[j]) // && Affinity[j] != 2
				Affinity[j] = 2;
		}
	}
	
	CAffinityDialog AffinityDialog(theAPI->GetCpuCount());
	AffinityDialog.SetName(Tasks.first()->GetName());
	AffinityDialog.SetAffinity(Affinity);
	if (!AffinityDialog.exec())
		return;
	
	Affinity = AffinityDialog.GetAffinity();
	
	QList<STATUS> Errors;
	for (int i = 0; i < Tasks.size(); i++)
	{
		const CTaskPtr& pTask = Tasks.at(i);
		quint64 AffinityMask = pTask->GetAffinityMask();

		for (int j = 0; j < Affinity.size(); j++)
		{
			if (Affinity[j] == 1)
				AffinityMask |= (1ui64 << j);
			else if(Affinity[j] == 0)
				AffinityMask &= ~(1ui64 << j);
		}

		STATUS Status = pTask->SetAffinityMask(AffinityMask);
		if (Status.IsError())
			Errors.append(Status);
	}

	CTaskExplorer::CheckErrors(Errors);
}

void CTaskView::OnPriority()
{
	SPriority Priority = m_pPriorityLevels[(QAction*)sender()];

	QList<CTaskPtr>	Tasks = GetSellectedTasks();

	QList<STATUS> Errors;
	foreach(const CTaskPtr& pTask, Tasks)
	{
		STATUS Status = OK;
		switch (Priority.Type)
		{
		case eProcess:
		case eThread:	Status = pTask->SetPriority(Priority.Value); break;
		case eIO:		Status = pTask->SetIOPriority(Priority.Value); break;
		case ePage:		Status = pTask->SetPagePriority(Priority.Value); break;
		}

		if(Errors.isEmpty())
			Errors.append(Status);
	}

	CTaskExplorer::CheckErrors(Errors);
}

void CTaskView::OnPermissions()
{
#ifdef WIN32
	QList<CTaskPtr>	Tasks = GetSellectedTasks();
	if (Tasks.count() != 1)
		return;

	if (QSharedPointer<CWinThread> pWinThread = Tasks.first().objectCast<CWinThread>())
	{
		pWinThread->OpenPermissions();
	}
	else if (QSharedPointer<CWinProcess> pWinProcess = Tasks.first().objectCast<CWinProcess>())
	{
		pWinProcess->OpenPermissions();
	}
#endif
}