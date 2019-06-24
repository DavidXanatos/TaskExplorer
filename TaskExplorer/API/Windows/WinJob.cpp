#include "stdafx.h"
#include "WinJob.h"
#include "..\TaskExplorer\GUI\TaskExplorer.h"
#include "ProcessHacker.h"
#include "WindowsAPI.h"

struct SWinJob
{
	SWinJob()
	{
		jobHandle = NULL;
		QueryHandle = NULL;
	}
	HANDLE jobHandle;
	HANDLE QueryHandle;
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
	if(m->jobHandle)
		NtClose(m->jobHandle);

	delete m;
}

NTSTATUS NTAPI PhpOpenProcessJobForPage(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
	NTSTATUS status;
	HANDLE processHandle;
	HANDLE jobHandle = NULL;

	// Note: we are using the query handle of the process instance instead of pid and re opening it
	processHandle = (HANDLE)Context;
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
    return status;
}

bool CWinJob::InitStaticData(quint64 QueryHandle)
{
	QWriteLocker Locker(&m_Mutex); 

	m->QueryHandle = (HANDLE)QueryHandle;

	if (!NT_SUCCESS(PhpOpenProcessJobForPage(&m->jobHandle, JOB_OBJECT_QUERY, m->QueryHandle)))
		return false;

	PPH_STRING jobObjectName = NULL;
	PhGetHandleInformation(NtCurrentProcess(), m->jobHandle, ULONG_MAX, NULL, NULL, NULL, &jobObjectName);
	m_JobName = CastPhString(jobObjectName);
	if (m_JobName.isEmpty())
		m_JobName = tr("Unnamed job");


	/*JOBOBJECT_EXTENDED_LIMIT_INFORMATION extendedLimits;
	if (NT_SUCCESS(PhGetJobExtendedLimits(m->jobHandle, &extendedLimits)))
    {
        ULONG flags = extendedLimits.BasicLimitInformation.LimitFlags;

        if (flags & JOB_OBJECT_LIMIT_ACTIVE_PROCESS)
        {
            WCHAR value[PH_INT32_STR_LEN_1];
            PhPrintUInt32(value, extendedLimits.BasicLimitInformation.ActiveProcessLimit);
            PhpAddLimit(limitsLv, L"Active processes", value);
        }

        if (flags & JOB_OBJECT_LIMIT_AFFINITY)
        {
            WCHAR value[PH_PTR_STR_LEN_1];
            PhPrintPointer(value, (PVOID)extendedLimits.BasicLimitInformation.Affinity);
            PhpAddLimit(limitsLv, L"Affinity", value);
        }

        if (flags & JOB_OBJECT_LIMIT_BREAKAWAY_OK)
        {
            PhpAddLimit(limitsLv, L"Breakaway OK", L"Enabled");
        }

        if (flags & JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION)
        {
            PhpAddLimit(limitsLv, L"Die on unhandled exception", L"Enabled");
        }

        if (flags & JOB_OBJECT_LIMIT_JOB_MEMORY)
        {
            PPH_STRING value = PhaFormatSize(extendedLimits.JobMemoryLimit, -1);
            PhpAddLimit(limitsLv, L"Job memory", value->Buffer);
        }

        if (flags & JOB_OBJECT_LIMIT_JOB_TIME)
        {
            WCHAR value[PH_TIMESPAN_STR_LEN_1];
            PhPrintTimeSpan(value, extendedLimits.BasicLimitInformation.PerJobUserTimeLimit.QuadPart,
                PH_TIMESPAN_DHMS);
            PhpAddLimit(limitsLv, L"Job time", value);
        }

        if (flags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE)
        {
            PhpAddLimit(limitsLv, L"Kill on job close", L"Enabled");
        }

        if (flags & JOB_OBJECT_LIMIT_PRIORITY_CLASS)
        {
            PhpAddLimit(limitsLv, L"Priority class",
                PhGetProcessPriorityClassString(extendedLimits.BasicLimitInformation.PriorityClass));
        }

        if (flags & JOB_OBJECT_LIMIT_PROCESS_MEMORY)
        {
            PPH_STRING value = PhaFormatSize(extendedLimits.ProcessMemoryLimit, -1);
            PhpAddLimit(limitsLv, L"Process memory", value->Buffer);
        }

        if (flags & JOB_OBJECT_LIMIT_PROCESS_TIME)
        {
            WCHAR value[PH_TIMESPAN_STR_LEN_1];
            PhPrintTimeSpan(value, extendedLimits.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart,
                PH_TIMESPAN_DHMS);
            PhpAddLimit(limitsLv, L"Process time", value);
        }

        if (flags & JOB_OBJECT_LIMIT_SCHEDULING_CLASS)
        {
            WCHAR value[PH_INT32_STR_LEN_1];
            PhPrintUInt32(value, extendedLimits.BasicLimitInformation.SchedulingClass);
            PhpAddLimit(limitsLv, L"Scheduling class", value);
        }

        if (flags & JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK)
        {
            PhpAddLimit(limitsLv, L"Silent breakaway OK", L"Enabled");
        }

        if (flags & JOB_OBJECT_LIMIT_WORKINGSET)
        {
            PPH_STRING value;

            value = PhaFormatSize(extendedLimits.BasicLimitInformation.MinimumWorkingSetSize, -1);
            PhpAddLimit(limitsLv, L"Working set minimum", value->Buffer);

            value = PhaFormatSize(extendedLimits.BasicLimitInformation.MaximumWorkingSetSize, -1);
            PhpAddLimit(limitsLv, L"Working set maximum", value->Buffer);
        }
    }

	JOBOBJECT_BASIC_UI_RESTRICTIONS basicUiRestrictions;
    if (NT_SUCCESS(PhGetJobBasicUiRestrictions(m->jobHandle, &basicUiRestrictions)))
    {
        ULONG flags = basicUiRestrictions.UIRestrictionsClass;

        if (flags & JOB_OBJECT_UILIMIT_DESKTOP)
            PhpAddLimit(limitsLv, L"Desktop", L"Limited");
        if (flags & JOB_OBJECT_UILIMIT_DISPLAYSETTINGS)
            PhpAddLimit(limitsLv, L"Display settings", L"Limited");
        if (flags & JOB_OBJECT_UILIMIT_EXITWINDOWS)
            PhpAddLimit(limitsLv, L"Exit windows", L"Limited");
        if (flags & JOB_OBJECT_UILIMIT_GLOBALATOMS)
            PhpAddLimit(limitsLv, L"Global atoms", L"Limited");
        if (flags & JOB_OBJECT_UILIMIT_HANDLES)
            PhpAddLimit(limitsLv, L"Handles", L"Limited");
        if (flags & JOB_OBJECT_UILIMIT_READCLIPBOARD)
            PhpAddLimit(limitsLv, L"Read clipboard", L"Limited");
        if (flags & JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS)
            PhpAddLimit(limitsLv, L"System parameters", L"Limited");
        if (flags & JOB_OBJECT_UILIMIT_WRITECLIPBOARD)
            PhpAddLimit(limitsLv, L"Write clipboard", L"Limited");
    }*/

	return true;
}

bool CWinJob::UpdateDynamicData()
{
	QWriteLocker Locker(&m_Mutex); 


	m_Processes.clear();
	PJOBOBJECT_BASIC_PROCESS_ID_LIST processIdList;
    if (NT_SUCCESS(PhGetJobProcessIdList(m->jobHandle, &processIdList)))
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
	if (NT_SUCCESS(PhGetJobBasicAndIoAccounting(m->jobHandle, &basicAndIo)))
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
	if (NT_SUCCESS(PhGetJobExtendedLimits(m->jobHandle, &extendedLimitInfo)))
	{
		m_PeakProcessMemoryUsed = extendedLimitInfo.PeakProcessMemoryUsed;
		m_PeakJobMemoryUsed = extendedLimitInfo.PeakJobMemoryUsed;
	}

	m_Stats.UpdateStats();

	return true;
}

STATUS CWinJob::Terminate()
{   
    HANDLE jobHandle;
	NTSTATUS status = PhpOpenProcessJobForPage(&jobHandle, JOB_OBJECT_TERMINATE, m->QueryHandle);
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
        if (NT_SUCCESS(status = PhpOpenProcessJobForPage(&jobHandle, JOB_OBJECT_ASSIGN_PROCESS | JOB_OBJECT_QUERY, m->QueryHandle)))
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

void CWinJob::OpenPermissions()
{
	QWriteLocker Locker(&m_Mutex); 

    PhEditSecurity(NULL, L"Job", L"Job", PhpOpenProcessJobForPage, NULL, m->QueryHandle);
}