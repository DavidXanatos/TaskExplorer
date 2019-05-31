#include "stdafx.h"
#include "SystemAPI.h"
#include "Windows\WindowsAPI.h"


CSystemAPI::CSystemAPI(QObject *parent): QObject(parent)
{
}


CSystemAPI::~CSystemAPI()
{
}

CSystemAPI* CSystemAPI::New(QObject *parent)
{
#ifdef WIN32
	return new CWindowsAPI(parent);
#else
	// todo: implement other systems liek Linux
#endif
}

QMap<quint64, CProcessPtr> CSystemAPI::GetProcessList()
{
	QReadLocker Locker(&m_ProcessMutex); // Note: the unlock is triggerd after the copy if the list was finished
	return m_ProcessList;
}

CProcessPtr CSystemAPI::GetProcessByID(quint64 ProcessId)
{
	QReadLocker Locker(&m_ProcessMutex);
	return m_ProcessList.value(ProcessId);
}

QMultiMap<quint64, CSocketPtr> CSystemAPI::GetSocketList()
{
	QReadLocker Locker(&m_SocketMutex);
	return m_SocketList;
}