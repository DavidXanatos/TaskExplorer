#include "stdafx.h"
#include "../../GUI/TaskExplorer.h"
#include "WinDriver.h"
#include "WinModule.h"
#include "WindowsAPI.h"

#include "ProcessHacker.h"

CWinDriver::CWinDriver(QObject *parent) 
	: CDriverInfo(parent) 
{
	m_ImageBase = 0;
	m_ImageSize = 0;

	m_pModuleInfo = CModulePtr(new CWinModule());
	connect(m_pModuleInfo.data(), SIGNAL(AsyncDataDone(bool, ulong, ulong)), this, SLOT(OnAsyncDataDone(bool, ulong, ulong)));
}

CWinDriver::~CWinDriver()
{
	
}

bool CWinDriver::InitStaticData(struct _RTL_PROCESS_MODULE_INFORMATION* Module)
{
	QWriteLocker Locker(&m_Mutex);

	PPH_STRING kernelFileName = PhConvertMultiByteToUtf16((PSTR)Module->FullPathName);
	PPH_STRING newFileName = PhGetFileName(kernelFileName);
	PhDereferenceObject(kernelFileName);

	m_BinaryPath = CastPhString(newFileName);
	m_FileName = (char*)Module->FullPathName + Module->OffsetToFileName;
	m_ImageBase = (quint64)Module->ImageBase;
	m_ImageSize = (quint64)Module->ImageSize;

	m_CreateTimeStamp = GetTime() * 1000;

	qobject_cast<CWinModule*>(m_pModuleInfo)->InitAsyncData(m_BinaryPath);

	return true;
}

void CWinDriver::OnAsyncDataDone(bool IsPacked, ulong ImportFunctions, ulong ImportModules)
{
}

//bool CWinDriver::UpdateDynamicData()
//{
//}