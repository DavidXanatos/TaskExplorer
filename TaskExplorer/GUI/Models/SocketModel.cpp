#include "stdafx.h"
#include "../TaskExplorer.h"
#include "SocketModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinSocket.h"
#endif

CSocketModel::CSocketModel(QObject *parent)
:CListItemModel(parent)
{
	m_ProcessFilter = false;
}

CSocketModel::~CSocketModel()
{
}

void CSocketModel::Sync(QMultiMap<quint64, CSocketPtr> SocketList)
{
	QList<SListNode*> New;
	QMap<QVariant, SListNode*> Old = m_Map;

	foreach (const CSocketPtr& pSocket, SocketList)
	{
		if (m_ProcessFilter)
		{
			if (pSocket->GetProcess() != m_CurProcess)
				continue;
		}

		QVariant ID = (quint64)pSocket.data();

		int Row = -1;
		SSocketNode* pNode = static_cast<SSocketNode*>(Old[ID]);
		if(!pNode)
		{
			pNode = static_cast<SSocketNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pSocket = pSocket;
			New.append(pNode);
		}
		else
		{
			Old[ID] = NULL;
			Row = m_List.indexOf(pNode);
		}

#ifdef WIN32
		CWinSocket* pWinSock = qobject_cast<CWinSocket*>(pSocket.data());
#endif

		int Col = 0;
		bool State = false;
		int Changed = 0;

		// Note: icons are loaded asynchroniusly
		if (!pNode->Icon.isValid())
		{
			CProcessPtr pProcess = pNode->pSocket->GetProcess();
			if (!pProcess.isNull())
			{
				QPixmap Icon = pProcess->GetModuleInfo()->GetFileIcon();
				if (!Icon.isNull()) {
					Changed = 1; // set change for first column
					pNode->Icon = Icon;
				}
			}
		}

		int RowColor = CTaskExplorer::eNone;
		if (pSocket->IsMarkedForRemoval())		RowColor = CTaskExplorer::eToBeRemoved;
		else if (pSocket->IsNewlyCreated())		RowColor = CTaskExplorer::eAdded;
		
		if (pNode->iColor != RowColor) {
			pNode->iColor = RowColor;
			pNode->Color = CTaskExplorer::GetColor(RowColor);
			Changed = 2;
		}


		SSockStats Stats = pSocket->GetStats();

		for(int section = eProcess; section < columnCount(); section++)
		{
			QVariant Value;
			switch(section)
			{
				case eProcess:			Value = pSocket->GetProcessName(); break;
				case eProtocol:			Value = pSocket->GetProtocolString(); break; 
				case eState:			Value = pSocket->GetStateString(); break; 
				case eLocalAddress:		Value = pSocket->GetLocalAddress().toString(); break;
				case eLocalPort:		Value = pSocket->GetLocalPort(); break; 
				case eRemoteAddress:	Value = pSocket->GetRemoteAddress().toString(); break; 
				case eRemotePort:		Value = pSocket->GetRemotePort(); break; 
#ifdef WIN32
				case eOwner:			Value = pWinSock->GetOwnerServiceName(); break; 
#endif
				case eTimeStamp:		Value = pSocket->GetCreateTimeStamp(); break;
				//case eLocalHostname:	Value = ; break; 
				//case eRemoteHostname:	Value = ; break; 
				case eReceives:			Value = Stats.Net.ReceiveCount; break; 
				case eSends:			Value = Stats.Net.SendCount; break; 
				case eReceiveBytes:		Value = Stats.Net.ReceiveRaw; break; 
				case eSendBytes:		Value = Stats.Net.SendRaw; break; 
				//case eTotalBytes:		Value = ; break; 
				case eReceivesDetla:	Value = Stats.Net.ReceiveDelta.Delta; break; 
				case eSendsDelta:		Value = Stats.Net.SendDelta.Delta; break; 
				case eReceiveBytesDelta:Value = Stats.Net.ReceiveRawDelta.Delta; break; 
				case eSendBytesDelta:	Value = Stats.Net.SendRawDelta.Delta; break; 
				//case eTotalBytesDelta:	Value = ; break; 
#ifdef WIN32
				case eFirewallStatus:	Value = pWinSock->GetFirewallStatus(); break; 
#endif
				case eReceiveRate:		Value = Stats.Net.ReceiveRate.Get(); break; 
				case eSendRate:			Value = Stats.Net.SendRate.Get(); break; 
				//case eTotalRate:		Value = ; break; 
			}

			SSocketNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
					case eTimeStamp:			ColValue.Formated = QDateTime::fromTime_t(pSocket->GetCreateTimeStamp()/1000).toString("dd.MM.yyyy hh:mm:ss"); break;

					case eReceiveBytes:
					case eSendBytes:
					case eReceiveBytesDelta:
					case eSendBytesDelta:
												ColValue.Formated = FormatSize(Value.toULongLong()); break; 

					case eReceiveRate:
					case eSendRate:
												ColValue.Formated = FormatSize(Value.toULongLong()) + "/s"; break; 
				}
			}

			if(State != (Changed != 0))
			{
				if(State && Row != -1)
					emit dataChanged(createIndex(Row, Col), createIndex(Row, section-1));
				State = (Changed != 0);
				Col = section;
			}
			if(Changed == 1)
				Changed = 0;
		}
		if(State && Row != -1)
			emit dataChanged(createIndex(Row, Col, pNode), createIndex(Row, columnCount()-1, pNode));

	}

	CListItemModel::Sync(New, Old);
}

CSocketPtr CSocketModel::GetSocket(const QModelIndex &index) const
{
	if (!index.isValid())
        return CSocketPtr();

	SSocketNode* pNode = static_cast<SSocketNode*>(index.internalPointer());
	return pNode->pSocket;
}

int CSocketModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CSocketModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eProcess:			return tr("Process");
			case eProtocol:			return tr("Protocol");
			case eState:			return tr("State");
			case eLocalAddress:		return tr("Local address");
			case eLocalPort:		return tr("Local port");
			case eRemoteAddress:	return tr("Remote address");
			case eRemotePort:		return tr("Remote port");
#ifdef WIN32
			case eOwner:			return tr("Owner");
#endif
			case eTimeStamp:		return tr("Time stamp");
			//case eLocalHostname:	return tr("Local hostname");
			//case eRemoteHostname:	return tr("Remote hostname");
			case eReceives:			return tr("Receives");
			case eSends:			return tr("Sends");
			case eReceiveBytes:		return tr("Receive bytes");
			case eSendBytes:		return tr("Send bytes");
			//case eTotalBytes:		return tr("Total bytes");
			case eReceivesDetla:	return tr("Receives detla");
			case eSendsDelta:		return tr("Sends delta");
			case eReceiveBytesDelta:return tr("Receive bytes delta");
			case eSendBytesDelta:	return tr("Send bytes delta");
			//case eTotalBytesDelta:	return tr("Total bytes delta");
#ifdef WIN32
			case eFirewallStatus:	return tr("Firewall status");
#endif
			case eReceiveRate:		return tr("Receive rate");
			case eSendRate:			return tr("Send rate");
			//case eTotalRate:		return tr("Total rate");
		}
	}
    return QVariant();
}


QVariant CSocketModel::GetDefaultIcon() const 
{ 
	return g_ExeIcon;
}