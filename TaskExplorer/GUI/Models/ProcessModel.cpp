#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ProcessModel.h"
#include "../../../MiscHelpers/Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinProcess.h"
#include "../../API/Windows/WinToken.h"
#include "../../API/Windows/WinModule.h"
#include "../../API/Windows/WindowsAPI.h"
#else
#include "../../API/Linux/LinuxAPI.h"
#endif


CProcessModel::CProcessModel(QObject *parent)
:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
	m_iUseDescr = 1;

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

QSet<quint64> CProcessModel::Sync(QMap<quint64, CProcessPtr> ProcessList)
{
	QSet<quint64> Added;
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	bool bShow32 = theConf->GetBool("Options/Show32", true);
	bool bClearZeros = theConf->GetBool("Options/ClearZeros", true);
	bool bShowMaxThread = theConf->GetBool("Options/ShowMaxThread", false);
	int iHighlightMax = theConf->GetInt("Options/HighLoadHighlightCount", 5);
	time_t curTime = GetTime();

#ifdef WIN32
	bool HasExtProcInfo = ((CWindowsAPI*)theAPI)->HasExtProcInfo();
	bool IsMonitoringETW = ((CWindowsAPI*)theAPI)->IsMonitoringETW();
#endif

	bool bGpuStats = m_Columns.contains(eGPU_History) || m_Columns.contains(eVMEM_History)
		|| m_Columns.contains(eGPU_Usage) || m_Columns.contains(eGPU_Shared) || m_Columns.contains(eGPU_Dedicated) || m_Columns.contains(eGPU_Adapter);

	QVector<QList<QPair<quint64, SProcessNode*> > > Highlights;
	if(iHighlightMax > 0)
		Highlights.resize(columnCount());

	foreach (const CProcessPtr& pProcess, ProcessList)
	{
#ifdef WIN32
		QSharedPointer<CWinProcess> pWinProc = pProcess.staticCast<CWinProcess>();
		if (pWinProc->IsReflectedProcess())
			continue;
#endif

		quint64 ID = pProcess->GetProcessId();

		QModelIndex Index;
		
		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SProcessNode* pNode = I != Old.end() ? static_cast<SProcessNode*>(I.value()) : NULL;
		if(!pNode || (m_bTree ? !TestProcPath(pNode->Path, pProcess, ProcessList) : !pNode->Path.isEmpty()))
		{
			pNode = static_cast<SProcessNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			if (m_bTree)
				pNode->Path = MakeProcPath(pProcess, ProcessList);
			pNode->pProcess = pProcess;
			New[pNode->Path].append(pNode);
			Added.insert(ID);
		}
		else
		{
			I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));
		
        CModulePtr pModule = pProcess->GetModuleInfo();
#ifdef WIN32
		CWinTokenPtr pToken = pWinProc->GetToken();
		QSharedPointer<CWinModule> pWinModule = pModule.staticCast<CWinModule>();
#endif

		int Col = 0;
		bool State = false;
		int Changed = 0;

		// Note: icons are loaded asynchroniusly
		if (!pNode->Icon.isValid() && pModule)
		{
			QPixmap Icon = pModule->GetFileIcon();
			if (!Icon.isNull()) {
				Changed = 1; // set change for first column
				pNode->Icon = Icon;
			}
		}


		int RowColor = CTaskExplorer::eNone;
#ifdef WIN32
		if (pWinProc->TokenHasChanged() && CTaskExplorer::UseListColor(CTaskExplorer::eDangerous))				RowColor = CTaskExplorer::eDangerous;
		else if (pWinProc->IsCriticalProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eIsProtected))		RowColor = CTaskExplorer::eIsProtected;
		else if (pWinProc->IsSandBoxed() && CTaskExplorer::UseListColor(CTaskExplorer::eSandBoxed))				RowColor = CTaskExplorer::eSandBoxed;
		else
#endif
			 if (pProcess->IsServiceProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eService))			RowColor = CTaskExplorer::eService;
		else if (pProcess->IsSystemProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eSystem))			RowColor = CTaskExplorer::eSystem;
		else if (pProcess->IsElevated() && CTaskExplorer::UseListColor(CTaskExplorer::eElevated))				RowColor = CTaskExplorer::eElevated;
#ifdef WIN32
		else if (pWinProc->IsSubsystemProcess() && CTaskExplorer::UseListColor(CTaskExplorer::ePico))			RowColor = CTaskExplorer::ePico;
		else if (pWinProc->IsImmersiveProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eImmersive))		RowColor = CTaskExplorer::eImmersive;
		else if (pWinProc->IsNetProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eDotNet))				RowColor = CTaskExplorer::eDotNet;
#endif
#ifdef WIN32
		//else if (pWinProc->IsJobProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eJob))				RowColor = CTaskExplorer::eJob;
		else if (pWinProc->IsInJob() && CTaskExplorer::UseListColor(CTaskExplorer::eJob))						RowColor = CTaskExplorer::eJob;
#endif
		else if (pProcess->IsUserProcess() && CTaskExplorer::UseListColor(CTaskExplorer::eUser))				RowColor = CTaskExplorer::eUser;
		
		int SortKey = RowColor; // all but he new/removed

		if (pProcess->IsMarkedForRemoval() && CTaskExplorer::UseListColor(CTaskExplorer::eToBeRemoved))			RowColor = CTaskExplorer::eToBeRemoved;
		else if (pProcess->IsNewlyCreated() && CTaskExplorer::UseListColor(CTaskExplorer::eAdded))				RowColor = CTaskExplorer::eAdded;

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

		SProcStats IoStats = pProcess->GetStats();
		STaskStatsEx CpuStats = pProcess->GetCpuStats();
		STaskStats CpuStats2 = pProcess->GetCpuStats2();
		SGpuStats GpuStats;
		if (bGpuStats) // Note: GetGpuStats marks gpu stats to be keept updated, hence don't get it when its not needed
			GpuStats = pProcess->GetGpuStats();

		for(int section = 0; section < columnCount(); section++)
		{
			if (!m_Columns.contains(section))
				continue; // ignore columns which are hidden

			quint64 CurIntValue = -1;

			QVariant Value;
			switch(section)
			{
				case eProcess:
				{
					QString Name = pProcess->GetName();
#ifdef WIN32
					Name += bShow32 && pWinProc->IsWoW64() ? " *32" : "";
#endif
					if (m_iUseDescr)
					{
						QString Descr;
#ifdef WIN32
						bool IsSvcHost = Name.compare("svchost.exe", Qt::CaseInsensitive) == 0;
						if (IsSvcHost && pProcess->IsServiceProcess())
						{
							Descr = pWinProc->GetServiceList().join(tr(", "));
						}
						else 
#endif
						  if (pModule)
							Descr = pModule->GetFileInfo("Description");

						if (!Descr.isEmpty())
						{
							if (m_iUseDescr == 1)
								Value = Descr + " (" + Name + ")";
							else
								Value = Name + " (" + Descr + ")";
							break;
						}
					}
											Value = Name; break;
				}
				case ePID:					Value = pProcess->GetProcessId(); break;
				case eParentPID:			Value = pProcess->GetParentId(); break;
				case eConsolePID:			Value = pWinProc->GetConsoleHostId(); break;
				case eSequenceNumber:		Value = pWinProc->GetProcessSequenceNumber(); break;
				case eStartKey:				Value = pWinProc->GetStartKey(); break;
				case eCPU_History:
				case eCPU:					Value = CpuStats.CpuUsage; CurIntValue = 10000 * Value.toDouble(); break;
				case eIO_History:			Value = qMax(IoStats.Disk.ReadRate.Get(), IoStats.Io.ReadRate.Get()) + qMax(IoStats.Disk.WriteRate.Get(), IoStats.Io.WriteRate.Get()) + IoStats.Io.OtherRate.Get(); break;
				case eIO_TotalRate:			Value = CurIntValue = IoStats.Io.ReadRate.Get() + IoStats.Io.WriteRate.Get() + IoStats.Io.OtherRate.Get(); break;
				case eStaus:				Value = pProcess->GetStatusString(); break;
				case ePrivateBytes:			Value = CurIntValue = CpuStats.PrivateBytesDelta.Value; break;
				case eUserName:				Value = pProcess->GetUserName(); break;
#ifdef WIN32
				case eServices:				Value = pWinProc->GetServiceList().join(tr(", ")); break;
				case eDescription:			Value = pModule ? pModule->GetFileInfo("Description") : ""; break;
				case eCompanyName:			Value = pModule ? pModule->GetFileInfo("CompanyName") : ""; break;
				case eVersion:				Value = pModule ? pModule->GetFileInfo("FileVersion") : ""; break;
#endif

				case eGPU_History:			Value = GpuStats.GpuTimeUsage.Usage; break;
				case eVMEM_History:			Value = qMax(GpuStats.GpuDedicatedUsage,GpuStats.GpuSharedUsage); break;

				case eGPU_Usage:			Value = GpuStats.GpuTimeUsage.Usage; CurIntValue = 10000 * Value.toDouble(); break;
				case eGPU_Shared:			Value = CurIntValue = GpuStats.GpuSharedUsage; break;
				case eGPU_Dedicated:		Value = CurIntValue = GpuStats.GpuDedicatedUsage; break;
				case eGPU_Adapter:			Value = GpuStats.GpuAdapter; break;

				case eFileName:				Value = pProcess->GetFileName(); break;
				case eCommandLine:			Value = pProcess->GetCommandLineStr(); break;
				case ePeakPrivateBytes:		Value = CurIntValue = pProcess->GetPeakPrivateBytes(); break;
				case eMEM_History:
				case eWorkingSet:			Value = CurIntValue = pProcess->GetWorkingSetSize(); break;
				case ePeakWS:				Value = CurIntValue = pProcess->GetPeakWorkingSetSize(); break;
				case ePrivateWS:			Value = CurIntValue = pProcess->GetPrivateWorkingSetSize(); break;
				case eSharedWS:				Value = CurIntValue = pProcess->GetSharedWorkingSetSize(); break;
				case eShareableWS:			Value = CurIntValue = pProcess->GetShareableWorkingSetSize(); break;
#ifdef WIN32
				case eShareableCommit:		Value = CurIntValue = pWinProc->GetShareableCommitSize(); break;
#endif
				case eVirtualSize:			Value = CurIntValue = pProcess->GetVirtualSize(); break;
				case ePeakVirtualSize:		Value = CurIntValue = pProcess->GetPeakVirtualSize(); break;
				case eSessionID:			Value = pProcess->GetSessionID(); break;
				case eDebugTotal:			Value = pProcess->GetDebugMessageCount(); break;
				case ePriorityClass:		Value = (quint32)pProcess->GetPriority(); break;
				case eBasePriority:			Value = (quint32)pProcess->GetBasePriority(); break;
#ifdef WIN32
				case ePriorityBoost:		Value = pWinProc->HasPriorityBoost(); break;
#endif

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
				case eVerificationStatus:	Value = pWinModule ? pWinModule->GetVerifyResultString() : ""; break;
				case eVerifiedSigner:		Value = pWinModule ? pWinModule->GetVerifySignerName() : ""; break;
				case eMitigations:			Value = pWinProc->GetMitigationsString(); break;
				case eImageCoherency:		Value = pWinModule ? pWinModule->GetImageCoherency() : -1.0F; break;
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
				case eVirtualized:			Value = pToken ? pToken->GetVirtualizationString() : ""; break;
#endif
				case eContextSwitches:		Value = CurIntValue = CpuStats.ContextSwitchesDelta.Value; break;
				case eContextSwitchesDelta:	Value = CurIntValue = CpuStats.ContextSwitchesDelta.Delta; break;
				case ePageFaults:			Value = CurIntValue = CpuStats.PageFaultsDelta.Value; break;
				case ePageFaultsDelta:		Value = CurIntValue = CpuStats.PageFaultsDelta.Delta; break;
				case eHardFaults:			Value = CurIntValue = CpuStats.HardFaultsDelta.Value; break;
				case eHardFaultsDelta:		Value = CurIntValue = CpuStats.HardFaultsDelta.Delta; break;

				// IO
				case eIO_Reads:				Value = CurIntValue = IoStats.Io.ReadCount; break;
				case eIO_Writes:			Value = CurIntValue = IoStats.Io.WriteCount; break;
				case eIO_Other:				Value = CurIntValue = IoStats.Io.OtherCount; break;
				case eIO_ReadBytes:			Value = CurIntValue = IoStats.Io.ReadRaw; break;
				case eIO_WriteBytes:		Value = CurIntValue = IoStats.Io.WriteRaw; break;
				case eIO_OtherBytes:		Value = CurIntValue = IoStats.Io.OtherRaw; break;
				//case eIO_TotalBytes:		Value = CurIntValue = ; break;
				case eIO_ReadsDelta:		Value = CurIntValue = IoStats.Io.ReadDelta.Delta; break;
				case eIO_WritesDelta:		Value = CurIntValue = IoStats.Io.WriteDelta.Delta; break;
				case eIO_OtherDelta:		Value = CurIntValue = IoStats.Io.OtherDelta.Delta; break;
				//case eIO_TotalDelta:		Value = CurIntValue = ; break;
				case eIO_ReadBytesDelta:	Value = CurIntValue = IoStats.Io.ReadRawDelta.Delta; break;
				case eIO_WriteBytesDelta:	Value = CurIntValue = IoStats.Io.WriteRawDelta.Delta; break;
				case eIO_OtherBytesDelta:	Value = CurIntValue = IoStats.Io.OtherRawDelta.Delta; break;
				//case eIO_TotalBytesDelta:	Value = CurIntValue = ; break;
				case eIO_ReadRate:			Value = CurIntValue = IoStats.Io.ReadRate.Get(); break;
				case eIO_WriteRate:			Value = CurIntValue = IoStats.Io.WriteRate.Get(); break;
				case eIO_OtherRate:			Value = CurIntValue = IoStats.Io.OtherRate.Get(); break;
				//case eIO_TotalRate:		Value = CurIntValue = ; break;

#ifdef WIN32
				case eOS_Context:			Value = (quint32)pWinProc->GetOsContextVersion(); break;
				case ePagedPool:			Value = CurIntValue = pWinProc->GetPagedPool(); break;
				case eTLS:					Value = CurIntValue = pWinProc->GetTlsBitmapCount(); break;
				case ePeakPagedPool:		Value = CurIntValue = pWinProc->GetPeakPagedPool(); break;
				case eNonPagedPool:			Value = CurIntValue = pWinProc->GetNonPagedPool(); break;
				case ePeakNonPagedPool:		Value = CurIntValue = pWinProc->GetPeakNonPagedPool(); break;
#endif
				case eMinimumWS:			Value = /*CurIntValue =*/ pProcess->GetMinimumWS(); break;
				case eMaximumWS:			Value = /*CurIntValue =*/ pProcess->GetMaximumWS(); break;
				case ePrivateBytesDelta:	Value = /*CurIntValue =*/ CpuStats.PrivateBytesDelta.Delta; break;
				case eSubsystem:			Value = (quint32)pProcess->GetSubsystem(); break;
#ifdef WIN32
				case ePackageName:			Value = pWinProc->GetPackageName(); break;
				case eAppID:				Value = pWinProc->GetAppID();  break;
				case eDPI_Awareness:		Value = (int)pWinProc->GetDPIAwareness(); break;
				case eTimeStamp:			Value = pWinModule ? pWinModule->GetTimeStamp() : 0; break;
#endif
				case eFileModifiedTime:		Value = pModule ? pModule->GetModificationTime() : 0; break;
				case eFileSize:				Value = pModule ? pModule->GetFileSize() : 0; break;
#ifdef WIN32
				case eJobObjectID:			Value = pWinProc->GetJobObjectID(); break;
				case eProtection:			Value = (int)pWinProc->GetProtection(); break;
				case eDesktop:				Value = pWinProc->GetUsedDesktop(); break;
				case eCritical:				Value = pWinProc->IsCriticalProcess(); break;

				case ePowerThrottling:		Value = pWinProc->IsPowerThrottled(); break;

				case eRunningTime:			Value = pWinProc->GetUpTime(); break;
				case eSuspendedTime:		Value = pWinProc->GetSuspendTime(); break;
				case eHangCount:			Value = pWinProc->GetHangCount(); break;
				case eGhostCount:			Value = pWinProc->GetGhostCount(); break;

				case eErrorMode:			Value = pWinProc->GetErrorMode(); break;
				case eCodePage:				Value = pWinProc->GetCodePage(); break;
				case eReferences:			Value = pWinProc->GetReferenceCount(); break;
				case eGrantedAccess:		Value = pWinProc->GetAccessMask(); break;
#endif

				// Network IO
				case eNET_History:
				case eNet_TotalRate:		Value = CurIntValue = IoStats.Net.ReceiveRate.Get() + IoStats.Net.SendRate.Get(); break; 
				case eNetUsage:				Value = pProcess->GetNetworkUsageFlags(); break;
				case eReceives:				Value = CurIntValue = IoStats.Net.ReceiveCount; break; 
				case eSends:				Value = CurIntValue = IoStats.Net.SendCount; break; 
				case eReceiveBytes:			Value = CurIntValue = IoStats.Net.ReceiveRaw; break; 
				case eSendBytes:			Value = CurIntValue = IoStats.Net.SendRaw; break; 
				//case eTotalBytes:			Value = CurIntValue = ; break; 
				case eReceivesDelta:		Value = CurIntValue = IoStats.Net.ReceiveDelta.Delta; break; 
				case eSendsDelta:			Value = CurIntValue = IoStats.Net.SendDelta.Delta; break; 
				case eReceiveBytesDelta:	Value = CurIntValue = IoStats.Net.ReceiveRawDelta.Delta; break; 
				case eSendBytesDelta:		Value = CurIntValue = IoStats.Net.SendRawDelta.Delta; break; 
				//case eTotalBytesDelta:	Value = CurIntValue = ; break; 
				case eReceiveRate:			Value = CurIntValue = IoStats.Net.ReceiveRate.Get(); break; 
				case eSendRate:				Value = CurIntValue = IoStats.Net.SendRate.Get(); break; 

				// Disk IO
				case eDisk_TotalRate:		Value = CurIntValue = IoStats.Disk.ReadRate.Get() + IoStats.Disk.WriteRate.Get(); break; 
				case eReads:				Value = CurIntValue = IoStats.Disk.ReadCount; break;
				case eWrites:				Value = CurIntValue = IoStats.Disk.WriteCount; break;
				case eReadBytes:			Value = CurIntValue = IoStats.Disk.ReadRaw; break;
				case eWriteBytes:			Value = CurIntValue = IoStats.Disk.WriteRaw; break;
				//case eTotalBytes:			Value = CurIntValue = ; break;
				case eReadsDelta:			Value = CurIntValue = IoStats.Disk.ReadDelta.Delta; break;
				case eWritesDelta:			Value = CurIntValue = IoStats.Disk.WriteDelta.Delta; break;
				case eReadBytesDelta:		Value = CurIntValue = IoStats.Disk.ReadRawDelta.Delta; break;
				case eWriteBytesDelta:		Value = CurIntValue = IoStats.Disk.WriteRawDelta.Delta; break;
				//case eTotalBytesDelta:	Value = CurIntValue = ; break;
				case eReadRate:				Value = CurIntValue = IoStats.Disk.ReadRate.Get(); break;
				case eWriteRate:			Value = CurIntValue = IoStats.Disk.WriteRate.Get(); break;
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
					case ePID:				
					case eParentPID:
					case eConsolePID:
											if (Value.toLongLong() < 0) ColValue.Formated = ""; 
											else ColValue.Formated = theGUI->FormatID(Value.toLongLong()); 
											break;
					case eStartKey:			ColValue.Formated = tr("0x%1").arg(Value.toULongLong(), 0, 16); break;
					case eCPU:
											if(!bClearZeros || Value.toDouble() > 0.00004)
											{
												QString ValueStr = QString::number(Value.toDouble()*100, 10, 2) + "%"; 
												if(bShowMaxThread)
													ValueStr += " / " + QString::number(CpuStats2.CpuUsage*100, 10, 2) + "%"; 
												ColValue.Formated = ValueStr;
											}
											else
												ColValue.Formated = "";
											break;


					case eGPU_Usage:
											ColValue.Formated = (!bClearZeros || Value.toDouble() > 0.00004) ? QString::number(Value.toDouble()*100, 10, 2) + "%" : ""; break;

					case eStaus:			ColValue.SortKey = SortKey;	break;

					case ePrivateBytes:		
					case ePeakPrivateBytes:
					case eWorkingSet:
					case ePeakWS:
					case ePrivateWS:
					case eSharedWS:
					case eShareableWS:
#ifdef WIN32
					case eShareableCommit:
#endif
					case eVirtualSize:
					case ePeakVirtualSize:
#ifdef WIN32
					case ePagedPool:
					case ePeakPagedPool:
					case eNonPagedPool:
					case ePeakNonPagedPool:
#endif
					case eMinimumWS:
					case eMaximumWS:

					case eFileSize:
											ColValue.Formated = FormatSize(Value.toULongLong()); break;

#ifdef WIN32
					case eTLS:				ColValue.Formated = pWinProc->GetTlsBitmapCountString(); break;

					case eErrorMode:		ColValue.Formated = pWinProc->GetErrorModeString(); break;
					case eGrantedAccess:	ColValue.Formated = pWinProc->GetAccessMaskString(); break;
#endif

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

#ifdef WIN32
					// since not all programs use GUI resources, make this value clearable
					case eWND_Handles:
					case eGDI_Handles:
					case eUSER_Handles:
					case eHangCount:
					case eGhostCount:
#endif

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

					case eStartTime:		ColValue.Formated = QDateTime::fromSecsSinceEpoch(Value.toULongLong()/1000).toString("dd.MM.yyyy hh:mm:ss"); break;
#ifdef WIN32
					case eTimeStamp:
#endif
					case eFileModifiedTime:
											if (Value.toULongLong() != 0) ColValue.Formated = QDateTime::fromSecsSinceEpoch(Value.toULongLong()).toString("dd.MM.yyyy hh:mm:ss"); break;
                    case eUpTime:
#ifdef WIN32
					case eRunningTime:			
					case eSuspendedTime:
#endif
											ColValue.Formated = (Value.toULongLong() == 0) ? QString() : FormatTime(Value.toULongLong()); break;
					case eTotalCPU_Time:
					case eKernelCPU_Time:
					case eUserCPU_Time:
											ColValue.Formated = FormatTime(Value.toULongLong()); break;

					case ePriorityClass:	ColValue.Formated = pProcess->GetPriorityString(); break;
#ifdef WIN32
					case ePriorityBoost:	ColValue.Formated = pWinProc->HasPriorityBoost() ? tr("Yes") : ""; break;
#endif
					case eBasePriority:		ColValue.Formated = pProcess->GetBasePriorityString(); break;
					case ePagePriority:		ColValue.Formated = pProcess->GetPagePriorityString(); break;
					case eIO_Priority:		ColValue.Formated = pProcess->GetIOPriorityString(); break;
#ifdef WIN32
					case eIntegrity:		ColValue.Formated = pToken ? pToken->GetIntegrityString() : "";  break;
					case eImageCoherency:	ColValue.Formated = pWinModule ? pWinModule->GetImageCoherencyString() : ""; break;
					case eCritical:			ColValue.Formated = pWinProc->IsCriticalProcess() ? tr("Critical") : ""; break;

					case ePowerThrottling:	ColValue.Formated = pWinProc->IsPowerThrottled() ? tr("Yes") : ""; break;
#endif
					case eSubsystem:		ColValue.Formated = pProcess->GetSubsystemString(); break;
#ifdef WIN32
					case eDPI_Awareness:	ColValue.Formated = pWinProc->GetDPIAwarenessString(); break;
					case eProtection:		ColValue.Formated = pWinProc->GetProtectionString(); break;
					case eOS_Context:		ColValue.Formated = pWinProc->GetOsContextString(); break;
#endif
					case eNetUsage:			ColValue.Formated = pProcess->GetNetworkUsageString(); break;

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

	return Added;
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
		//case ePID_LXSS:				return tr("PID (LXSS)");
		case eParentPID:			return tr("Parent PID");
		case eConsolePID:			return tr("Console PID");
		case eSequenceNumber:		return tr("Seq. number");
		case eStartKey:				return tr("Start key");
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
		case eNetUsage:				return tr("Network");

		case eFileName:				return tr("File name");
		case eCommandLine:			return tr("Command line");
		case ePeakPrivateBytes:		return tr("Peak private bytes");
		case eWorkingSet:			return tr("Working set");
		case ePeakWS:				return tr("Peak working set");
		case ePrivateWS:			return tr("Private WS");
		case eSharedWS:				return tr("Shared WS (slow)");
		case eShareableWS:			return tr("Shareable WS (slow)");
#ifdef WIN32
		case eShareableCommit:		return tr("Shared commit");
#endif
		case eVirtualSize:			return tr("Virtual size");
		case ePeakVirtualSize:		return tr("Peak virtual size");
		case eDebugTotal:			return tr("Debug Messages");
		//case eDebugDelte:			return tr("Debug Message Delta");
		case eSessionID:			return tr("Session ID");
		case ePriorityClass:		return tr("Priority class");
		case eBasePriority:			return tr("Base priority");
#ifdef WIN32
		case ePriorityBoost:		return tr("Priority boost");
#endif

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
		case eMitigations:			return tr("Mitigations");
		case eImageCoherency:		return tr("Image coherency");
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
		case eTLS:					return tr("Thread local storage");
		case ePagedPool:			return tr("Paged pool");
		case ePeakPagedPool:		return tr("Peak paged pool");
		case eNonPagedPool:			return tr("Non-paged pool");
		case ePeakNonPagedPool:		return tr("Peak non-paged pool");
#endif
		case eMinimumWS:			return tr("Minimum working set");
		case eMaximumWS:			return tr("Maximum working set");
		case ePrivateBytesDelta:	return tr("Private bytes delta");
		case eSubsystem:			return tr("Subsystem"); // WSL or Wine
#ifdef WIN32
		case ePackageName:			return tr("Package name");
		case eAppID:				return tr("App ID");
		case eDPI_Awareness:		return tr("DPI awareness");
#endif
		case eTimeStamp:			return tr("Time stamp");
		case eFileModifiedTime:		return tr("File modified time");
		case eFileSize:				return tr("File size");
#ifdef WIN32
		case eJobObjectID:			return tr("Job Object ID");
		case eProtection:			return tr("Protection");
		case eDesktop:				return tr("Desktop");
		case eCritical:				return tr("Critical Process");

		case ePowerThrottling:		return tr("Power throttling");
		case eRunningTime:			return tr("Running Time");
		case eSuspendedTime:		return tr("Suspended Time");
		case eHangCount:			return tr("Hang Count");
		case eGhostCount:			return tr("Ghost Count");

		case eErrorMode:			return tr("Error mode");
		case eCodePage:				return tr("Code page");
		case eReferences:			return tr("References");
		case eGrantedAccess:		return tr("Granted access");
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
