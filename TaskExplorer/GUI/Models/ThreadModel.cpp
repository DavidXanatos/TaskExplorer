#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ThreadModel.h"
#include "../../../MiscHelpers/Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinThread.h"
#endif

CThreadModel::CThreadModel(QObject *parent)
:CListItemModel(parent)
{
	m_bExtThreadId = false;
}

CThreadModel::~CThreadModel()
{
}

void CThreadModel::Sync(QMap<quint64, CThreadPtr> ThreadList)
{
	QList<SListNode*> New;
	QHash<QVariant, SListNode*> Old = m_Map;

	bool bClearZeros = theConf->GetBool("Options/ClearZeros", true);

	foreach (const CThreadPtr& pThread, ThreadList)
	{
		QVariant ID = pThread->GetThreadId();

		int Row = -1;
		QHash<QVariant, SListNode*>::iterator I = Old.find(ID);
		SThreadNode* pNode = I != Old.end() ? static_cast<SThreadNode*>(I.value()) : NULL;
		if(!pNode)
		{
			pNode = static_cast<SThreadNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pThread = pThread;
			pNode->IsBold = pThread->IsMainThread();
			New.append(pNode);
		}
		else
		{
			I.value() = NULL;
			Row = GetRow(pNode);
		}

#ifdef WIN32
		CWinThread* pWinThread = qobject_cast<CWinThread*>(pThread.data());
#endif

		int Col = 0;
		bool State = false;
		int Changed = 0;

		// Note: icons are loaded asynchroniusly
		if (m_bUseIcons && !pNode->Icon.isValid() && m_Columns.contains(eThread))
		{
			CProcessPtr pProcess = pNode->pThread->GetProcess().staticCast<CProcessInfo>();
			CModulePtr pModule = pProcess ? pProcess->GetModuleInfo() : CModulePtr();
			if (pModule)
			{
				QPixmap Icon = pModule->GetFileIcon();
				if (!Icon.isNull()) {
					Changed = 1; // set change for first column
					pNode->Icon = Icon;
				}
			}
		}

		int RowColor = CTaskExplorer::eNone;
		if (pThread->IsMarkedForRemoval() && CTaskExplorer::UseListColor(CTaskExplorer::eToBeRemoved))			RowColor = CTaskExplorer::eToBeRemoved;
		else if (pThread->IsNewlyCreated() && CTaskExplorer::UseListColor(CTaskExplorer::eAdded))				RowColor = CTaskExplorer::eAdded;
#ifdef WIN32
		else if (pWinThread->IsCriticalThread() && CTaskExplorer::UseListColor(CTaskExplorer::eIsProtected))	RowColor = CTaskExplorer::eIsProtected;
		else if (pWinThread->IsGuiThread() && CTaskExplorer::UseListColor(CTaskExplorer::eGuiThread))			RowColor = CTaskExplorer::eGuiThread;
#endif
		
		if (pNode->iColor != RowColor) {
			pNode->iColor = RowColor;
			pNode->Color = CTaskExplorer::GetListColor(RowColor);
			Changed = 2;
		}

		if (pNode->IsGray != pThread->IsSuspended())
		{
			pNode->IsGray = pThread->IsSuspended();
			Changed = 2;
		}

		STaskStats CpuStats = pThread->GetCpuStats();
		SIOStatsEx IoStats = pThread->GetIoStats();

		for(int section = 0; section < columnCount(); section++)
		{
			if (!m_Columns.contains(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch(section)
			{
				case eThread:				Value = pThread->GetThreadId(); break;
				case eCPU_History:
				case eCPU:					Value = CpuStats.CpuUsage; break;
#ifdef WIN32
				case eStartAddress:			Value = pWinThread->GetStartAddressString(); break;
				case eService:				Value = pWinThread->GetServiceName(); break;
				case eName:					Value = pWinThread->GetThreadName(); break;
				case eType:					Value = pWinThread->IsMainThread() ? 2 : pWinThread->IsGuiThread() ? 1 : 0; break;
#endif
				case eCreated:				Value = pThread->GetCreateTimeStamp(); break;
#ifdef WIN32
				case eStartModule:			Value = pWinThread->GetStartAddressFileName(); break;
#endif
				case eContextSwitches:		Value = CpuStats.ContextSwitchesDelta.Value; break;
				case eContextSwitchesDelta:	Value = CpuStats.ContextSwitchesDelta.Delta; break;
                case ePriority:				Value = (quint32)pThread->GetPriority(); break;
                case eBasePriority:			Value = (quint32)pThread->GetBasePriority(); break;
                case ePagePriority:			Value = (quint32)pThread->GetPagePriority(); break;
                case eIOPriority:			Value = (quint32)pThread->GetIOPriority(); break;
				case eCycles:				Value = CpuStats.CycleDelta.Value; break;
				case eCyclesDelta:			Value = CpuStats.CycleDelta.Delta; break;
				case eState:				Value = pThread->GetWaitState(); break;
				case eKernelTime:			Value = CpuStats.CpuKernelDelta.Value;
				case eUserTime:				Value = CpuStats.CpuUserDelta.Value;
#ifdef WIN32
				case eIdealProcessor:		Value = pWinThread->GetIdealProcessor(); break;
				case eImpersonation:		Value = pWinThread->IsSandboxed() ? (pWinThread->HasToken2() ? 1 : 0) : (int)pWinThread->GetTokenState(); break;
				case eCritical:				Value = pWinThread->IsCriticalThread() ? tr("Critical") : ""; break;
				case eAppDomain:			Value = pWinThread->GetAppDomain(); break;

				case ePendingIRP:			Value = pWinThread->HasPendingIrp(); break;
				case eLastSystemCall:		Value = pWinThread->GetLastSysCallInfoString(); break;
				case eLastStatusCode:		Value = pWinThread->GetLastSysCallStatusString(); break;
				case eCOM_Apartment:		Value = pWinThread->GetApartmentState(); break;
				case eFiber:				Value = pWinThread->IsFiber(); break;
				case ePriorityBoost:		Value = pWinThread->HasPriorityBoost(); break;
				case eStackUsage:			Value = pWinThread->GetStackUsagePercent(); break;
				//case eWaitTime:				
				case eIO_Reads:				Value = IoStats.ReadCount; break;
				case eIO_Writes:			Value = IoStats.WriteCount; break;
				case eIO_Other:				Value = IoStats.OtherCount; break;
				case eIO_ReadBytes:			Value = IoStats.ReadRaw; break;
				case eIO_WriteBytes:		Value = IoStats.WriteRaw; break;
				case eIO_OtherBytes:		Value = IoStats.OtherRaw; break;		
				//case eIO_TotalBytes:		Value = ; break;
				case eIO_ReadsDelta:		Value = IoStats.ReadDelta.Delta; break;
				case eIO_WritesDelta:		Value = IoStats.WriteDelta.Delta; break;
				case eIO_OtherDelta:		Value = IoStats.OtherDelta.Delta; break;
					//case eIO_TotalDelta:		Value = ; break;
				case eIO_ReadBytesDelta:	Value = IoStats.ReadRawDelta.Delta; break;
				case eIO_WriteBytesDelta:	Value = IoStats.WriteRawDelta.Delta; break;
				case eIO_OtherBytesDelta:	Value = IoStats.OtherRawDelta.Delta; break;
					//case eIO_TotalBytesDelta:	Value = ; break;
				case eIO_ReadRate:			Value = IoStats.ReadRate.Get(); break;
				case eIO_WriteRate:			Value = IoStats.WriteRate.Get(); break;
				case eIO_OtherRate:			Value = IoStats.OtherRate.Get(); break;
					//case eIO_TotalRate:		Value = ; break;
				//case eTID_LXSS:		
				case ePowerThrottling:		Value = pWinThread->IsPowerThrottled(); break;
				//case eContainerID:			
#endif
			}

			SThreadNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
					case eThread:				if (m_bExtThreadId)
													ColValue.Formated = tr("%1 (%2): %3").arg(pThread->GetName()).arg(theGUI->FormatID(pThread->GetProcessId())).arg(theGUI->FormatID(pThread->GetThreadId()));
												else
													ColValue.Formated = theGUI->FormatID(pThread->GetThreadId());
												break;
					//case eThread:				ColValue.Formated = "0x" + QString::number(pThread->GetThreadId()); break;
					case eCPU:					ColValue.Formated = (!bClearZeros || CpuStats.CpuUsage > 0.00004) ? QString::number(CpuStats.CpuUsage*100, 10, 2) + "%" : ""; break;

					case ePriority:				ColValue.Formated = pThread->GetPriorityString(); break;
					case eBasePriority:			ColValue.Formated = pThread->GetBasePriorityString(); break;
					case ePagePriority:			ColValue.Formated = pThread->GetPagePriorityString(); break;
					case eIOPriority:			ColValue.Formated = pThread->GetIOPriorityString(); break;

					case eCreated:				ColValue.Formated = QDateTime::fromSecsSinceEpoch(Value.toULongLong()/1000).toString("dd.MM.yyyy hh:mm:ss"); break;
#ifdef WIN32
					case eType:					ColValue.Formated = pWinThread->GetTypeString(); break;
#endif
					case eState:				ColValue.Formated = pThread->GetStateString(); break;
					case eCycles:
					case eContextSwitches:
												ColValue.Formated = FormatNumber(Value.toULongLong()); break;
					case eCyclesDelta:
					case eContextSwitchesDelta:
												ColValue.Formated = FormatNumberEx(Value.toULongLong(), bClearZeros); break;
#ifdef WIN32
					case eImpersonation:		ColValue.Formated = pWinThread->GetTokenStateString(); break;

					case ePendingIRP:			ColValue.Formated = pWinThread->HasPendingIrp() ? tr("Yes") : ""; break;
					case eFiber:				ColValue.Formated = pWinThread->IsFiber() ? tr("Yes") : ""; break;
					case ePriorityBoost:		ColValue.Formated = pWinThread->HasPriorityBoost() ? tr("Yes") : ""; break;
					case eStackUsage:			ColValue.Formated = pWinThread->GetStackUsageString(); break;
					case ePowerThrottling:		ColValue.Formated = pWinThread->IsPowerThrottled() ? tr("Yes") : ""; break;

					case eCOM_Apartment:		ColValue.Formated = pWinThread->GetApartmentStateString(); break;

					case eIO_Reads:
					case eIO_Writes:
					case eIO_Other:
												ColValue.Formated = FormatNumber(Value.toULongLong()); break;

					case eIO_ReadsDelta:
					case eIO_WritesDelta:
					case eIO_OtherDelta:
												ColValue.Formated = FormatNumberEx(Value.toULongLong(), bClearZeros); break;

					case eIO_ReadBytes:
					case eIO_WriteBytes:
					case eIO_OtherBytes:
												if(Value.type() != QVariant::String) ColValue.Formated = FormatSize(Value.toULongLong()); break; 

					case eIO_ReadBytesDelta:
					case eIO_WriteBytesDelta:
					case eIO_OtherBytesDelta:
												if(Value.type() != QVariant::String) ColValue.Formated = FormatSizeEx(Value.toULongLong(), bClearZeros); break; 

					//case eIO_TotalRate:
					case eIO_ReadRate:
					case eIO_WriteRate:
					case eIO_OtherRate:
												if(Value.type() != QVariant::String) ColValue.Formated = FormatRateEx(Value.toULongLong(), bClearZeros); break; 
#endif
				}
			}

			if(State != (Changed != 0))
			{
				if(State && Row != -1)
					emit dataChanged(createIndex(Row, Col), createIndex(Row, section-1));
				State = (Changed != 0);
				Col = section;
			}
			if (Changed == 1)
				Changed = 0;
		}
		if(State && Row != -1)
			emit dataChanged(createIndex(Row, Col, pNode), createIndex(Row, columnCount()-1, pNode));

	}

	CListItemModel::Sync(New, Old);
}

CThreadPtr CThreadModel::GetThread(const QModelIndex &index) const
{
	if (!index.isValid())
        return CThreadPtr();

	SThreadNode* pNode = static_cast<SThreadNode*>(index.internalPointer());
	return pNode->pThread;
}

int CThreadModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CThreadModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eThread:				return tr("Thread");
			case eCPU_History:			return tr("CPU graph");
			case eCPU:					return tr("CPU");
			case eCyclesDelta:			return tr("Cycles delta");
#ifdef WIN32
			case eStartAddress:			return tr("Start address");
			case eService:				return tr("Service");
			case eName:					return tr("Name");
			case eType:					return tr("Type");
#endif
			case eCreated:				return tr("Created");
#ifdef WIN32
			case eStartModule:			return tr("Start module");
#endif
			case eContextSwitches:		return tr("Context switches");
			case eContextSwitchesDelta:	return tr("Context switches delta");
			case ePriority:				return tr("Priority");
			case eBasePriority:			return tr("Base priority");
			case ePagePriority:			return tr("Page priority");
			case eIOPriority:			return tr("I/O priority");
			case eCycles:				return tr("Cycles");
			case eState:				return tr("State");
			case eKernelTime:			return tr("Kernel time");
			case eUserTime:				return tr("User time");
#ifdef WIN32
			case eIdealProcessor:		return tr("Ideal processor");
			case eCritical:				return tr("Critical");
			case eImpersonation:		return tr("Impersonation Token");
			case eAppDomain:			return tr("App Domain");

			case ePendingIRP:			return tr("Pending IRP");	
			case eLastSystemCall:		return tr("Last system call");
			case eLastStatusCode:		return tr("Last status code");
			case eCOM_Apartment:		return tr("COM apartment");
			case eFiber:				return tr("Fiber");	
			case ePriorityBoost:		return tr("Priority boost");
			case eStackUsage:			return tr("Stack usage");
			case eWaitTime:				return tr("Wait time");
			case eIO_Reads:				return tr("I/O reads");
			case eIO_Writes:			return tr("I/O writes");
			case eIO_Other:				return tr("I/O other");
			case eIO_ReadBytes:			return tr("I/O read bytes");
			case eIO_WriteBytes:		return tr("I/O write bytes");
			case eIO_OtherBytes:		return tr("I/O other bytes");
			//case eIO_TotalBytes:		return tr("I/O total bytes");
			case eIO_ReadsDelta:		return tr("I/O reads delta");
			case eIO_WritesDelta:		return tr("I/O writes delta");
			case eIO_OtherDelta:		return tr("I/O other delta");
			//case eIO_TotalDelta:		return tr("I/O total delta");
			case eIO_ReadBytesDelta:	return tr("I/O read bytes delta");
			case eIO_WriteBytesDelta:	return tr("I/O write bytes delta");
			case eIO_OtherBytesDelta:	return tr("I/O other bytes delta");
			//case eIO_TotalBytesDelta:	return tr("I/O total bytes delta");
			case eIO_ReadRate:			return tr("I/O read rate");
			case eIO_WriteRate:			return tr("I/O write rate");
			case eIO_OtherRate:			return tr("I/O other rate");
			//case eIO_TotalRate:		return tr("I/O total rate");
			//case eTID_LXSS:				return tr("LXSS TID");
			case ePowerThrottling:		return tr("Power throttling");
			//case eContainerID:			return tr("Container ID");

#endif
		}
	}
    return QVariant();
}

QVariant CThreadModel::GetDefaultIcon() const 
{ 
	return g_ExeIcon;
}
