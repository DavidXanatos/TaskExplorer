#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ProcessModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinProcess.h"
#include "../../API/Windows/WinToken.h"
#include "../../API/Windows/WinModule.h"
#include "../../API/Windows/WindowsAPI.h"
#endif


CProcessModel::CProcessModel(QObject *parent)
:CTreeItemModel(parent)
{
	m_bUseIcons = true;
	m_bUseDescr = true;

	m_Columns.insert(eProcess);
}

CProcessModel::~CProcessModel()
{
}

QList<QVariant> CProcessModel::MakeProcPath(const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList)
{
	QList<QVariant> Path;

	quint64 ParentID = pProcess->GetParentId();
	CProcessPtr pParent = ProcessList.value(ParentID);

	if (!pParent.isNull())
	{
#ifdef WIN32
		if (!qobject_cast<CWinProcess*>(pProcess.data())->ValidateParent(pParent.data()))
			return Path;
#endif

		Path = MakeProcPath(pParent, ProcessList);
		Path.append(ParentID);
	}

	return Path;
}

bool CProcessModel::TestProcPath(const QList<QVariant>& Path, const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList, int Index)
{
	quint64 ParentID = pProcess->GetParentId();
	CProcessPtr pParent = ProcessList.value(ParentID);

	if (!pParent.isNull())
	{
#ifdef WIN32
		if (!qobject_cast<CWinProcess*>(pProcess.data())->ValidateParent(pParent.data()))
			return Path.size() == Index;
#endif

		if(Index >= Path.size() || Path[Path.size() - Index - 1] != ParentID)
			return false;

		return TestProcPath(Path, pParent, ProcessList, Index + 1);
	}

	return Path.size() == Index;
}

void CProcessModel::Sync(QMap<quint64, CProcessPtr> ProcessList)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	bool bShow32 = theConf->GetBool("Options/Show32", true);
	bool bClearZeros = theConf->GetBool("Options/ClearZeros", true);
	int iHighlightMax = theConf->GetInt("Options/HighLoadHighlightCount", 5);
	time_t curTime = GetTime();

#ifdef WIN32
	bool HasExtProcInfo = ((CWindowsAPI*)theAPI)->HasExtProcInfo();
	bool IsMonitoringETW = ((CWindowsAPI*)theAPI)->IsMonitoringETW();
#endif

	QVector<QList<QPair<quint64, SProcessNode*> > > Highlights;
	if(iHighlightMax > 0)
		Highlights.resize(columnCount());

	foreach (const CProcessPtr& pProcess, ProcessList)
	{
		QVariant ID = pProcess->GetProcessId();

		QModelIndex Index;
		
		SProcessNode* pNode = static_cast<SProcessNode*>(Old.value(ID));
		if(!pNode || (m_bTree ? !TestProcPath(pNode->Path, pProcess, ProcessList) : !pNode->Path.isEmpty()))
		{
			pNode = static_cast<SProcessNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			if (m_bTree)
				pNode->Path = MakeProcPath(pProcess, ProcessList);
			pNode->pProcess = pProcess;
			New[pNode->Path].append(pNode);
		}
		else
		{
			Old[ID] = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));
		
#ifdef WIN32
		QSharedPointer<CWinProcess> pWinProc = pProcess.objectCast<CWinProcess>();
		CWinTokenPtr pToken = pWinProc->GetToken();
		CModulePtr pModule = pProcess->GetModuleInfo();
		QSharedPointer<CWinModule> pWinModule = pModule.objectCast<CWinModule>();
#endif

		int Col = 0;
		bool State = false;
		int Changed = 0;

		// Note: icons are loaded asynchroniusly
		if (!pNode->Icon.isValid())
		{
			QPixmap Icon = pModule->GetFileIcon();
			if (!Icon.isNull()) {
				Changed = 1; // set change for first column
				pNode->Icon = Icon;
			}
		}

		int RowColor = CTaskExplorer::eNone;
		if (pProcess->IsMarkedForRemoval())			RowColor = CTaskExplorer::eToBeRemoved;
		else if (pProcess->IsNewlyCreated())		RowColor = CTaskExplorer::eAdded;
#ifdef WIN32
		else if (pWinProc->TokenHasChanged())		RowColor = CTaskExplorer::eDange;
#endif
		else if (pProcess->IsServiceProcess())		RowColor = CTaskExplorer::eService;
		else if (pProcess->IsSystemProcess())		RowColor = CTaskExplorer::eSystem;
		else if (pProcess->IsElevated())			RowColor = CTaskExplorer::eElevated;
#ifdef WIN32
		else if (pWinProc->IsPicoProcess())			RowColor = CTaskExplorer::ePico;
		else if (pWinProc->IsImmersiveProcess())	RowColor = CTaskExplorer::eImmersive;
		else if (pWinProc->IsNetProcess())			RowColor = CTaskExplorer::eDotNet;
#endif
		else if (pProcess->IsUserProcess())			RowColor = CTaskExplorer::eUser;
#ifdef WIN32
		//else if (pWinProc->IsJobProcess())			RowColor = CTaskExplorer::eJob;
		else if (pWinProc->IsInJob())				RowColor = CTaskExplorer::eJob;
#endif
		
		if (pNode->iColor != RowColor) {
			pNode->iColor = RowColor;
			pNode->Color = CTaskExplorer::GetListColor(RowColor);
			Changed = 2;
		}

		if (pNode->IsGray != pProcess->IsSuspended())
		{
			pNode->IsGray = pProcess->IsSuspended();
			Changed = 2;
		}

		pNode->Bold.clear();

		SProcStats Stats = pProcess->GetStats();
		STaskStatsEx CpuStats = pProcess->GetCpuStats();

		for(int section = eProcess; section < columnCount(); section++)
		{
			if (!m_Columns.contains(section))
				continue; // ignore columns which are hidden

			quint64 CurIntValue = -1;

			QVariant Value;
			switch(section)
			{
				case eProcess:		
					if (m_bUseDescr)
					{
						QString Descr = pModule->GetFileInfo("Description");
						if (!Descr.isEmpty())
						{
							Value =  Descr + " (" + pProcess->GetName() + 
#ifdef WIN32
								(bShow32 && pWinProc->IsWoW64() ? " *32" : "") + 
#endif
								")";
							break;
						}
					}
#ifdef WIN32
					if (bShow32 && pWinProc->IsWoW64()) {
						Value = pProcess->GetName() + " *32";
						break;
					}
#endif
											Value = pProcess->GetName(); break;
				case ePID:					Value = (qint64)pProcess->GetProcessId(); break;
				case eCPU_History:
				case eCPU:					Value = CpuStats.CpuUsage; CurIntValue = 10000 * Value.toDouble(); break;
				case eIO_History:			Value = qMax(Stats.Disk.ReadRate.Get(), Stats.Io.ReadRate.Get()) + qMax(Stats.Disk.WriteRate.Get(), Stats.Io.WriteRate.Get()) + Stats.Io.OtherRate.Get(); break;
				case eIO_TotalRate:			Value = CurIntValue = Stats.Io.ReadRate.Get() + Stats.Io.WriteRate.Get() + Stats.Io.OtherRate.Get(); break;
				case eStaus:				Value = pProcess->GetStatusString(); break;
				case ePrivateBytes:			Value = CurIntValue = pProcess->GetWorkingSetPrivateSize(); break;
				case eUserName:				Value = pProcess->GetUserName(); break;
#ifdef WIN32
				case eServices:				Value = pWinProc->GetServiceList().join(tr(", ")); break;
				case eDescription:			Value = pModule->GetFileInfo("Description"); break;
				case eCompanyName:			Value = pModule->GetFileInfo("CompanyName"); break;
				case eVersion:				Value = pModule->GetFileInfo("FileVersion"); break;
#endif

				case eGPU_History:			Value = pProcess->GetGpuUsage(); break;
				case eVMEM_History:			Value = qMax(pProcess->GetGpuDedicatedUsage(),pProcess->GetGpuSharedUsage()); break;

				case eGPU_Usage:			Value = pProcess->GetGpuUsage(); CurIntValue = 10000 * Value.toDouble(); break;
				case eGPU_Shared:			Value = CurIntValue = pProcess->GetGpuSharedUsage(); break;
				case eGPU_Dedicated:		Value = CurIntValue = pProcess->GetGpuDedicatedUsage(); break;
				case eGPU_Adapter:			Value = pProcess->GetGpuAdapter(); break;

				case eFileName:				Value = pProcess->GetFileName(); break;
				case eCommandLine:			Value = pProcess->GetCommandLineStr(); break;
				case ePeakPrivateBytes:		Value = CurIntValue = pProcess->GetPeakPrivateBytes(); break;
				case eMEM_History:
				case eWorkingSet:			Value = CurIntValue = pProcess->GetWorkingSetSize(); break;
				case ePeakWS:				Value = CurIntValue = pProcess->GetPeakWorkingSetSize(); break;
				case ePrivateWS:			Value = CurIntValue = pProcess->GetPrivateWorkingSetSize(); break;
				case eSharedWS:				Value = CurIntValue = pProcess->GetSharedWorkingSetSize(); break;
				case eShareableWS:			Value = CurIntValue = pProcess->GetShareableWorkingSetSize(); break;
				case eVirtualSize:			Value = CurIntValue = pProcess->GetVirtualSize(); break;
				case ePeakVirtualSize:		Value = CurIntValue = pProcess->GetPeakVirtualSize(); break;
				case eSessionID:			Value = pProcess->GetSessionID(); break;
				case ePriorityClass:		Value = (quint32)pProcess->GetPriority(); break;
				case eBasePriority:			Value = (quint32)pProcess->GetBasePriority(); break;

				case eThreads:				Value = CurIntValue = (quint32)pProcess->GetNumberOfThreads(); break;
				case ePeakThreads:			Value = CurIntValue = (quint32)pProcess->GetPeakNumberOfThreads(); break;
				case eHandles:				Value = CurIntValue = (quint32)pProcess->GetNumberOfHandles(); break;
				case ePeakHandles:			Value = CurIntValue = (quint32)pProcess->GetPeakNumberOfHandles(); break;
#ifdef WIN32
				case eWND_Handles:			Value = CurIntValue = (quint32)pWinProc->GetWndHandles(); break;
				case eGDI_Handles:			Value = CurIntValue = (quint32)pWinProc->GetGdiHandles(); break;
				case eUSER_Handles:			Value = CurIntValue = (quint32)pWinProc->GetUserHandles(); break;
				case eIntegrity:			Value = pToken ? pToken->GetIntegrityLevel() : 0; break;
#endif
				case eIO_Priority:			Value = (quint32)pProcess->GetIOPriority(); break;
				case ePagePriority:			Value = (quint32)pProcess->GetPagePriority(); break;
				case eStartTime:			Value = pProcess->GetCreateTimeStamp(); break;
				case eTotalCPU_Time:		Value = CurIntValue = (CpuStats.CpuKernelDelta.Value + CpuStats.CpuUserDelta.Value) / CPU_TIME_DIVIDER; break;
				case eKernelCPU_Time:		Value = CurIntValue = CpuStats.CpuKernelDelta.Value / CPU_TIME_DIVIDER; break;
				case eUserCPU_Time:			Value = CurIntValue = CpuStats.CpuUserDelta.Value / CPU_TIME_DIVIDER; break;
#ifdef WIN32
				case eVerificationStatus:	Value = pWinModule->GetVerifyResultString(); break;
				case eVerifiedSigner:		Value = pWinModule->GetVerifySignerName(); break;
				case eASLR:					Value = pWinModule->GetASLRString(); break;
#endif
				case eUpTime:				Value = pProcess->GetCreateTimeStamp() != 0 ? (curTime - pProcess->GetCreateTimeStamp() / 1000) : 0; break; // we must update the value to refresh the display
				case eArch:					Value = pProcess->GetArchString(); break;
#ifdef WIN32
				case eElevation:			Value = pToken ? pToken->GetElevationString() : ""; break;
#else
				case eElevation:			break; // linux....
#endif
#ifdef WIN32
				case eWindowTitle:			Value = pWinProc->GetWindowTitle();  break;
				case eWindowStatus:			Value = pWinProc->GetWindowStatusString(); break;
#endif
				case eCycles:				Value = CurIntValue = CpuStats.CycleDelta.Value; break;
				case eCyclesDelta:			Value = CurIntValue = CpuStats.CycleDelta.Delta; break;
#ifdef WIN32
				case eDEP:					Value = pWinProc->GetDEPStatusString(); break;
				case eVirtualized:			Value = pToken ? pToken->GetVirtualizationString() : ""; break;
#endif
				case eContextSwitches:		Value = CurIntValue = CpuStats.ContextSwitchesDelta.Value; break;
				case eContextSwitchesDelta:	Value = CurIntValue = CpuStats.ContextSwitchesDelta.Delta; break;
				case ePageFaults:			Value = CurIntValue = CpuStats.PageFaultsDelta.Value; break;
				case ePageFaultsDelta:		Value = CurIntValue = CpuStats.PageFaultsDelta.Delta; break;
				case eHardFaults:			Value = CurIntValue = CpuStats.HardFaultsDelta.Value; break;
				case eHardFaultsDelta:		Value = CurIntValue = CpuStats.HardFaultsDelta.Delta; break;

				// IO
				case eIO_Reads:				Value = CurIntValue = Stats.Io.ReadCount; break;
				case eIO_Writes:			Value = CurIntValue = Stats.Io.WriteCount; break;
				case eIO_Other:				Value = CurIntValue = Stats.Io.OtherCount; break;
				case eIO_ReadBytes:			Value = CurIntValue = Stats.Io.ReadRaw; break;
				case eIO_WriteBytes:		Value = CurIntValue = Stats.Io.WriteRaw; break;
				case eIO_OtherBytes:		Value = CurIntValue = Stats.Io.OtherRaw; break;
				//case eIO_TotalBytes:		Value = CurIntValue = ; break;
				case eIO_ReadsDelta:		Value = CurIntValue = Stats.Io.ReadDelta.Delta; break;
				case eIO_WritesDelta:		Value = CurIntValue = Stats.Io.WriteDelta.Delta; break;
				case eIO_OtherDelta:		Value = CurIntValue = Stats.Io.OtherDelta.Delta; break;
				//case eIO_TotalDelta:		Value = CurIntValue = ; break;
				case eIO_ReadBytesDelta:	Value = CurIntValue = Stats.Io.ReadRawDelta.Delta; break;
				case eIO_WriteBytesDelta:	Value = CurIntValue = Stats.Io.WriteRawDelta.Delta; break;
				case eIO_OtherBytesDelta:	Value = CurIntValue = Stats.Io.OtherRawDelta.Delta; break;
				//case eIO_TotalBytesDelta:	Value = CurIntValue = ; break;
				case eIO_ReadRate:			Value = CurIntValue = Stats.Io.ReadRate.Get(); break;
				case eIO_WriteRate:			Value = CurIntValue = Stats.Io.WriteRate.Get(); break;
				case eIO_OtherRate:			Value = CurIntValue = Stats.Io.OtherRate.Get(); break;
				//case eIO_TotalRate:		Value = CurIntValue = ; break;

#ifdef WIN32
				case eOS_Context:			Value = (quint32)pWinProc->GetOsContextVersion(); break;
#endif
				case ePagedPool:			Value = CurIntValue = pProcess->GetPagedPool(); break;
				case ePeakPagedPool:		Value = CurIntValue = pProcess->GetPeakPagedPool(); break;
				case eNonPagedPool:			Value = CurIntValue = pProcess->GetNonPagedPool(); break;
				case ePeakNonPagedPool:		Value = CurIntValue = pProcess->GetPeakNonPagedPool(); break;
				case eMinimumWS:			Value = /*CurIntValue =*/ pProcess->GetMinimumWS(); break;
				case eMaximumWS:			Value = /*CurIntValue =*/ pProcess->GetMaximumWS(); break;
				case ePrivateBytesDelta:	Value = /*CurIntValue =*/ CpuStats.PrivateBytesDelta.Delta; break;
				case eSubsystem:			Value = (quint32)pProcess->GetSubsystem(); break;
#ifdef WIN32
				case ePackageName:			Value = pWinProc->GetPackageName(); break;
				case eAppID:				Value = pWinProc->GetAppID();  break;
				case eDPI_Awareness:		Value = (int)pWinProc->GetDPIAwareness(); break;
				case eCF_Guard:				Value = pWinProc->GetCFGuardString(); break;
				case eTimeStamp:			Value = pWinModule->GetTimeStamp(); break;
#endif
				case eFileModifiedTime:		Value = pModule->GetModificationTime(); break;
				case eFileSize:				Value = pModule->GetFileSize(); break;
#ifdef WIN32
				case eJobObjectID:			Value = pWinProc->GetJobObjectID(); break;
				case eProtection:			Value = (int)pWinProc->GetProtection(); break;
				case eDesktop:				Value = pWinProc->GetDesktopInfo(); break;
				case eCritical:				Value = pWinProc->IsCriticalProcess() ? tr("Critical") : ""; break;

				case eRunningTime:			Value = pWinProc->GetUpTime(); break;
				case eSuspendedTime:		Value = pWinProc->GetSuspendTime(); break;
				case eHangCount:			Value = pWinProc->GetHangCount(); break;
				case eGhostCount:			Value = pWinProc->GetGhostCount(); break;
#endif

				// Network IO
				case eNET_History:
				case eNet_TotalRate:		Value = CurIntValue = Stats.Net.ReceiveRate.Get() + Stats.Net.SendRate.Get(); break; 
				case eReceives:				Value = CurIntValue = Stats.Net.ReceiveCount; break; 
				case eSends:				Value = CurIntValue = Stats.Net.SendCount; break; 
				case eReceiveBytes:			Value = CurIntValue = Stats.Net.ReceiveRaw; break; 
				case eSendBytes:			Value = CurIntValue = Stats.Net.SendRaw; break; 
				//case eTotalBytes:			Value = CurIntValue = ; break; 
				case eReceivesDelta:		Value = CurIntValue = Stats.Net.ReceiveDelta.Delta; break; 
				case eSendsDelta:			Value = CurIntValue = Stats.Net.SendDelta.Delta; break; 
				case eReceiveBytesDelta:	Value = CurIntValue = Stats.Net.ReceiveRawDelta.Delta; break; 
				case eSendBytesDelta:		Value = CurIntValue = Stats.Net.SendRawDelta.Delta; break; 
				//case eTotalBytesDelta:	Value = CurIntValue = ; break; 
				case eReceiveRate:			Value = CurIntValue = Stats.Net.ReceiveRate.Get(); break; 
				case eSendRate:				Value = CurIntValue = Stats.Net.SendRate.Get(); break; 

				// Disk IO
				case eDisk_TotalRate:		Value = CurIntValue = Stats.Disk.ReadRate.Get() + Stats.Disk.WriteRate.Get(); break; 
				case eReads:				Value = CurIntValue = Stats.Disk.ReadCount; break;
				case eWrites:				Value = CurIntValue = Stats.Disk.WriteCount; break;
				case eReadBytes:			Value = CurIntValue = Stats.Disk.ReadRaw; break;
				case eWriteBytes:			Value = CurIntValue = Stats.Disk.WriteRaw; break;
				//case eTotalBytes:			Value = CurIntValue = ; break;
				case eReadsDelta:			Value = CurIntValue = Stats.Disk.ReadDelta.Delta; break;
				case eWritesDelta:			Value = CurIntValue = Stats.Disk.WriteDelta.Delta; break;
				case eReadBytesDelta:		Value = CurIntValue = Stats.Disk.ReadRawDelta.Delta; break;
				case eWriteBytesDelta:		Value = CurIntValue = Stats.Disk.WriteRawDelta.Delta; break;
				//case eTotalBytesDelta:	Value = CurIntValue = ; break;
				case eReadRate:				Value = CurIntValue = Stats.Disk.ReadRate.Get(); break;
				case eWriteRate:			Value = CurIntValue = Stats.Disk.WriteRate.Get(); break;
			}

			SProcessNode::SValue& ColValue = pNode->Values[section];

			bool bChanged = false;
#ifdef WIN32
			if (!IsMonitoringETW // Note: this columns are not available without ETW being enabled
			 && (section == eNET_History || (section >= eNet_TotalRate && section <= eSendRate)
			 || ((section >= eDisk_TotalRate && section <= eWriteRate) && !HasExtProcInfo)))
				Value = tr("N/A");
			else
#endif
			if (CurIntValue == -1)
			{
				CurIntValue = 0;
				bChanged = (ColValue.Raw != Value);
			}
			else if (ColValue.Raw.isNull())
				bChanged = true;
			else // if the change is less than 0.01%, i.e. in unit notation the difference will be not displayed, dont issue an update
			{
				// Note: this savec 20% of CPU load in the debug build

				quint64 OldIntValue;
				switch (section)
				{
					case eCPU:
					case eGPU_Usage:
						OldIntValue = ColValue.Raw.toDouble() * 10000; 
						break;
					default:
						OldIntValue = ColValue.Raw.toULongLong();
				}
				
				if (CurIntValue > OldIntValue)
					bChanged = (10000 * (CurIntValue - OldIntValue) / CurIntValue) > 0;
				else if (OldIntValue > CurIntValue)
					bChanged = (10000 * (OldIntValue - CurIntValue) / OldIntValue) > 0;
			}

			if (bChanged)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch(section)
				{
					case ePID:				if (Value.toLongLong() < 0) ColValue.Formated = ""; break;

					case eCPU:
					case eGPU_Usage:
											ColValue.Formated = (!bClearZeros || CpuStats.CpuUsage > 0.00004) ? QString::number(CpuStats.CpuUsage*100, 10, 2) + "%" : ""; break;

					case ePrivateBytes:		
					case ePeakPrivateBytes:
					case eWorkingSet:
					case ePeakWS:
					case ePrivateWS:
					case eSharedWS:
					case eShareableWS:
					case eVirtualSize:
					case ePeakVirtualSize:
					case ePagedPool:
					case ePeakPagedPool:
					case eNonPagedPool:
					case ePeakNonPagedPool:
					case eMinimumWS:
					case eMaximumWS:

					case eFileSize:
											ColValue.Formated = FormatSize(Value.toULongLong()); break;

					// since not all programs use GPU memory, make this value clearable
					case eGPU_Dedicated:
					case eGPU_Shared:
											ColValue.Formated = FormatSizeEx(Value.toULongLong(), bClearZeros); break;

					case ePrivateBytesDelta:
					{
						qint64 iDelta = Value.toLongLong();
						if (iDelta < 0)
							ColValue.Formated = "-" + FormatSize(iDelta * -1);
						else if (iDelta > 0)
							ColValue.Formated = "+" + FormatSize(iDelta);
						else if (bClearZeros)
							ColValue.Formated = QString();
						else
							ColValue.Formated = "0";
						break;
					}

					case eCycles:
					case eContextSwitches:
					case ePageFaults:
					case eHardFaults:
					case eIO_Reads:
					case eIO_Writes:
					case eIO_Other:
					case eReceives:
					case eSends:
					case eReads:
					case eWrites:
										ColValue.Formated = FormatNumber(Value.toULongLong()); break;

					// since not all programs use GUI resources, make this value clearable
					case eWND_Handles:
					case eGDI_Handles:
					case eUSER_Handles:
					case eHangCount:
					case eGhostCount:

					case eCyclesDelta:
					case eContextSwitchesDelta:
					case ePageFaultsDelta:
					case eHardFaultsDelta:
					case eIO_ReadsDelta:
					case eIO_WritesDelta:
					case eIO_OtherDelta:
					case eReceivesDelta:
					case eSendsDelta:
					case eReadsDelta:
					case eWritesDelta:
											ColValue.Formated = FormatNumberEx(Value.toULongLong(), bClearZeros); break;

					case eStartTime:		ColValue.Formated = QDateTime::fromTime_t(Value.toULongLong()/1000).toString("dd.MM.yyyy hh:mm:ss"); break;
#ifdef WIN32
					case eTimeStamp:
#endif
					case eFileModifiedTime:
											if (Value.toULongLong() != 0) ColValue.Formated = QDateTime::fromTime_t(Value.toULongLong()).toString("dd.MM.yyyy hh:mm:ss"); break;
					case eUpTime:			
					case eRunningTime:			
					case eSuspendedTime:
											ColValue.Formated = (Value.toULongLong() == 0) ? QString() : FormatTime(Value.toULongLong()); break;
					case eTotalCPU_Time:
					case eKernelCPU_Time:
					case eUserCPU_Time:
											ColValue.Formated = FormatTime(Value.toULongLong()); break;
					case ePriorityClass:	ColValue.Formated = pProcess->GetPriorityString(); break;
#ifdef WIN32
					case eIntegrity:		ColValue.Formated = pToken ? pToken->GetIntegrityString() : "";  break;
#endif
					case eSubsystem:		ColValue.Formated = pProcess->GetSubsystemString(); break;
#ifdef WIN32
					case eDPI_Awareness:	ColValue.Formated = pWinProc->GetDPIAwarenessString(); break;
					case eProtection:		ColValue.Formated = pWinProc->GetProtectionString(); break;
					case eOS_Context:		ColValue.Formated = pWinProc->GetOsContextString(); break;
#endif

					case eIO_ReadBytes:
					case eIO_WriteBytes:
					case eIO_OtherBytes:

					case eReceiveBytes:
					case eSendBytes:

					case eReadBytes:
					case eWriteBytes:
												if(Value.type() != QVariant::String) ColValue.Formated = FormatSize(Value.toULongLong()); break; 

					case eIO_ReadBytesDelta:
					case eIO_WriteBytesDelta:
					case eIO_OtherBytesDelta:

					case eReceiveBytesDelta:
					case eSendBytesDelta:

					case eReadBytesDelta:
					case eWriteBytesDelta:
												if(Value.type() != QVariant::String) ColValue.Formated = FormatSizeEx(Value.toULongLong(), bClearZeros); break; 

					case eIO_TotalRate:
					case eIO_ReadRate:
					case eIO_WriteRate:
					case eIO_OtherRate:
					case eReceiveRate:
					case eSendRate:
					case eNet_TotalRate:
					case eReadRate:
					case eWriteRate:
					case eDisk_TotalRate:
												if(Value.type() != QVariant::String) ColValue.Formated = FormatRateEx(Value.toULongLong(), bClearZeros); break; 


				}
			}


			if(!Highlights.isEmpty() && CurIntValue != 0)
			{
				QList<QPair<quint64, SProcessNode*> >& List = Highlights[section];

				if (List.isEmpty() || List.last().first == CurIntValue)
					List.append(qMakePair(CurIntValue, pNode));
				else if (List.last().first < CurIntValue || List.count() < iHighlightMax)
				{
					int i = 0;
					for (; i < List.size(); i++)
					{
						if (List.at(i).first < CurIntValue)
							break;
					}
					List.insert(i, qMakePair(CurIntValue, pNode));

					while (List.count() > iHighlightMax)
						List.removeLast();
				}
			}


			if(State != (Changed != 0))
			{
				if(State && Index.isValid())
					emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), section-1, pNode));
				State = (Changed != 0);
				Col = section;
			}
			if(Changed == 1)
				Changed = 0;
		}
		if(State && Index.isValid())
			emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount()-1, pNode));

	}

	if (!Highlights.isEmpty())
	{
		for (int section = eProcess; section < columnCount(); section++)
		{
			QList<QPair<quint64, SProcessNode*> >& List = Highlights[section];

			for (int i = 0; i < List.size(); i++)
				List.at(i).second->Bold.insert(section);
		}
	}

	CTreeItemModel::Sync(New, Old);

	//for (QMap<QList<QVariant>, QList<STreeNode*> >::const_iterator I = New.begin(); I != New.end(); I++)
	//{
	//	foreach(STreeNode* pNode, I.value())
	//	{
	//		QModelIndex Index = Find(m_Root, pNode);
	//		if(Index.isValid())
	//			AllIndexes.append(Index);
	//	}
	//}
	//m_AllIndexes = AllIndexes;
}

QVariant CProcessModel::NodeData(STreeNode* pNode, int role, int section) const
{
    switch(role)
	{
		case Qt::FontRole:
		{
			SProcessNode* pProcessNode = static_cast<SProcessNode*>(pNode);
			if (pProcessNode->Bold.contains(section))
			{
				QFont fnt;
				fnt.setBold(true);
				return fnt;
			}
			break;
		}
	}

	return CTreeItemModel::NodeData(pNode, role, section);
}

CProcessPtr CProcessModel::GetProcess(const QModelIndex &index) const
{
	if (!index.isValid())
        return CProcessPtr();

	SProcessNode* pNode = static_cast<SProcessNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pProcess;
}

int CProcessModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CProcessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return GetColumHeader(section);
    return QVariant();
}

QString CProcessModel::GetColumHeader(int section) const
{
	switch(section)
	{
		case eProcess:				return tr("Process");
		case ePID:					return tr("PID");
		case eCPU:					return tr("CPU");
		case eIO_TotalRate:			return tr("I/O total rate");
		case eStaus:				return tr("Status");
		case ePrivateBytes:			return tr("Private bytes");
		case eUserName:				return tr("User name");
#ifdef WIN32
		case eServices:				return tr("Services");
		case eDescription:			return tr("Description");
		case eCompanyName:			return tr("Company name");
		case eVersion:				return tr("Version");
#endif
		case eFileName:				return tr("File name");
		case eCommandLine:			return tr("Command line");
		case ePeakPrivateBytes:		return tr("Peak private bytes");
		case eWorkingSet:			return tr("Working set");
		case ePeakWS:				return tr("Peak working set");
		case ePrivateWS:			return tr("Private WS");
		case eSharedWS:				return tr("Shared WS (slow)");
		case eShareableWS:			return tr("Shareable WS (slow)");
		case eVirtualSize:			return tr("Virtual size");
		case ePeakVirtualSize:		return tr("Peak virtual size");
		case eSessionID:			return tr("Session ID");
		case ePriorityClass:		return tr("Priority class");
		case eBasePriority:			return tr("Base priority");

		case eGPU_Usage:			return tr("GPU");
		case eGPU_Shared:			return tr("Shared");
		case eGPU_Dedicated:		return tr("Dedicated");
		case eGPU_Adapter:			return tr("GPU Adapter");

		case eThreads:				return tr("Threads");
		case ePeakThreads:			return tr("Peak threads");
		case eHandles:				return tr("Handles");
		case ePeakHandles:			return tr("Peak handles");
#ifdef WIN32
		case eWND_Handles:			return tr("Windows");	
		case eGDI_Handles:			return tr("GDI handles");
		case eUSER_Handles:			return tr("USER handles");
		case eIntegrity:			return tr("Integrity");
#endif
		case eIO_Priority:			return tr("I/O priority");
		case ePagePriority:			return tr("Page priority");
		case eStartTime:			return tr("Start time");
		case eTotalCPU_Time:		return tr("Total CPU time");
		case eKernelCPU_Time:		return tr("Kernel CPU time");
		case eUserCPU_Time:			return tr("User CPU time");
#ifdef WIN32
		case eVerificationStatus:	return tr("Verification status");
		case eVerifiedSigner:		return tr("Verified signer");
		case eASLR:					return tr("ASLR");
#endif
		case eUpTime:				return tr("Up Time");
		case eArch:					return tr("CPU Arch.");
		case eElevation:			return tr("Elevation");
#ifdef WIN32
		case eWindowTitle:			return tr("Window title");
		case eWindowStatus:			return tr("Window status");
#endif
		case eCycles:				return tr("Cycles");
		case eCyclesDelta:			return tr("Cycles delta");
		case eCPU_History:			return tr("CPU graph");
		case eMEM_History:			return tr("Mem. graph");
		case eIO_History:			return tr("I/O graph");
		case eNET_History:			return tr("Net. graph");
		case eGPU_History:			return tr("GPU graph");
		case eVMEM_History:			return tr("V. Mem. graph");

#ifdef WIN32
		case eDEP:					return tr("DEP");
		case eVirtualized:			return tr("Virtualized");
#endif
		case eContextSwitches:		return tr("Context switches");
		case eContextSwitchesDelta:	return tr("Context switches delta");
		case ePageFaults:			return tr("Page faults");
		case ePageFaultsDelta:		return tr("Page faults delta");
		case eHardFaults:			return tr("Hard faults");
		case eHardFaultsDelta:		return tr("Hard faults delta");

		// IO
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

#ifdef WIN32
		case eOS_Context:			return tr("OS context");
#endif
		case ePagedPool:			return tr("Paged pool");
		case ePeakPagedPool:		return tr("Peak paged pool");
		case eNonPagedPool:			return tr("Non-paged pool");
		case ePeakNonPagedPool:		return tr("Peak non-paged pool");
		case eMinimumWS:			return tr("Minimum working set");
		case eMaximumWS:			return tr("Maximum working set");
		case ePrivateBytesDelta:	return tr("Private bytes delta");
		case eSubsystem:			return tr("Subsystem"); // WSL or Wine
#ifdef WIN32
		case ePackageName:			return tr("Package name");
		case eAppID:				return tr("App ID");
		case eDPI_Awareness:		return tr("DPI awareness");
		case eCF_Guard:				return tr("CF Guard");
#endif
		case eTimeStamp:			return tr("Time stamp");
		case eFileModifiedTime:		return tr("File modified time");
		case eFileSize:				return tr("File size");
#ifdef WIN32
		case eJobObjectID:			return tr("Job Object ID");
		case eProtection:			return tr("Protection");
		case eDesktop:				return tr("Desktop");
		case eCritical:				return tr("Critical");

		case eRunningTime:			return tr("Running Time");
		case eSuspendedTime:		return tr("Suspended Time");
		case eHangCount:			return tr("Hang Count");
		case eGhostCount:			return tr("Ghost Count");
#endif


		// Network IO
		case eNet_TotalRate:		return tr("Network total rate");
		case eReceives:				return tr("Network receives");
		case eSends:				return tr("Network sends");
		case eReceiveBytes:			return tr("Network receive bytes");
		case eSendBytes:			return tr("Network send bytes");
		//case eTotalBytes:			return tr("Network Total bytes");
		case eReceivesDelta:		return tr("Network receives delta");
		case eSendsDelta:			return tr("Network sends delta");
		case eReceiveBytesDelta:	return tr("Network receive bytes delta");
		case eSendBytesDelta:		return tr("Network send bytes delta");
		//case eTotalBytesDelta:	return tr("Network total bytes delta");
		case eReceiveRate:			return tr("Network receive rate");
		case eSendRate:				return tr("Network send rate");

		// Disk IO
		case eDisk_TotalRate:		return tr("Disk total rate");
		case eReads:				return tr("Disk reads");
		case eWrites:				return tr("Disk writes");
		case eReadBytes:			return tr("Disk read bytes");
		case eWriteBytes:			return tr("Disk write bytes");
		//case eTotalBytes:			return tr("Disk total bytes");
		case eReadsDelta:			return tr("Disk reads delta");
		case eWritesDelta:			return tr("Disk writes delta");
		case eReadBytesDelta:		return tr("Disk read bytes delta");
		case eWriteBytesDelta:		return tr("Disk write bytes delta");
		//case eTotalBytesDelta:		return tr("Disk total bytes delta");
		case eReadRate:				return tr("Disk read rate");
		case eWriteRate:			return tr("Disk write rate");
	}
	return "";
}

QVariant CProcessModel::GetDefaultIcon() const 
{ 
	return g_ExeIcon;
}
