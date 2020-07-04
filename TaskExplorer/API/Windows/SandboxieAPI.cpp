#include "stdafx.h"
#include "../../GUI/TaskExplorer.h"
#include "SandboxieAPI.h"
#include "WindowsAPI.h"
#include "ProcessHacker.h"


#define SBIE_DEVICE_NAME		L"\\Device\\SandboxieDriverApi"
#define SBIE_SBIEDRV_CTLCODE	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_NEITHER, FILE_ANY_ACCESS)
#define SBIE_API_NUM_ARGS       8

#define API_ARGS_BEGIN(x)           typedef struct _##x { ULONG64 func_code;
#define API_ARGS_FIELD(t,m)         union { ULONG64 val64; t val; } m;
#define API_ARGS_CLOSE(x)           } x;

API_ARGS_BEGIN(API_QUERY_PROCESS_ARGS)
API_ARGS_FIELD(HANDLE,process_id)
API_ARGS_FIELD(UNICODE_STRING64 *,box_name)
API_ARGS_FIELD(UNICODE_STRING64 *,image_name)
API_ARGS_FIELD(UNICODE_STRING64 *,sid_string)
API_ARGS_FIELD(ULONG *,session_id)
API_ARGS_FIELD(ULONG64 *,create_time)
API_ARGS_CLOSE(API_QUERY_PROCESS_ARGS)

API_ARGS_BEGIN(API_QUERY_PROCESS_INFO_ARGS)
API_ARGS_FIELD(HANDLE, process_id)
API_ARGS_FIELD(ULONG, info_type)
API_ARGS_FIELD(ULONG64 *, info_data)
API_ARGS_CLOSE(API_QUERY_PROCESS_INFO_ARGS)

struct SSandboxieAPI
{
	SSandboxieAPI()
	{
	}

	LONG SbieApiQueryProcess(HANDLE ProcessId, ULONG image_name_len_in_wchars, WCHAR *out_box_name_wchar34, WCHAR *out_image_name_wcharXXX, WCHAR *out_sid_wchar96, ULONG *out_session_id, ULONG64 *out_create_time)
	{
		__declspec(align(8)) UNICODE_STRING64 BoxName;
		__declspec(align(8)) UNICODE_STRING64 ImageName;
		__declspec(align(8)) UNICODE_STRING64 SidString;
		__declspec(align(8)) ULONG64 parms[SBIE_API_NUM_ARGS];
		API_QUERY_PROCESS_ARGS *args = (API_QUERY_PROCESS_ARGS *)parms;

		memset(parms, 0, sizeof(parms));
		args->func_code = 0x12340007; // API_QUERY_PROCESS;

		args->process_id.val64 = (ULONG64)(ULONG_PTR)ProcessId;

		if (out_box_name_wchar34) {
			BoxName.Length = 0;
			BoxName.MaximumLength = (USHORT)(sizeof(WCHAR) * 34);
			BoxName.Buffer = (ULONG64)(ULONG_PTR)out_box_name_wchar34;
			args->box_name.val64 = (ULONG64)(ULONG_PTR)&BoxName;
		}

		if (out_image_name_wcharXXX) {
			ImageName.Length = 0;
			ImageName.MaximumLength =
				(USHORT)(sizeof(WCHAR) * image_name_len_in_wchars);
			ImageName.Buffer = (ULONG64)(ULONG_PTR)out_image_name_wcharXXX;
			args->image_name.val64 = (ULONG64)(ULONG_PTR)&ImageName;
		}

		if (out_sid_wchar96) {
			SidString.Length = 0;
			SidString.MaximumLength = (USHORT)(sizeof(WCHAR) * 96);
			SidString.Buffer = (ULONG64)(ULONG_PTR)out_sid_wchar96;
			args->sid_string.val64 = (ULONG64)(ULONG_PTR)&SidString;
		}

		if (out_session_id)
			args->session_id.val64 = (ULONG64)(ULONG_PTR)out_session_id;

		if (out_create_time)
			args->create_time.val64 = (ULONG64)(ULONG_PTR)out_create_time;

		IO_STATUS_BLOCK IoStatusBlock;
		NTSTATUS status = IoControl(parms);

		return status;
	}

	ULONG64 QueryProcessInfo(HANDLE ProcessId, ULONG info_type)
	{
		__declspec(align(8)) ULONG64 ResultValue;
		__declspec(align(8)) ULONG64 parms[SBIE_API_NUM_ARGS];
		API_QUERY_PROCESS_INFO_ARGS *args = (API_QUERY_PROCESS_INFO_ARGS *)parms;

		memset(parms, 0, sizeof(parms));
		args->func_code = 0x1234002C; // API_QUERY_PROCESS_INFO;

		args->process_id.val64 = (ULONG64)(ULONG_PTR)ProcessId;
		args->info_type.val64 = (ULONG64)(ULONG_PTR)info_type;
		args->info_data.val64 = (ULONG64)(ULONG_PTR)&ResultValue;

		NTSTATUS status = IoControl(parms);
		if (!NT_SUCCESS(status))
			ResultValue = 0;

		return ResultValue;
	}

	NTSTATUS IoControl(ULONG64 *parms)
	{
		NTSTATUS status;
		IO_STATUS_BLOCK IoStatusBlock;

		UNICODE_STRING uni;
		RtlInitUnicodeString(&uni, SBIE_DEVICE_NAME);

		OBJECT_ATTRIBUTES objattrs;
		InitializeObjectAttributes(&objattrs, &uni, OBJ_CASE_INSENSITIVE, NULL, NULL);

		HANDLE SbieApiHandle = INVALID_HANDLE_VALUE;
		status = NtOpenFile(&SbieApiHandle, FILE_GENERIC_READ, &objattrs, &IoStatusBlock, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
		if (!NT_SUCCESS(status))
			return status;

		status = NtDeviceIoControlFile(SbieApiHandle, NULL, NULL, NULL, &IoStatusBlock, SBIE_SBIEDRV_CTLCODE, parms, sizeof(ULONG64) * SBIE_API_NUM_ARGS, NULL, 0);

		NtClose(SbieApiHandle);

		return status;
	}
};

CSandboxieAPI::CSandboxieAPI(QObject* parent)
 : QObject(parent)
{
	m = new SSandboxieAPI();
}

CSandboxieAPI::~CSandboxieAPI()
{
	delete m;
}

/*bool CSandboxieAPI::UpdateSandboxes()
{
    if (!m->QueryBoxPath || !m->EnumBoxes || !m->EnumProcessEx)
        return false;

	QWriteLocker Locker(&m_Mutex);

	QMap<quint64, SBoxedProcess*> OldBoxedProcesses = m_BoxedProcesses;
	QMap<QString, SBoxInfo*> OldSandBoxes = m_SandBoxes;

	WCHAR boxName[34];
    for (LONG index = -1; (index = m->EnumBoxes(index, boxName)) != -1; )
    {
		SBoxInfo* pBoxInfo = OldSandBoxes.take(QString::fromWCharArray(boxName));
		if (pBoxInfo == NULL)
		{
			pBoxInfo = new SBoxInfo();
			pBoxInfo->BoxName = QString::fromWCharArray(boxName);
			m_SandBoxes.insert(pBoxInfo->BoxName, pBoxInfo);
		}

		ULONG filePathLength = 0;
		ULONG keyPathLength = 0;
		ULONG ipcPathLength = 0;
		m->QueryBoxPath(boxName, NULL, NULL, NULL, &filePathLength, &keyPathLength, &ipcPathLength);
		
		wstring FileRoot(filePathLength + 1, '0');
		wstring KeyRoot(filePathLength + 1, '0');
		wstring IpcRoot(filePathLength + 1, '0');
		m->QueryBoxPath(boxName, (WCHAR*)FileRoot.data(), (WCHAR*)KeyRoot.data(), (WCHAR*)IpcRoot.data(), &filePathLength, &keyPathLength, &ipcPathLength);

		pBoxInfo->FileRoot = QString::fromStdWString(FileRoot);
		pBoxInfo->KeyRoot = QString::fromStdWString(KeyRoot);
		pBoxInfo->IpcRoot = QString::fromStdWString(IpcRoot);

		ULONG pids[512];
        if (m->EnumProcessEx(boxName, TRUE, 0, pids) == 0)
        {
            ULONG count = pids[0];
            PULONG pid = &pids[1];

            while (count != 0)
            {
				SBoxedProcess* pBoxedProcess = OldBoxedProcesses.take(*pid);
				if (pBoxedProcess == NULL)
				{
					pBoxedProcess = new SBoxedProcess();
					pBoxedProcess->ProcessId = *pid;
					m_BoxedProcesses.insert(pBoxedProcess->ProcessId, pBoxedProcess);
				}
				pBoxedProcess->BoxName = QString::fromWCharArray(boxName); // update this always in case it changed or the pid got reused

                count--;
                pid++;
            }
        }
    }

	foreach(quint64 pid, OldBoxedProcesses.keys())
		delete m_BoxedProcesses.take(pid);

	foreach(QString name, OldSandBoxes.keys())
		delete m_SandBoxes.take(name);

	return true;
}*/

QString CSandboxieAPI::GetSandBoxName(quint64 ProcessId) const
{
	/*QReadLocker Locker(&m_Mutex);

	SBoxedProcess* pBoxedProcess = m_BoxedProcesses.take(ProcessId);
	return pBoxedProcess ? pBoxedProcess->BoxName : QString();*/

	WCHAR boxName[34];
	if(!NT_SUCCESS(m->SbieApiQueryProcess((HANDLE)ProcessId, 0, boxName, NULL, NULL, NULL, NULL)))
		return QString();
	return QString::fromWCharArray(boxName);
}

bool CSandboxieAPI::IsSandBoxed(quint64 ProcessId) const
{
	ULONG session_id = 0;
	if (!NT_SUCCESS(m->SbieApiQueryProcess((HANDLE)ProcessId, 0, NULL, NULL, NULL, &session_id, NULL)))
		return false;
	return session_id != 0;
}

quint64 CSandboxieAPI::OpenOriginalHandle(quint64 ProcessId) const
{
	return m->QueryProcessInfo((HANDLE)ProcessId, 'ptok');
}