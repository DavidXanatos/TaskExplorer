#pragma once
#include <qwidget.h>
#include "..\..\API\ThreadInfo.h"

class CThreadModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    CThreadModel(QObject *parent = 0);
	~CThreadModel();

	void			Sync(QMap<quint64, CThreadPtr> ThreadList);
	QModelIndex		FindIndex(quint64 SubID);
	void			Clear();

	CThreadPtr		GetThread(const QModelIndex &index) const;

	QVariant		Data(const QModelIndex &index, int role, int section) const;

	// derived functions
    QVariant		data(const QModelIndex &index, int role) const;
    Qt::ItemFlags	flags(const QModelIndex &index) const;
    QModelIndex		index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex		parent(const QModelIndex &index) const;
    int				rowCount(const QModelIndex &parent = QModelIndex()) const;
    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eThread = 0,
		eCPU,
		eCyclesDelta,
		eStartAddress,
		ePriority,
		eService,
		eName,
		eCreated,
		eStartModule,
		eContextSwitches,
		eBasePriority,
		ePagePriority,
		eIOPriority,
		eCycles,
		eState,
		eKernelTime,
		eUserTime,
		eIdealProcessor,
		eCritical,
		eCount
	};

protected:
	struct SThreadNode
	{
		quint64				UID;
		
		CThreadPtr			pThread;

		struct SValue
		{
			QVariant Raw;
			QVariant Formated;
		};
		QVector<SValue>		Values;
	};

	void Sync(QList<SThreadNode*>& New, QMap<quint64, SThreadNode*>& Old);

	QList<SThreadNode*>			m_List;
	QMap<quint64, SThreadNode*>	m_Map;
};