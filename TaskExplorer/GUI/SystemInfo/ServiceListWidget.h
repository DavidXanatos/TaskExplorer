#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/PanelView.h"
#include "../../API/ServiceInfo.h"
#include "../Models/ServiceModel.h"

class CServiceModel;
class QSortFilterProxyModel;

class CServiceListWidget : public QWidget
{
	Q_OBJECT
public:
	CServiceListWidget(QWidget *parent = Q_NULLPTR);

	void SetLabel(const QString& text);
	void SetServices(const QMap<QString, CServicePtr>& Services);
	void SetServices(const QStringList& ServiceNames);

private slots:
	void OnItemSellected(QTreeWidgetItem* item);
	void OnStart();
	void OnPause();

protected:
	QMap<QString, CServicePtr>m_Services;

private:
	enum ESvcColumns
	{
		eName = 0,
		eDisplayName,
		eFileName,
		eCount
	};

	QGridLayout*		m_pMainLayout;
	QLabel*				m_pInfoLabel;
	QLabel*				m_pDescription;

	QTreeWidget*		m_pServiceList;

	QPushButton*		m_pStartBtn;
	QPushButton*		m_pPauseBtn;
};