#include "stdafx.h"
#include "../TaskExplorer.h"
#include "SocketModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinSocket.h"
#endif

CSocketModel::CSocketModel(QObject *parent)
:QAbstractItemModel(parent)
{
	m_ProcessFilter = false;
}

CSocketModel::~CSocketModel()
{
	foreach(SSocketNode* pNode, m_List)
		delete pNode;
}

void CSocketModel::Sync(QMultiMap<quint64, CSocketPtr> SocketList)
{
	QList<SSocketNode*> New;
	QMap<quint64, SSocketNode*> Old = m_Map;

	foreach (const CSocketPtr& pSocket, SocketList)
	{
		if (m_ProcessFilter)
		{
			if (pSocket->GetProcess() != m_CurProcess)
				continue;
		}

		quint64 UID = (quint64)pSocket.data();

		int Row = -1;
		SSocketNode* pNode = Old[UID];
		if(!pNode)
		{
			pNode = new SSocketNode();
			pNode->Values.resize(columnCount());
			pNode->UID = UID;
			pNode->pSocket = pSocket;
			New.append(pNode);
		}
		else
		{
			Old[UID] = NULL;
			Row = m_List.indexOf(pNode);
		}

		int Col = 0;
		bool State = false;
		bool Changed = false;

		// Note: icons are loaded asynchroniusly
		if (!pNode->Icon.isValid())
		{
			CProcessPtr pProcess = pNode->pSocket->GetProcess();
			if (!pProcess.isNull())
			{
				QPixmap Icon = pProcess->GetFileIcon();
				if (!Icon.isNull()) {
					Changed = true; // set change for first column
					pNode->Icon = Icon;
				}
			}
		}

#ifdef WIN32
		CWinSocket* pWinSock = qobject_cast<CWinSocket*>(pSocket.data());
#endif

		for(int section = eProcess; section < columnCount(); section++)
		{
			QVariant Value;
			switch(section)
			{
				case eProcess:			Value = pSocket->GetProcessName(); break;
				case eLocalAddress:		Value = pSocket->GetLocalAddress().toString(); break;
				case eLocalPort:		Value = pSocket->GetLocalPort(); break; 
				case eRemoteAddress:	Value = pSocket->GetRemoteAddress().toString(); break; 
				case eRemotePort:		Value = pSocket->GetRemotePort(); break; 
				case eProtocol:			Value = pSocket->GetProtocolString(); break; 
				case eState:			Value = pSocket->GetStateString(); break; 
#ifdef WIN32
				case eOwner:			Value = pWinSock->GetOwnerServiceName(); break; 
#endif
				case eTimeStamp:		Value = pWinSock->GetCreateTime(); break;
				//case eLocalHostname:	Value = ; break; 
				//case eRemoteHostname:	Value = ; break; 
				//case eReceives:			Value = ; break; 
				//case eSends:			Value = ; break; 
				//case eReceiveBytes:		Value = ; break; 
				//case eSendBytes:		Value = ; break; 
				//case eTotalBytes:		Value = ; break; 
				//case eReceivesDetla:	Value = ; break; 
				//case eSendsDelta:		Value = ; break; 
				//case eReceiveBytesDelta:Value = ; break; 
				//case eSendBytesDelta:	Value = ; break; 
				//case eTotalBytesDelta:	Value = ; break; 
				//case eFirewallStatus:	Value = ; break; 
				//case eReceiveRate:		Value = ; break; 
				//case eSendRate:			Value = ; break; 
				//case eTotalRate:		Value = ; break; 
			}

			SSocketNode::SValue& ColValue = pNode->Values[section];

			bool Changed = false;
			if (ColValue.Raw != Value)
			{
				Changed = true;
				ColValue.Raw = Value;

				switch (section)
				{
					case eTimeStamp:	ColValue.Formated = pWinSock->GetCreateTime().toString("dd.MM.yyyy hh:mm:ss"); break;
				}
			}

			if(State != Changed)
			{
				if(State && Row != -1)
					emit dataChanged(createIndex(Row, Col), createIndex(Row, section-1));
				State = Changed;
				Col = section;
			}
			Changed = false;
		}
		if(State && Row != -1)
			emit dataChanged(createIndex(Row, Col, pNode), createIndex(Row, columnCount()-1, pNode));

	}

	Sync(New, Old);
}

void CSocketModel::Sync(QList<SSocketNode*>& New, QMap<quint64, SSocketNode*>& Old)
{
	int Begin = -1;
	int End = -1;
	for(int i = m_List.count()-1; i >= -1; i--) 
	{
		quint64 ID = i >= 0 ? m_List[i]->UID : -1;
		if(ID != -1 && (Old.value(ID) != NULL)) // remove it
		{
			m_Map.remove(ID);
			if(End == -1)
				End = i;
		}
		else if(End != -1) // keep it and remove whatis to be removed at once
		{
			Begin = i + 1;

			beginRemoveRows(QModelIndex(), Begin, End);
			for(int j = End; j >= Begin; j--)
				delete m_List.takeAt(j);
			endRemoveRows();

			End = -1;
			Begin = -1;
		}
    }

	Begin = m_List.count();
	for(QList<SSocketNode*>::iterator I = New.begin(); I != New.end(); I++)
	{
		SSocketNode* pNode = *I;
		m_Map.insert(pNode->UID, pNode);
		m_List.append(pNode);
	}
	End = m_List.count();
	if(Begin < End)
	{
		beginInsertRows(QModelIndex(), Begin, End-1);
		endInsertRows();
	}
}

QModelIndex CSocketModel::FindIndex(quint64 SubID)
{
	if(SSocketNode* pNode = m_Map.value(SubID))
	{
		int row = m_List.indexOf(pNode);
		ASSERT(row != -1);
		return createIndex(row, eProcess, pNode);
	}
	return QModelIndex();
}

void CSocketModel::Clear()
{
	//beginResetModel();
	beginRemoveRows(QModelIndex(), 0, rowCount());
	foreach(SSocketNode* pNode, m_List)
		delete pNode;
	m_List.clear();
	m_Map.clear();
	endRemoveRows();
	//endResetModel();
}

QVariant CSocketModel::data(const QModelIndex &index, int role) const
{
    return Data(index, role, index.column());
}

CSocketPtr CSocketModel::GetSocket(const QModelIndex &index) const
{
	if (!index.isValid())
        return CSocketPtr();

	SSocketNode* pNode = static_cast<SSocketNode*>(index.internalPointer());
	return pNode->pSocket;
}

QVariant CSocketModel::Data(const QModelIndex &index, int role, int section) const
{
	if (!index.isValid())
        return QVariant();

    //if(role == Qt::SizeHintRole )
    //    return QSize(64,16); // for fixing height

	SSocketNode* pNode = static_cast<SSocketNode*>(index.internalPointer());

	switch(role)
	{
		case Qt::DisplayRole:
		{
			SSocketNode::SValue& Value = pNode->Values[section];
			return Value.Formated.isValid() ? Value.Formated : Value.Raw;
		}
		case Qt::EditRole: // sort role
		{
			return pNode->Values[section].Raw;
		}
		case Qt::DecorationRole:
		{
			if (section == eProcess)
				return pNode->Icon.isValid() ? pNode->Icon : g_ExeIcon;
			break;
		}
		case Qt::BackgroundRole:
		{
			//return QBrush(QColor(255,128,128));
			break;
		}
		case Qt::ForegroundRole:
		{
			/* QColor Color = Qt::black;
			return QBrush(Color); */
			break;
		}
		case Qt::UserRole:
		{
			switch(section)
			{
				case eProcess:			return pNode->UID;
			}
			break;
		}
	}
	return QVariant();
}

Qt::ItemFlags CSocketModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex CSocketModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (parent.isValid()) 
        return QModelIndex();
	if(m_List.count() > row)
		return createIndex(row, column, m_List[row]);
	return QModelIndex();
}

QModelIndex CSocketModel::parent(const QModelIndex &index) const
{
	return QModelIndex();
}

int CSocketModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    if (parent.isValid())
        return 0;
	return m_List.count();
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
			case eLocalAddress:		return tr("Local address");
			case eLocalPort:		return tr("Local port");
			case eRemoteAddress:	return tr("Remote address");
			case eRemotePort:		return tr("Remote port");
			case eProtocol:			return tr("Protocol");
			case eState:			return tr("State");
#ifdef WIN32
			case eOwner:			return tr("Owner");
#endif
			case eTimeStamp:		return tr("Time stamp");
			case eLocalHostname:	return tr("Local hostname");
			case eRemoteHostname:	return tr("Remote hostname");
			case eReceives:			return tr("Receives");
			case eSends:			return tr("Sends");
			case eReceiveBytes:		return tr("Receive bytes");
			case eSendBytes:		return tr("Send bytes");
			case eTotalBytes:		return tr("Total bytes");
			case eReceivesDetla:	return tr("Receives detla");
			case eSendsDelta:		return tr("Sends delta");
			case eReceiveBytesDelta:return tr("Receive bytes delta");
			case eSendBytesDelta:	return tr("Send bytes delta");
			case eTotalBytesDelta:	return tr("Total bytes delta");
			case eFirewallStatus:	return tr("Firewall status");
			case eReceiveRate:		return tr("Receive rate");
			case eSendRate:			return tr("Send rate");
			case eTotalRate:		return tr("Total rate");
		}
	}
    return QVariant();
}
