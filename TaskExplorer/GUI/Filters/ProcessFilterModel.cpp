#include "stdafx.h"
#include "ProcessFilterModel.h"
#include "../../../MiscHelpers/Common/Common.h"
#include "../Models/ProcessModel.h"
#ifdef WIN32
#include "../../API/Windows/WinProcess.h"
#endif

CProcessFilterModel::CProcessFilterModel(bool bAlternate, QObject* parrent) 
	: CSortFilterProxyModel(bAlternate, parrent)  
{
	m_bEnabled = false;
	m_iFilterSystem = 0;
#ifdef WIN32
	m_iFilterWindows = 0;
#endif
	m_iFilterService = 0;
	m_iFilterOther = 0;
	m_iFilterOwn = 0;
}

bool CProcessFilterModel::filterAcceptsRow(int source_row, const QModelIndex & source_parent) const
{
	if(m_bEnabled)
	{
		CProcessModel* pModel = (CProcessModel*)sourceModel();
		QModelIndex srcIndex = sourceModel()->index(source_row, 0, source_parent);
		if(srcIndex.isValid())
		{
			CProcessPtr pProcess = pModel->GetProcess(srcIndex);
			if(pProcess)
			{
#ifdef WIN32
				if(m_iFilterWindows && (pProcess.objectCast<CWinProcess>()->IsWindowsProcess() ? (m_iFilterWindows == 1) : (m_iFilterWindows == 2)))
					return false;
#endif
				if(m_iFilterSystem && (pProcess->IsSystemProcess() ? (m_iFilterSystem == 1) : (m_iFilterSystem == 2)))
					return false;

				if(m_iFilterService && (pProcess->IsServiceProcess() ? (m_iFilterService == 1) : (m_iFilterService == 2)))
					return false;

				if(m_iFilterOther || m_iFilterOwn)
				{
					bool bOwn = pProcess->IsUserProcess();
					if(m_iFilterOwn && (bOwn ? (m_iFilterOwn == 1) : (m_iFilterOwn == 2)))
						return false;

					bool bOther = !bOwn && m_Users.contains(pProcess->GetUserName());
					if(m_iFilterOther && (bOther ? (m_iFilterOther == 1) : (m_iFilterOther == 2)))
						return false;
				}
			}
		}
	}

	// default behavioure
	return CSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}