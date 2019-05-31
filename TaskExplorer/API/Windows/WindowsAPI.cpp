#include "stdafx.h"
#include "WindowsAPI.h"
#include "ProcessHacker.h"


CWindowsAPI::CWindowsAPI(QObject *parent) : CSystemAPI(parent)
{
	InitPH();
}


CWindowsAPI::~CWindowsAPI()
{
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

	// Copy the process map Map
	QMap<quint64, CProcessPtr>	OldProcesses = GetProcessList();

	for (PSYSTEM_PROCESS_INFORMATION process = PH_FIRST_PROCESS(processes); process != NULL; process = PH_NEXT_PROCESS(process))
	{
		quint64 ProcessID = (quint64)process->UniqueProcessId;

		// take all running processes out of the copyed map
		QSharedPointer<CWinProcess> pProcess = OldProcesses.take(ProcessID).objectCast<CWinProcess>();
		// ToDo: check creation time?
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

		if (bAdd)
			Added.insert(ProcessID);
		else if (bChanged)
			Changed.insert(ProcessID);
	}

	QWriteLocker Locker(&m_ProcessMutex);
	// purle all processes left as thay are not longer running
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