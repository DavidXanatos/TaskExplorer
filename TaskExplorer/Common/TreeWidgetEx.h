#pragma once

class QTreeWidgetEx: public QTreeWidget
{
	Q_OBJECT
public:
	QTreeWidgetEx(QWidget *parent = 0) : QTreeWidget(parent) 
	{
		header()->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(header(), SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

		m_pMenu = new QMenu(this);
	}

private slots:
	void				OnMenuRequested(const QPoint &point)
	{
		if(m_Columns.isEmpty())
		{
			QTreeWidgetItem* pHeader = headerItem();
			for(int i=0; i < columnCount(); i++)
			{
				QAction* pAction = new QAction(pHeader->text(i), m_pMenu);
				pAction->setCheckable(true);
				connect(pAction, SIGNAL(triggered()), this, SLOT(OnMenu()));
				m_pMenu->addAction(pAction);
				m_Columns[pAction] = i;
			}
		}

		for(QMap<QAction*, int>::iterator I = m_Columns.begin(); I != m_Columns.end(); I++)
			I.key()->setChecked(!isColumnHidden(I.value()));

		m_pMenu->popup(QCursor::pos());	
	}

	void				OnMenu()
	{
		QAction* pAction = (QAction*)sender();
		int Column = m_Columns.value(pAction, -1);
		setColumnHidden(Column, !pAction->isChecked());
	}

protected:
	QMenu*				m_pMenu;
	QMap<QAction*, int>	m_Columns;
};
