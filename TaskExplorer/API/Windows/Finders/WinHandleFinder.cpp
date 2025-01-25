#include "stdafx.h"
#include "WinHandleFinder.h"
#include "../ProcessHacker.h"
#include "../../../../MiscHelpers/Common/Common.h"
#include "../WinHandle.h"
#include "../WindowsAPI.h"

CWinHandleFinder::CWinHandleFinder(const QVariant& Type, const QRegularExpression& RegExp, QObject* parent) : CAbstractFinder(parent)
{
	m_Type = Type.toInt();
	m_RegExp = RegExp;
}

CWinHandleFinder::~CWinHandleFinder() 
{
}

void CWinHandleFinder::run()
{
	PSYSTEM_HANDLE_INFORMATION_EX handleInfo;
	if (NT_SUCCESS(PhEnumHandlesEx(&handleInfo)))
	{
		quint64 TimeStamp = GetTime() * 1000;

		//if (!KphCommsIsConnected())
		//{
		//	PhFree(handleInfo);
		//	return false;
		//}

		QMap<HANDLE, HANDLE> ProcessHandles;

		QList<QSharedPointer<QObject>> List;

		int Modulo = handleInfo->NumberOfHandles / 100;
		for (int i = 0; i < handleInfo->NumberOfHandles && !m_bCancel; i++)
		{
			if (Modulo && (i % Modulo) == 0)
				emit Progress(float(i) / handleInfo->NumberOfHandles);

			PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handle = &handleInfo->Handles[i];

			if (m_Type != -1 && handle->ObjectTypeIndex != m_Type)
				continue;

			QSharedPointer<CWinHandle> pWinHandle = QSharedPointer<CWinHandle>(new CWinHandle());

			HANDLE &ProcessHandle = ProcessHandles[handle->UniqueProcessId];
			if (ProcessHandle == NULL)
			{
				if (!NT_SUCCESS(PhOpenProcess(&ProcessHandle, PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, (HANDLE)handle->UniqueProcessId)))
					continue;
			}

			pWinHandle->InitStaticData(handle, 0); // no timestamp
			pWinHandle->InitExtData(handle, (quint64)ProcessHandle);

			if (pWinHandle->m_FileName.isEmpty() || !pWinHandle->m_FileName.contains(m_RegExp))
				continue;

			//pWinHandle->UpdateDynamicData(handle, (quint64)ProcessHandle);

			List.append(pWinHandle);

			if (pWinHandle->m_pProcess.isNull())
				pWinHandle->m_pProcess = theAPI->GetProcessByID((quint64)handle->UniqueProcessId, true);

			// emit results every second
			quint64 NewStamp = GetTime() * 1000;
			if (TimeStamp + 1000 < NewStamp)
			{
				TimeStamp = NewStamp;

				emit Results(List);
				List.clear();
			}
		}

		emit Results(List);
		List.clear();

		foreach(HANDLE ProcessHandle, ProcessHandles)
			NtClose(ProcessHandle);
		ProcessHandles.clear();

		PhFree(handleInfo);
	}

	emit Finished();
}