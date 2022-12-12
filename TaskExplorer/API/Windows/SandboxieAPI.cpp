#include "stdafx.h"
#include "../../GUI/TaskExplorer.h"
#include "SandboxieAPI.h"
#include "WindowsAPI.h"
#include "ProcessHacker.h"


#define SBIE_DEVICE_NAME		L"\\Device\\SandboxieDriverApi"
#define SBIE_SBIEDRV_CTLCODE	CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_NEITHER, FILE_ANY_ACCESS)
#define SBIE_API_NUM_ARGS       8
#define SBIE_CONF_LINE_LEN      2000    // keep in sync with drv/conf.c

#define API_ARGS_BEGIN(x)           typedef struct _##x { ULONG64 func_code;
#define API_ARGS_FIELD(t,m)         union { ULONG64 val64; t val; } m;
#define API_ARGS_CLOSE(x)           } x;

enum {
    API_FIRST                   = 0x12340000L,

    API_GET_VERSION,
    API_GET_WORK_DEPRECATED,    			    // deprecated
    API_LOG_MESSAGE,
    API_GET_LICENSE_PRE_V3_48_DEPRECATED,       // deprecated
    API_SET_LICENSE_PRE_V3_48_DEPRECATED,       // deprecated
    API_START_PROCESS_PRE_V3_44_DEPRECATED,     // deprecated
    API_QUERY_PROCESS,
    API_QUERY_BOX_PATH,
    API_QUERY_PROCESS_PATH,
    API_QUERY_PATH_LIST,
    API_ENUM_PROCESSES,
    API_DISABLE_FORCE_PROCESS,
    API_HOOK_TRAMP_DEPRECATED,					// deprecated
    API_UNMOUNT_HIVES_DEPRECATED,               // deprecated
    API_QUERY_CONF,
    API_RELOAD_CONF,
    API_CREATE_DIR_OR_LINK,
    API_DUPLICATE_OBJECT,
    API_GET_INJECT_SAVE_AREA_DEPRECATED,        // deprecated
    API_RENAME_FILE,
    API_SET_USER_NAME,
    API_INIT_GUI,
    API_UNLOAD_DRIVER,
    API_GET_SET_DEVICE_MAP_DEPRECATED,          // deprecated
    API_SESSION_SET_LEADER_DEPRECATED,          // deprecated
    API_GLOBAL_FORCE_PROCESS_DEPRECATED,        // deprecated
    API_MONITOR_CONTROL,
    API_MONITOR_PUT_DEPRECATED,                 // deprecated
    API_MONITOR_GET_DEPRECATED,                 // deprecated
    API_GET_UNMOUNT_HIVE,
    API_GET_FILE_NAME,
    API_REFRESH_FILE_PATH_LIST,
    API_SET_LSA_AUTH_PKG,
    API_OPEN_FILE,
    API_SESSION_CHECK_LEADER_DEPRECATED,        // deprecated
    API_START_PROCESS,
    API_CHECK_INTERNET_ACCESS,
    API_GET_HOME_PATH,
    API_GET_BLOCKED_DLL,
    API_QUERY_LICENSE,
    API_ACTIVATE_LICENSE_DEPRECATED,            // deprecated
    API_OPEN_DEVICE_MAP,
    API_OPEN_PROCESS,
    API_QUERY_PROCESS_INFO,
    API_IS_BOX_ENABLED,
    API_SESSION_LEADER,
    API_QUERY_SYMBOLIC_LINK,
    API_OPEN_KEY,
    API_SET_LOW_LABEL_KEY,
    API_OVERRIDE_PROCESS_TOKEN_DEPRECATED,      // deprecated
    API_SET_SERVICE_PORT,
    API_INJECT_COMPLETE,
    API_QUERY_SYSCALLS,
    API_INVOKE_SYSCALL,
    API_GUI_CLIPBOARD,
    API_ALLOW_SPOOLER_PRINT_TO_FILE_DEPRECATED, // deprecated
    API_RELOAD_CONF2,
    API_MONITOR_PUT2,
    API_GET_SPOOLER_PORT_DEPRECATED,            // deprecated
    API_GET_WPAD_PORT_DEPRECATED,               // deprecated
    API_SET_GAME_CONFIG_STORE_PORT_DEPRECATED,  // deprecated
    API_SET_SMART_CARD_PORT_DEPRECATED,         // deprecated
	API_MONITOR_GET_EX,
	API_GET_MESSAGE,
	API_PROCESS_EXEMPTION_CONTROL,
    API_GET_DYNAMIC_PORT_FROM_PID,
    API_OPEN_DYNAMIC_PORT,

    API_LAST
};

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
API_ARGS_FIELD(ULONG, info_type) // in
API_ARGS_FIELD(ULONG64 *, info_data) // out
API_ARGS_FIELD(ULONG64, ext_data) // opt in
API_ARGS_CLOSE(API_QUERY_PROCESS_INFO_ARGS)

API_ARGS_BEGIN(API_QUERY_BOX_PATH_ARGS)
API_ARGS_FIELD(WCHAR *,box_name)
API_ARGS_FIELD(UNICODE_STRING64 *,file_path)
API_ARGS_FIELD(UNICODE_STRING64 *,key_path)
API_ARGS_FIELD(UNICODE_STRING64 *,ipc_path)
API_ARGS_FIELD(ULONG *,file_path_len)
API_ARGS_FIELD(ULONG *,key_path_len)
API_ARGS_FIELD(ULONG *,ipc_path_len)
API_ARGS_CLOSE(API_QUERY_BOX_PATH_ARGS)

API_ARGS_BEGIN(API_QUERY_PROCESS_PATH_ARGS)
API_ARGS_FIELD(HANDLE,process_id)
API_ARGS_FIELD(UNICODE_STRING64 *,file_path)
API_ARGS_FIELD(UNICODE_STRING64 *,key_path)
API_ARGS_FIELD(UNICODE_STRING64 *,ipc_path)
API_ARGS_FIELD(ULONG *,file_path_len)
API_ARGS_FIELD(ULONG *,key_path_len)
API_ARGS_FIELD(ULONG *,ipc_path_len)
API_ARGS_CLOSE(API_QUERY_PROCESS_PATH_ARGS)

API_ARGS_BEGIN(API_QUERY_PATH_LIST_ARGS)
API_ARGS_FIELD(ULONG,path_code)
API_ARGS_FIELD(ULONG *,path_len)
API_ARGS_FIELD(WCHAR *,path_str)
API_ARGS_FIELD(HANDLE,process_id)
API_ARGS_CLOSE(API_QUERY_PATH_LIST_ARGS)


struct SSandboxieAPI
{
	SSandboxieAPI()
	{
	}

	LONG QueryProcess(HANDLE ProcessId, ULONG image_name_len_in_wchars, WCHAR *out_box_name_wchar34, WCHAR *out_image_name_wcharXXX, WCHAR *out_sid_wchar96, ULONG *out_session_id, ULONG64 *out_create_time)
	{
		__declspec(align(8)) UNICODE_STRING64 BoxName;
		__declspec(align(8)) UNICODE_STRING64 ImageName;
		__declspec(align(8)) UNICODE_STRING64 SidString;
		__declspec(align(8)) ULONG64 parms[SBIE_API_NUM_ARGS];
		API_QUERY_PROCESS_ARGS *args = (API_QUERY_PROCESS_ARGS *)parms;

		memset(parms, 0, sizeof(parms));
		args->func_code = API_QUERY_PROCESS;

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

	ULONG64 QueryProcessInfoEx(
		HANDLE ProcessId,
		ULONG info_type,
		ULONG64 ext_data)
	{
		NTSTATUS status;
		__declspec(align(8)) ULONG64 ResultValue;
		__declspec(align(8)) ULONG64 parms[SBIE_API_NUM_ARGS];
		API_QUERY_PROCESS_INFO_ARGS *args = (API_QUERY_PROCESS_INFO_ARGS *)parms;

		memset(parms, 0, sizeof(parms));
		args->func_code             = API_QUERY_PROCESS_INFO;

		args->process_id.val64      = (ULONG64)(ULONG_PTR)ProcessId;
		args->info_type.val64       = (ULONG64)(ULONG_PTR)info_type;
		args->info_data.val64       = (ULONG64)(ULONG_PTR)&ResultValue;
		args->ext_data.val64        = (ULONG64)(ULONG_PTR)ext_data;

		status = IoControl(parms);

		if (! NT_SUCCESS(status))
			ResultValue = 0;

		return ResultValue;
	}

	/*LONG QueryBoxPath(
    const WCHAR *box_name,              // WCHAR [34]
    WCHAR *out_file_path,
    WCHAR *out_key_path,
    WCHAR *out_ipc_path,
    ULONG *inout_file_path_len,
    ULONG *inout_key_path_len,
	ULONG *inout_ipc_path_len)
	{
		NTSTATUS status;
		__declspec(align(8)) UNICODE_STRING64 FilePath;
		__declspec(align(8)) UNICODE_STRING64 KeyPath;
		__declspec(align(8)) UNICODE_STRING64 IpcPath;
		__declspec(align(8)) ULONG64 parms[SBIE_API_NUM_ARGS];
		API_QUERY_BOX_PATH_ARGS *args = (API_QUERY_BOX_PATH_ARGS *)parms;

		memset(parms, 0, sizeof(parms));
		args->func_code = API_QUERY_BOX_PATH;

		args->box_name.val64 = (ULONG64)(ULONG_PTR)box_name;

		if (out_file_path) {
			FilePath.Length = 0;
			FilePath.MaximumLength = (USHORT)*inout_file_path_len;
			FilePath.Buffer = (ULONG64)(ULONG_PTR)out_file_path;
			args->file_path.val64 = (ULONG64)(ULONG_PTR)&FilePath;
		}

		if (out_key_path) {
			KeyPath.Length = 0;
			KeyPath.MaximumLength = (USHORT)*inout_key_path_len;
			KeyPath.Buffer = (ULONG64)(ULONG_PTR)out_key_path;
			args->key_path.val64 = (ULONG64)(ULONG_PTR)&KeyPath;
		}

		if (out_ipc_path) {
			IpcPath.Length = 0;
			IpcPath.MaximumLength = (USHORT)*inout_ipc_path_len;
			IpcPath.Buffer = (ULONG64)(ULONG_PTR)out_ipc_path;
			args->ipc_path.val64 = (ULONG64)(ULONG_PTR)&IpcPath;
		}

		args->file_path_len.val = inout_file_path_len;
		args->key_path_len.val  = inout_key_path_len;
		args->ipc_path_len.val  = inout_ipc_path_len;

		status = IoControl(parms);

		if (! NT_SUCCESS(status)) {
			if (out_file_path)
				*out_file_path = L'\0';
			if (out_key_path)
				*out_key_path = L'\0';
			if (out_ipc_path)
				*out_ipc_path = L'\0';
		}

		return status;
	}*/

	LONG QueryProcessPath(
    HANDLE ProcessId,
    WCHAR *out_file_path,
    WCHAR *out_key_path,
    WCHAR *out_ipc_path,
    ULONG *inout_file_path_len,
    ULONG *inout_key_path_len,
    ULONG *inout_ipc_path_len)
	{
		NTSTATUS status;
		__declspec(align(8)) UNICODE_STRING64 FilePath;
		__declspec(align(8)) UNICODE_STRING64 KeyPath;
		__declspec(align(8)) UNICODE_STRING64 IpcPath;
		__declspec(align(8)) ULONG64 parms[SBIE_API_NUM_ARGS];
		API_QUERY_PROCESS_PATH_ARGS *args = (API_QUERY_PROCESS_PATH_ARGS *)parms;

		memset(parms, 0, sizeof(parms));
		args->func_code = API_QUERY_PROCESS_PATH;

		args->process_id.val64 = (ULONG64)(ULONG_PTR)ProcessId;

		if (out_file_path) {
			FilePath.Length = 0;
			FilePath.MaximumLength = (USHORT)*inout_file_path_len;
			FilePath.Buffer = (ULONG64)(ULONG_PTR)out_file_path;
			args->file_path.val64 = (ULONG64)(ULONG_PTR)&FilePath;
		}

		if (out_key_path) {
			KeyPath.Length = 0;
			KeyPath.MaximumLength = (USHORT)*inout_key_path_len;
			KeyPath.Buffer = (ULONG64)(ULONG_PTR)out_key_path;
			args->key_path.val64 = (ULONG64)(ULONG_PTR)&KeyPath;
		}

		if (out_ipc_path) {
			IpcPath.Length = 0;
			IpcPath.MaximumLength = (USHORT)*inout_ipc_path_len;
			IpcPath.Buffer = (ULONG64)(ULONG_PTR)out_ipc_path;
			args->ipc_path.val64 = (ULONG64)(ULONG_PTR)&IpcPath;
		}

		args->file_path_len.val = inout_file_path_len;
		args->key_path_len.val  = inout_key_path_len;
		args->ipc_path_len.val  = inout_ipc_path_len;

		status = IoControl(parms);

		if (! NT_SUCCESS(status)) {
			if (out_file_path)
				*out_file_path = L'\0';
			if (out_key_path)
				*out_key_path = L'\0';
			if (out_ipc_path)
				*out_ipc_path = L'\0';
		}

		return status;
	}

	LONG QueryPathList(
    ULONG path_code,
    ULONG *path_len,
    WCHAR *path_str,
    HANDLE process_id)
	{
		NTSTATUS status;
		__declspec(align(8)) ULONG64 parms[SBIE_API_NUM_ARGS];
		API_QUERY_PATH_LIST_ARGS *args = (API_QUERY_PATH_LIST_ARGS *)parms;

		memset(parms, 0, sizeof(parms));
		args->func_code = API_QUERY_PATH_LIST;
		args->path_code.val = path_code;
		args->path_len.val64 = (ULONG64)(ULONG_PTR)path_len;
		args->path_str.val64 = (ULONG64)(ULONG_PTR)path_str;
		args->process_id.val64 = (ULONG64)(ULONG_PTR)process_id;
		status = IoControl(parms);

		return status;
	}

	NTSTATUS SbieIniGet(const std::wstring& section, const std::wstring& setting, quint32 index, std::wstring& value)
	{
		WCHAR out_buffer[SBIE_CONF_LINE_LEN] = { 0 };

		__declspec(align(8)) UNICODE_STRING64 Output = { 0, SBIE_CONF_LINE_LEN - 4 , (ULONG64)out_buffer };
		__declspec(align(8)) ULONG64 parms[SBIE_API_NUM_ARGS];

		memset(parms, 0, sizeof(parms));
		parms[0] = API_QUERY_CONF;
		parms[1] = (ULONG64)section.c_str();
		parms[2] = (ULONG64)setting.c_str();
		parms[3] = (ULONG64)&index;
		parms[4] = (ULONG64)&Output;
		NTSTATUS status = IoControl(parms);
		
		value = std::wstring(out_buffer);

		return status;
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
		
		std::wstring FileRoot(filePathLength + 1, '0');
		std::wstring KeyRoot(filePathLength + 1, '0');
		std::wstring IpcRoot(filePathLength + 1, '0');
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
	if(!NT_SUCCESS(m->QueryProcess((HANDLE)ProcessId, 0, boxName, NULL, NULL, NULL, NULL)))
		return QString();
	return QString::fromWCharArray(boxName);
}

bool CSandboxieAPI::IsSandBoxed(quint64 ProcessId) const
{
	ULONG session_id = 0;
	if (!NT_SUCCESS(m->QueryProcess((HANDLE)ProcessId, 0, NULL, NULL, NULL, &session_id, NULL)))
		return false;
	return session_id != 0;
}

quint64 CSandboxieAPI::OpenOriginalToken(quint64 ProcessId) const
{
	return m->QueryProcessInfoEx((HANDLE)ProcessId, 'ptok', 0);
}

quint64 CSandboxieAPI::OpenOriginalToken(quint64 ProcessId, quint64 ThreadId) const
{
	return m->QueryProcessInfoEx((HANDLE)ProcessId, 'itok', ThreadId);
}

bool CSandboxieAPI::TestOriginalToken(quint64 ProcessId, quint64 ThreadId) const
{
	return m->QueryProcessInfoEx((HANDLE)ProcessId, 'ttok', ThreadId);
}

bool CSandboxieAPI::QueryProcess(quint64 ProcessId, QString& BoxName, QString& ImagName, QString& SID, quint32* SessionId, quint64* CreationTime)
{
	WCHAR boxName[34];
	WCHAR imageName[MAX_PATH];
	WCHAR sid[96];
	if (!NT_SUCCESS(m->QueryProcess((HANDLE)ProcessId, MAX_PATH, boxName, imageName, sid, (ULONG*)SessionId, CreationTime)))
		return false;

	BoxName = QString::fromWCharArray(boxName);
	ImagName = QString::fromWCharArray(imageName);
	SID = QString::fromWCharArray(sid);

	return true;
}

void CSandboxieAPI::GetProcessPaths(quint64 ProcessId, QString& FilePath, QString& KeyPath, QString& IpcPath) const
{
	ULONG filePathLength = 0;
	ULONG keyPathLength = 0;
	ULONG ipcPathLength = 0;
	if (!NT_SUCCESS(m->QueryProcessPath((HANDLE)ProcessId, NULL, NULL, NULL, &filePathLength, &keyPathLength, &ipcPathLength)))
		return;
		
	std::wstring fileRoot(filePathLength/2, '0');
	std::wstring keyRoot(keyPathLength/2, '0');
	std::wstring ipcRoot(ipcPathLength/2, '0');
	if (!NT_SUCCESS(m->QueryProcessPath((HANDLE)ProcessId, (WCHAR*)fileRoot.data(), (WCHAR*)keyRoot.data(), (WCHAR*)ipcRoot.data(), &filePathLength, &keyPathLength, &ipcPathLength)))
		return;

	FilePath = QString::fromWCharArray(fileRoot.c_str());
	KeyPath = QString::fromWCharArray(keyRoot.c_str());
	IpcPath = QString::fromWCharArray(ipcRoot.c_str());
}

void CSandboxieAPI::QueryPathList(quint64 ProcessId, quint32 path_code, QStringList& Paths) const
{
    ULONG len;
    LONG status = m->QueryPathList(path_code, &len, NULL, (HANDLE)ProcessId);
    if (status != STATUS_SUCCESS)
        return;

	std::wstring path(len / 2, '0');
    status = m->QueryPathList(path_code, NULL, (WCHAR*)path.c_str(), (HANDLE)ProcessId);

	Paths.clear();
	for (const WCHAR* ptr = path.c_str(); *ptr; )
	{
		QString temp = QString::fromWCharArray(ptr);
		ptr += temp.length() + 1;
		Paths.append(temp);
	}
}

#define CONF_GET_NO_GLOBAL          0x40000000L
#define CONF_GET_NO_EXPAND          0x20000000L
#define CONF_GET_NO_TEMPLS          0x10000000L

QList<QPair<QString, QString>> CSandboxieAPI::GetIniSection(const QString& BoxName, qint32* pStatus, bool withTemplates) const
{
	qint32 status = STATUS_SUCCESS;

	int flags = CONF_GET_NO_EXPAND;
	if (!withTemplates)
		flags |= CONF_GET_NO_TEMPLS | CONF_GET_NO_GLOBAL;

	QList<QPair<QString, QString>> Settings;
	for (int setting_index = 0; ; setting_index++)
	{
		std::wstring setting_name;
		status = m->SbieIniGet(BoxName.toStdWString(), L"", setting_index | flags, setting_name);
		if (status == STATUS_RESOURCE_NAME_NOT_FOUND) {
			status = STATUS_SUCCESS;
			break;
		}
		if (status != STATUS_SUCCESS)
			break;

		for (int value_index = 0; ; value_index++)
		{
			std::wstring setting_value;
			status = m->SbieIniGet(BoxName.toStdWString(), setting_name, value_index | flags, setting_value);
			if (status == STATUS_RESOURCE_NAME_NOT_FOUND) {
				status = STATUS_SUCCESS;
				break;
			}
			if (status != STATUS_SUCCESS)
				break;

			Settings.append(qMakePair(QString::fromStdWString(setting_name), QString::fromStdWString(setting_value)));
		}

		if (status != STATUS_SUCCESS)
			break;
	}

	if (pStatus) *pStatus = status;
	return Settings;
}

quint32 CSandboxieAPI::QueryProcessInfoEx(quint64 ProcessId, quint32* pil, quint32* pit)
{
	quint32 flags = m->QueryProcessInfoEx((HANDLE)ProcessId, 0, 0);
	if(pil)
		*pil = m->QueryProcessInfoEx((HANDLE)ProcessId, 'pril', 0);
	if(pit)
		*pit = m->QueryProcessInfoEx((HANDLE)ProcessId, 'gpit', 0);
	return flags;
}

enum {
	DLL_IMAGE_UNSPECIFIED = 0,
	DLL_IMAGE_SANDBOXIE_RPCSS,
	DLL_IMAGE_SANDBOXIE_DCOMLAUNCH,
	DLL_IMAGE_SANDBOXIE_CRYPTO,
	DLL_IMAGE_SANDBOXIE_WUAU,
	DLL_IMAGE_SANDBOXIE_BITS,
	DLL_IMAGE_SANDBOXIE_SBIESVC,
	DLL_IMAGE_MSI_INSTALLER,
	DLL_IMAGE_TRUSTED_INSTALLER,
	DLL_IMAGE_WUAUCLT,
	DLL_IMAGE_SHELL_EXPLORER,
	DLL_IMAGE_INTERNET_EXPLORER,
	DLL_IMAGE_MOZILLA_FIREFOX,
	DLL_IMAGE_WINDOWS_MEDIA_PLAYER,
	DLL_IMAGE_NULLSOFT_WINAMP,
	DLL_IMAGE_PANDORA_KMPLAYER,
	DLL_IMAGE_WINDOWS_LIVE_MAIL,
	DLL_IMAGE_SERVICE_MODEL_REG,
	DLL_IMAGE_RUNDLL32,
	DLL_IMAGE_DLLHOST,
	DLL_IMAGE_DLLHOST_WININET_CACHE,
	DLL_IMAGE_WISPTIS,
	DLL_IMAGE_GOOGLE_CHROME,
	DLL_IMAGE_GOOGLE_UPDATE,
	DLL_IMAGE_ACROBAT_READER,
	DLL_IMAGE_OFFICE_OUTLOOK,
	DLL_IMAGE_OFFICE_EXCEL,
	DLL_IMAGE_FLASH_PLAYER_SANDBOX,
	DLL_IMAGE_PLUGIN_CONTAINER,
	DLL_IMAGE_OTHER_WEB_BROWSER,
	DLL_IMAGE_OTHER_MAIL_CLIENT,
	DLL_IMAGE_LAST
};

QString CSandboxieAPI::ImageTypeToStr(quint32 type)
{
	switch (type)
	{
		case DLL_IMAGE_UNSPECIFIED: return tr("Generic");
		case DLL_IMAGE_SANDBOXIE_RPCSS: return tr("Sbie RpcSs");
		case DLL_IMAGE_SANDBOXIE_DCOMLAUNCH: return tr("Sbie DcomLaunch");
		case DLL_IMAGE_SANDBOXIE_CRYPTO: return tr("Sbie Crypto");
		case DLL_IMAGE_SANDBOXIE_WUAU: return tr("Sbie WuAu Svc");
		case DLL_IMAGE_SANDBOXIE_BITS: return tr("Sbie BITS");
		case DLL_IMAGE_SANDBOXIE_SBIESVC: return tr("Sbie Svc");
		case DLL_IMAGE_MSI_INSTALLER: return tr("Msi Installer");
		case DLL_IMAGE_TRUSTED_INSTALLER: return tr("Trusted Installer");
		case DLL_IMAGE_WUAUCLT: return tr("Windows Update");
		case DLL_IMAGE_SHELL_EXPLORER: return tr("Windows Explorer");
		case DLL_IMAGE_INTERNET_EXPLORER: return tr("Internet Explorer");
		case DLL_IMAGE_MOZILLA_FIREFOX: return tr("Mozilla Firefox (or derivative)");
		case DLL_IMAGE_WINDOWS_MEDIA_PLAYER: return tr("Windows Media Player");
		case DLL_IMAGE_NULLSOFT_WINAMP: return tr("WinAmp");
		case DLL_IMAGE_PANDORA_KMPLAYER: return tr("KM Player");
		case DLL_IMAGE_WINDOWS_LIVE_MAIL: return tr("Windows Live Mail");
		case DLL_IMAGE_SERVICE_MODEL_REG: return tr("Service Model Reg");
		case DLL_IMAGE_RUNDLL32: return tr("RunDll32");
		case DLL_IMAGE_DLLHOST: return tr("DllHost");
		case DLL_IMAGE_DLLHOST_WININET_CACHE: return tr("DllHost (WinInet Cache)");
		case DLL_IMAGE_WISPTIS: return tr("Windows Ink Services");
		case DLL_IMAGE_GOOGLE_CHROME: return tr("Google Chrome (or derivative)");
		case DLL_IMAGE_GOOGLE_UPDATE: return tr("Google Updater");
		case DLL_IMAGE_ACROBAT_READER: return tr("Acrobat Reader");
		case DLL_IMAGE_OFFICE_OUTLOOK: return tr("MS Outlook");
		case DLL_IMAGE_OFFICE_EXCEL: return tr("MS Excel");
		case DLL_IMAGE_FLASH_PLAYER_SANDBOX: return tr("Flash Player");
		case DLL_IMAGE_PLUGIN_CONTAINER: return tr("Firefox plugin container");
		case DLL_IMAGE_OTHER_WEB_BROWSER: return tr("Generic Web Browser");
		case DLL_IMAGE_OTHER_MAIL_CLIENT: return tr("Generic Mail Client");
		default: return tr("Unknown");
	}
}

#define SBIE_FLAG_VALID_PROCESS         0x00000001
#define SBIE_FLAG_FORCED_PROCESS        0x00000002
#define SBIE_FLAG_UNUSED_00000004       0x00000004
#define SBIE_FLAG_PROCESS_IS_START_EXE  0x00000008
#define SBIE_FLAG_PARENT_WAS_START_EXE  0x00000010
#define SBIE_FLAG_IMAGE_FROM_SBIE_DIR   0x00000020
#define SBIE_FLAG_IMAGE_FROM_SANDBOX    0x00000040
#define SBIE_FLAG_DROP_RIGHTS           0x00000080
#define SBIE_FLAG_RIGHTS_DROPPED        0x00000100
#define SBIE_FLAG_OPEN_ALL_WIN_CLASS    0x00002000
//#define SBIE_FLAG_BLOCK_FAKE_INPUT      0x00001000
//#define SBIE_FLAG_BLOCK_SYS_PARAM       0x00004000
#define SBIE_FLAG_PROCESS_IN_PCA_JOB    0x08000000
#define SBIE_FLAG_CREATE_CONSOLE_HIDE   0x10000000
#define SBIE_FLAG_CREATE_CONSOLE_SHOW   0x20000000
#define SBIE_FLAG_PROTECTED_PROCESS     0x40000000
#define SBIE_FLAG_HOST_INJECT_PROCESS   0x80000000

QStringList CSandboxieAPI::ImageFlagsToStr(quint32 flags)
{
	QStringList Flags;

	if ((flags & SBIE_FLAG_VALID_PROCESS) != 0)
		Flags.append("Valid");
	if ((flags & SBIE_FLAG_FORCED_PROCESS) != 0)
		Flags.append("Forced");
	if ((flags & SBIE_FLAG_PROCESS_IS_START_EXE) != 0)
		Flags.append("Is StartExe");
	if ((flags & SBIE_FLAG_PARENT_WAS_START_EXE) != 0)
		Flags.append("Started by StartExe");
	if ((flags & SBIE_FLAG_IMAGE_FROM_SBIE_DIR) != 0)
		Flags.append("From Sbie Dir");
	if ((flags & SBIE_FLAG_IMAGE_FROM_SANDBOX) != 0)
		Flags.append("Image from Box");
	if ((flags & SBIE_FLAG_DROP_RIGHTS) != 0)
		Flags.append("Drop Rights");
	if ((flags & SBIE_FLAG_RIGHTS_DROPPED) != 0)
		Flags.append("Rights Dropped");
	if ((flags & SBIE_FLAG_OPEN_ALL_WIN_CLASS) != 0)
		Flags.append("Win Class Open");
	if ((flags & SBIE_FLAG_PROCESS_IN_PCA_JOB) != 0)
		Flags.append("In PAC Job");
	if ((flags & SBIE_FLAG_CREATE_CONSOLE_HIDE) != 0)
		Flags.append("Cons Hide");
	if ((flags & SBIE_FLAG_CREATE_CONSOLE_SHOW) != 0)
		Flags.append("Cons Show");
	if ((flags & SBIE_FLAG_PROTECTED_PROCESS) != 0)
		Flags.append("Protected");
	if ((flags & SBIE_FLAG_HOST_INJECT_PROCESS) != 0)
		Flags.append("Host Inject");

	//if(Flags.isEmpty())
	//	Flags.append("None");

	return Flags;
}