#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/ServiceInfo.h"
#include "../Models/ServiceModel.h"

class CServiceModel;
class QSortFilterProxyModel;

class CServiceListWidget : public QWidget
{
	Q_OBJECT
public:
	CServiceListWidget(bool bEditable = false, QWidget *parent = Q_NULLPTR);

	void SetLabel(const QString& text);
	void SetServices(const QMap<QString, CServicePtr>& Services);
	void SetServicesList(const QStringList& ServiceNames);
	QStringList GetServicesList() const;

private slots:
	void OnItemSellected(QTreeWidgetItem* item);
	void OnAdd();
	void OnRemove();
	void OnStart();
	void OnPause();

protected:
	void UpdateServices();
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

	QPushButton*		m_pAddBtn;
	QPushButton*		m_pRemoveBtn;
	QPushButton*		m_pStartBtn;
	QPushButton*		m_pPauseBtn;
};