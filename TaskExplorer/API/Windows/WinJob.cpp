#include "stdafx.h"
#include "WinJob.h"
#include "ProcessHacker.h"
#include "WindowsAPI.h"

struct SWinJob
{
	SWinJob()
	{
		QueryHandle = NULL;
		Handle = NULL;
		Type = CWinJob::eProcess;
	}
	HANDLE QueryHandle;
	HANDLE Handle;
	CWinJob::EQueryType Type;
};

CWinJob::CWinJob(QObject *parent)
	:CAbstractInfo(parent)
{
	m_ActiveProcesses = 0;
	m_TotalProcesses = 0;
	m_TerminatedProcesses = 0;

	m_PeakProcessMemoryUsed = 0;
	m_PeakJobMemoryUsed = 0;

	m = new SWinJob();
}

CWinJob::~CWinJob()
{
	if (m->Type != eProcess)
		NtClose(m->QueryHandle);
	delete m;
}

NTSTATUS NTAPI CWinJob__OpenProcessJob(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
	NTSTATUS status = STATUS_INVALID_PARAMETER;
	SWinJob* m = (SWinJob*)Context;
	if (m->Type == CWinJob::eHandle)
	{
		status = NtDuplicateObject(m->QueryHandle, m->Handle, NtCurrentProcess(), Handle, DesiredAccess, 0, 0);
	}
	else
	{
		HANDLE processHandle;
		HANDLE jobHandle = NULL;

		// Note: we are using the query handle of the process instance instead of pid and re opening it
		processHandle = m->QueryHandle;
		//if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION, (HANDLE)Context)))
		//   return status;

		status = KphOpenProcessJob(processHandle, DesiredAccess, &jobHandle);

		//NtClose(processHandle);

		if (NT_SUCCESS(status) && status != STATUS_PROCESS_NOT_IN_JOB && jobHandle)
		{
			*Handle = jobHandle;
		}
		else if (NT_SUCCESS(status))
		{
			status = STATUS_UNSUCCESSFUL;
		}
	}
    return status;
}

CWinJob* CWinJob::JobFromHandle(quint64 ProcessId, quint64 HandleId)
{
	HANDLE processHandle;
    if (!NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_DUP_HANDLE, (HANDLE)ProcessId)))
        return NULL;

	CWinJob* pJob = new CWinJob();
	pJob->m->Type = eHandle;
	pJob->m->QueryHandle = processHandle;
	pJob->m->Handle = (HANDLE)HandleId;
	pJob->InitStaticData();
	return pJob;
}

CWinJob* CWinJob::JobFromProcess(void* QueryHandle)
{
	CWinJob* pJob = new CWinJob();
	pJob->m->Type = eProcess;
	pJob->m->QueryHandle = QueryHandle;
	pJob->InitStaticData();
	return pJob;
}

bool CWinJob::InitStaticData()
{
	QWriteLocker Locker(&m_Mutex);

	HANDLE jobHandle = NULL;
	if (!NT_SUCCESS(CWinJob__OpenProcessJob(&jobHandle, JOB_OBJECT_QUERY, m)))
		return false;

	PPH_STRING jobObjectName = NULL;
	PhGetHandleInformation(NtCurrentProcess(), jobHandle, ULONG_MAX, NULL, NULL, NULL, &jobObjectName);
	m_JobName = CastPhString(jobObjectName);
	if (m_JobName.isEmpty())
		m_JobName = tr("Unnamed job");

	NtClose(jobHandle);

	return true;
}

bool CWinJob::UpdateDynamicData()
{
	QWriteLocker Locker(&m_Mutex); 

	HANDLE jobHandle = NULL;
	if (!NT_SUCCESS(CWinJob__OpenProcessJob(&jobHandle, JOB_OBJECT_QUERY, m)))
		return false;

	//
	m_Limits.clear();

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION extendedLimits;
	if (NT_SUCCESS(PhGetJobExtendedLimits(jobHandle, &extendedLimits)))
    {
        ULONG flags = extendedLimits.BasicLimitInformation.LimitFlags;

        if (flags & JOB_OBJECT_LIMIT_ACTIVE_PROCESS)
			m_Limits.append(SJobLimit(tr("Active processes"), SJobLimit::eNumber, (int)extendedLimits.BasicLimitInformation.ActiveProcessLimit));

        if (flags & JOB_OBJECT_LIMIT_AFFINITY)
			m_Limits.append(SJobLimit(tr("Affinity"), SJobLimit::eAddress, (quint64)extendedLimits.BasicLimitInformation.Affinity));

        if (flags & JOB_OBJECT_LIMIT_BREAKAWAY_OK)
			m_Limits.append(SJobLimit(tr("Breakaway OK"), SJobLimit::eEnabled, true));

        if (flags & JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION)
			m_Limits.append(SJobLimit(tr("Die on unhandled exception"), SJobLimit::eEnabled, true));

        if (flags & JOB_OBJECT_LIMIT_JOB_MEMORY)
			m_Limits.append(SJobLimit(tr("Job memory"), SJobLimit::eSize, (quint64)extendedLimits.JobMemoryLimit));

        if (flags & JOB_OBJECT_LIMIT_JOB_TIME)
			m_Limits.append(SJobLimit(tr("Job time"), SJobLimit::eTimeMs, extendedLimits.BasicLimitInformation.PerJobUserTimeLimit.QuadPart / PH_TICKS_PER_MS));

        if (flags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE)
			m_Limits.append(SJobLimit(tr("Kill on job close"), SJobLimit::eEnabled, true));

        if (flags & JOB_OBJECT_LIMIT_PRIORITY_CLASS)
			m_Limits.append(SJobLimit(tr("Priority class"), SJobLimit::eString, CWinProcess::GetPriorityString(extendedLimits.BasicLimitInformation.PriorityClass)));

        if (flags & JOB_OBJECT_LIMIT_PROCESS_MEMORY)
			m_Limits.append(SJobLimit(tr("Process memory"), SJobLimit::eSize, (quint64)extendedLimits.ProcessMemoryLimit));

        if (flags & JOB_OBJECT_LIMIT_PROCESS_TIME)
			m_Limits.append(SJobLimit(tr("Process time"), SJobLimit::eTimeMs, extendedLimits.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart / PH_TICKS_PER_MS));

        if (flags & JOB_OBJECT_LIMIT_SCHEDULING_CLASS)
			m_Limits.append(SJobLimit(tr("Scheduling class"), SJobLimit::eNumber, (quint32)extendedLimits.BasicLimitInformation.SchedulingClass));

        if (flags & JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK)
			m_Limits.append(SJobLimit(tr("Silent breakaway OK"), SJobLimit::eEnabled, true));

		if (flags & JOB_OBJECT_LIMIT_WORKINGSET)
		{
			m_Limits.append(SJobLimit(tr("Working set minimum"), SJobLimit::eSize, (quint64)extendedLimits.BasicLimitInformation.MinimumWorkingSetSize));
			m_Limits.append(SJobLimit(tr("Working set maximum"), SJobLimit::eSize, (quint64)extendedLimits.BasicLimitInformation.MaximumWorkingSetSize));
		}
    }

	JOBOBJECT_BASIC_UI_RESTRICTIONS basicUiRestrictions;
    if (NT_SUCCESS(PhGetJobBasicUiRestrictions(jobHandle, &basicUiRestrictions)))
    {
        ULONG flags = basicUiRestrictions.UIRestrictionsClass;

        if (flags & JOB_OBJECT_UILIMIT_DESKTOP)
			m_Limits.append(SJobLimit(tr("Desktop limited"), SJobLimit::eLimited, true));

        if (flags & JOB_OBJECT_UILIMIT_DISPLAYSETTINGS)
			m_Limits.append(SJobLimit(tr("Display settings limited"), SJobLimit::eLimited, true));

        if (flags & JOB_OBJECT_UILIMIT_EXITWINDOWS)
			m_Limits.append(SJobLimit(tr("Exit windows limited"), SJobLimit::eLimited, true));

        if (flags & JOB_OBJECT_UILIMIT_GLOBALATOMS)
			m_Limits.append(SJobLimit(tr("Global atoms limited"), SJobLimit::eLimited, true));

        if (flags & JOB_OBJECT_UILIMIT_HANDLES)
			m_Limits.append(SJobLimit(tr("Handles limited"), SJobLimit::eLimited, true));

        if (flags & JOB_OBJECT_UILIMIT_READCLIPBOARD)
			m_Limits.append(SJobLimit(tr("Read clipboard limited"), SJobLimit::eLimited, true));

        if (flags & JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS)
			m_Limits.append(SJobLimit(tr("System parameters limited"), SJobLimit::eLimited, true));

        if (flags & JOB_OBJECT_UILIMIT_WRITECLIPBOARD)
			m_Limits.append(SJobLimit(tr("Write clipboard limited"), SJobLimit::eLimited, true));
    }
	//

	m_Processes.clear();
	PJOBOBJECT_BASIC_PROCESS_ID_LIST processIdList;
    if (NT_SUCCESS(PhGetJobProcessIdList(jobHandle, &processIdList)))
    {
        for (ULONG i = 0; i < processIdList->NumberOfProcessIdsInList; i++)
        {
			CProcessPtr pProcess = theAPI->GetProcessByID(processIdList->ProcessIdList[i]);
			if (!pProcess.isNull())
				m_Processes.insert(processIdList->ProcessIdList[i], pProcess);
        }
        PhFree(processIdList);
    }

    JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION basicAndIo;
	if (NT_SUCCESS(PhGetJobBasicAndIoAccounting(jobHandle, &basicAndIo)))
	{
		m_ActiveProcesses = basicAndIo.BasicInfo.ActiveProcesses;
		m_TotalProcesses = basicAndIo.BasicInfo.TotalProcesses;
		m_TerminatedProcesses = basicAndIo.BasicInfo.TotalTerminatedProcesses;

		m_Stats.UserDelta.Update(basicAndIo.BasicInfo.TotalUserTime.QuadPart);
		m_Stats.KernelDelta.Update(basicAndIo.BasicInfo.TotalKernelTime.QuadPart);
		//basicAndIo.BasicInfo.ThisPeriodTotalUserTime.QuadPart
		//basicAndIo.BasicInfo.ThisPeriodTotalKernelTime.QuadPart

		m_Stats.Io.SetRead(basicAndIo.IoInfo.ReadOperationCount, basicAndIo.IoInfo.ReadTransferCount);
		m_Stats.Io.SetWrite(basicAndIo.IoInfo.WriteOperationCount, basicAndIo.IoInfo.WriteTransferCount);
		m_Stats.Io.SetOther(basicAndIo.IoInfo.OtherOperationCount, basicAndIo.IoInfo.OtherTransferCount);

		m_Stats.PageFaultsDelta.Update(basicAndIo.BasicInfo.TotalPageFaultCount);
	}

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION extendedLimitInfo;
	if (NT_SUCCESS(PhGetJobExtendedLimits(jobHandle, &extendedLimitInfo)))
	{
		m_PeakProcessMemoryUsed = extendedLimitInfo.PeakProcessMemoryUsed;
		m_PeakJobMemoryUsed = extendedLimitInfo.PeakJobMemoryUsed;
	}

	NtClose(jobHandle);

	m_Stats.UpdateStats();

	return true;
}

STATUS CWinJob::Terminate()
{   
    HANDLE jobHandle;
	NTSTATUS status = CWinJob__OpenProcessJob(&jobHandle, JOB_OBJECT_TERMINATE, m);
	if (!NT_SUCCESS(status))
		return ERR(tr("Failed to open job"), status);

    status = NtTerminateJobObject(jobHandle, STATUS_SUCCESS);

    NtClose(jobHandle);

    if (!NT_SUCCESS(status))
		return ERR(tr("Failed to terminate job"), status);

	return OK;
}

STATUS CWinJob::AddProcess(quint64 ProcessId)
{
	NTSTATUS status;
	HANDLE processId = (HANDLE)ProcessId;
    HANDLE processHandle;
    HANDLE jobHandle;

    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_TERMINATE | PROCESS_SET_QUOTA, processId)))
    {
        if (NT_SUCCESS(status = CWinJob__OpenProcessJob(&jobHandle, JOB_OBJECT_ASSIGN_PROCESS | JOB_OBJECT_QUERY, m)))
        {
            status = NtAssignProcessToJobObject(jobHandle, processHandle);
            NtClose(jobHandle);
        }
        NtClose(processHandle);
    }

	if (!NT_SUCCESS(status))
		return ERR(tr("Unable to add the process to the job"), status);
	return OK;
}

NTSTATUS NTAPI CWinJob__cbPermissionsClosed(_In_opt_ PVOID Context)
{
	SWinJob* context = (SWinJob*)Context;
	delete context;

	return STATUS_SUCCESS;
}

void CWinJob::OpenPermissions()
{
	QReadLocker Locker(&m_Mutex); 
	SWinJob* context = new SWinJob();
	context->QueryHandle = m->QueryHandle;
	context->Handle = m->Handle;
	context->Type = m->Type;
	Locker.unlock();
    PhEditSecurity(NULL, L"Job", L"Job", CWinJob__OpenProcessJob, CWinJob__cbPermissionsClosed, context);
}