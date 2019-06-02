#include "stdafx.h"
#include "WindowsAPI.h"
#include "ProcessHacker.h"

int _QHostAddress_type = qRegisterMetaType<QHostAddress>("QHostAddress");

CWindowsAPI::CWindowsAPI(QObject *parent) : CSystemAPI(parent)
{
	InitPH();

	m_pEventMonitor = new CEventMonitor();

	connect(m_pEventMonitor, SIGNAL(NetworkEvent(int, quint64, quint64, ulong, ulong, const QHostAddress&, quint16, const QHostAddress&, quint16)), this, SLOT(OnNetworkEvent(int, quint64, quint64, ulong, ulong, const QHostAddress&, quint16, const QHostAddress&, quint16)));
	connect(m_pEventMonitor, SIGNAL(FileEvent(int, quint64, const QString&)), this, SLOT(OnFileEvent(int, quint64, const QString&)));
	connect(m_pEventMonitor, SIGNAL(DiskEvent(int, quint64, quint64, quint64, ulong, ulong, quint64)), this, SLOT(OnDiskEvent(int, quint64, quint64, quint64, ulong, ulong, quint64)));

	m_pEventMonitor->Init();

	m_pSymbolProvider = CSymbolProviderPtr(new CSymbolProvider());
	m_pSymbolProvider->Init();
}

CWindowsAPI::~CWindowsAPI()
{
	delete m_pEventMonitor;

	ClearPH();
}

bool CWindowsAPI::UpdateProcessList()
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	PVOID processes;
	if (!NT_SUCCESS(PhEnumProcesses(&processes)))
		return false;

	// Copy the process Map
	QMap<quint64, CProcessPtr>	OldProcesses = GetProcessList();

	for (PSYSTEM_PROCESS_INFORMATION process = PH_FIRST_PROCESS(processes); process != NULL; process = PH_NEXT_PROCESS(process))
	{
		quint64 ProcessID = (quint64)process->UniqueProcessId;

		// take all running processes out of the copyed map
		QSharedPointer<CWinProcess> pProcess = OldProcesses.take(ProcessID).objectCast<CWinProcess>();
		bool bAdd = false;
		if (pProcess.isNull())
		{
			pProcess = QSharedPointer<CWinProcess>(new CWinProcess());
			bAdd = pProcess->InitStaticData(process);
			pProcess->InitAsyncData();
			QWriteLocker Locker(&m_ProcessMutex);
			ASSERT(!m_ProcessList.contains(ProcessID));
			m_ProcessList.insert(ProcessID, pProcess);
		}

		bool bChanged = false;
		bChanged = pProcess->UpdateDynamicData(process);

		pProcess->UpdateThreadData(process);

		if (bAdd)
			Added.insert(ProcessID);
		else if (bChanged)
			Changed.insert(ProcessID);
	}

	// purle all processes left as thay are not longer running

	QWriteLocker Locker(&m_ProcessMutex);
	foreach(quint64 ProcessID, OldProcesses.keys())
	{
		QSharedPointer<CWinProcess> pProcess = m_ProcessList.take(ProcessID).objectCast<CWinProcess>();
		pProcess->UnInit();
		Removed.insert(ProcessID);
	}
	Locker.unlock();

	PhFree(processes);

	emit ProcessListUpdated(Added, Changed, Removed);
	return true;
}

static BOOLEAN NetworkImportDone = FALSE;

QMultiMap<quint64, CSocketPtr>::iterator FindSocketEntry(QMultiMap<quint64, CSocketPtr> &Sockets, PH_NETWORK_CONNECTION& connection)
{
	quint64 HashID = ((quint64)connection.LocalEndpoint.Port << 0) | ((quint64)connection.RemoteEndpoint.Port << 16) | (quint64)(((quint32*)&connection.ProcessId)[0] ^ ((quint32*)&connection.ProcessId)[1]) << 32;

	for (QMultiMap<quint64, CSocketPtr>::iterator I = Sockets.find(HashID); I != Sockets.end() && I.key() == HashID; I++)
	{
		if (I.value().objectCast<CWinSocket>()->Match(&connection))
			return I;
	}

	// no matching entry
	return Sockets.end();
}

bool CWindowsAPI::UpdateSocketList()
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

    PPH_NETWORK_CONNECTION connections;
    ulong numberOfConnections;

    if (!NetworkImportDone)
    {
        WSADATA wsaData;
        // Make sure WSA is initialized. (wj32)
        WSAStartup(WINSOCK_VERSION, &wsaData);
        NetworkImportDone = TRUE;
    }

	if (!PhGetNetworkConnections(&connections, &numberOfConnections))
		return false;

	//QWriteLocker Locker(&m_SocketMutex);
	
	// Copy the socket map Map
	QMultiMap<quint64, CSocketPtr> OldSockets = GetSocketList();

	for (ulong i = 0; i < numberOfConnections; i++)
	{
		QSharedPointer<CWinSocket> pSocket;

		bool bAdd = false;
		QMultiMap<quint64, CSocketPtr>::iterator I = FindSocketEntry(OldSockets, connections[i]);
		if (I == OldSockets.end())
		{
			pSocket = QSharedPointer<CWinSocket>(new CWinSocket());
			bAdd = pSocket->InitStaticData(&(connections[i]));
			//pSocket->InitAsyncData();
			QWriteLocker Locker(&m_SocketMutex);
			m_SocketList.insertMulti(pSocket->m_HashID, pSocket);
		}
		else
		{
			pSocket = I.value().objectCast<CWinSocket>();
			OldSockets.erase(I);
		}

		bool bChanged = false;
		bChanged = pSocket->UpdateDynamicData(&(connections[i]));

		if (bAdd)
			Added.insert(pSocket->m_HashID);
		else if (bChanged)
			Changed.insert(pSocket->m_HashID);
	}

	QWriteLocker Locker(&m_SocketMutex);
	// purle all sockets left as thay are not longer running
	for(QMultiMap<quint64, CSocketPtr>::iterator I = OldSockets.begin(); I != OldSockets.end(); ++I)
	{
		m_SocketList.remove(I.key(), I.value());
		Removed.insert(I.key());
	}
	Locker.unlock();

	PhFree(connections);

	emit SocketListUpdated(Added, Changed, Removed);
	return true;
}

void CWindowsAPI::OnNetworkEvent(int Type, quint64 ProcessId, quint64 ThreadId, ulong ProtocolType, ulong TransferSize,
								const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort)
{
}

void CWindowsAPI::OnFileEvent(int Type, quint64 FileId, const QString& FileName)
{
	m_FileNames.insert(FileId, FileName);

	/*switch (Type)
    {
    case CEventMonitor::EtEtwFileNameType: // Name
		qDebug() << "File Named: " << FileName;
        break;
    case CEventMonitor::EtEtwFileCreateType: // FileCreate
        qDebug() << "File Created: " << FileName;
        break;
    case CEventMonitor::EtEtwFileDeleteType: // FileDelete
        qDebug() << "File Deleted: " << FileName;
        break;
	case CEventMonitor::EtEtwFileRundownType:
		qDebug() << "File Named (Rundown): " << FileName;
		break;
    default:
        break;
    }*/
}

void CWindowsAPI::OnDiskEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, ulong IrpFlags, ulong TransferSize, quint64 HighResResponseTime)
{
	QString FileName = m_FileNames.value(FileId, "Unknown");

	/*switch (Type)
    {
    case CEventMonitor::EtEtwDiskReadType:
		qDebug() << "File Read: " << FileName << " (" << TransferSize << " bytes)";
        break;
    case CEventMonitor::EtEtwDiskWriteType:
		qDebug() << "File Writen: " << FileName << " (" << TransferSize << " bytes)";
        break;
    default:
        break;
    }*/
}
