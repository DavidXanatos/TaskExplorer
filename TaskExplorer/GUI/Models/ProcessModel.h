#pragma once
#include <qwidget.h>
#include "..\..\API\ProcessInfo.h"


class CProcessModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    CProcessModel(QObject *parent = 0);
	~CProcessModel();

	void			SetTree(bool bTree)			{ m_bTree = bTree; }
	void			SetUseDescr(bool bDescr)	{ m_bUseDescr = bDescr; }

	void			Sync(QMap<quint64, CProcessPtr> ProcessList);
	void			CountItems();
	QModelIndex		FindIndex(quint64 ID);
	void			Clear();

	CProcessPtr		GetProcess(const QModelIndex &index) const;

	QVariant		Data(const QModelIndex &index, int role, int section) const;

	// derived functions
    QVariant		data(const QModelIndex &index, int role) const;
	bool			setData(const QModelIndex &index, const QVariant &value, int role);
    Qt::ItemFlags	flags(const QModelIndex &index) const;
    QModelIndex		index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex		parent(const QModelIndex &index) const;
    int				rowCount(const QModelIndex &parent = QModelIndex()) const;
    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eProcess = 0,
		ePID,
		eCPU,
		eIO_Rate,
		ePrivateBytes,
#ifdef WIN32
		eDescription,
#endif
		eStartTime,
		eThreads,
		ePriority,
#ifdef WIN32
		eIntegrity,
		eHandles,
		eOS_Ver,
		eGDI,
		eUSER,
		eSession,
#endif
		eUserName,
		eIO_Reads,
		eIO_Writes,
		eIO_Other,
		eFileName,
		eCount
	};

signals:
	void			CheckChanged(quint64 ID, bool State);
	void			Updated();

protected:
	struct SProcessNode
	{
		SProcessNode(quint64 Id){
			ID = Id;
			Parent = NULL;
			//AllChildren = 0;
		}
		~SProcessNode(){
			foreach(SProcessNode* pNode, Children)
				delete pNode;
		}

		CProcessPtr			pProcess;

		quint64				ID;
		SProcessNode*		Parent;
		QList<quint64>		Path;
		QList<SProcessNode*>Children;
		//int				AllChildren;
		QMap<quint64, int>	Aux;

		QVariant			Icon;
		struct SValue
		{
			QVariant Raw;
			QVariant Formated;
		};
		QVector<SValue>		Values;

	};

	QList<quint64>  MakeProcPath(const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList);
	void			Sync(QMap<QList<quint64>, QList<SProcessNode*> >& New, QMap<quint64, SProcessNode*>& Old);
	void			Purge(SProcessNode* pParent, const QModelIndex &parent, QMap<quint64, SProcessNode*> &Old);
	void			Fill(SProcessNode* pParent, const QModelIndex &parent, const QList<quint64>& Paths, int PathsIndex, const QList<SProcessNode*>& New, const QList<quint64>& Path);
	QModelIndex		Find(SProcessNode* pParent, SProcessNode* pNode);
	int				CountItems(SProcessNode* pRoot);

	SProcessNode*						m_Root;
	QMultiMap<quint64, SProcessNode*>	m_Map;
	bool								m_bTree;
	bool								m_bUseDescr;
};