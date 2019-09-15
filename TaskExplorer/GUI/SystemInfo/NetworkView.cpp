#include "stdafx.h"
#include "NetworkView.h"
#include "../TaskExplorer.h"
#ifdef WIN32
#include "../API/Windows/WindowsAPI.h"
#include "../API/Windows/Monitors/WinNetMonitor.h"
#endif


CNetworkView::CNetworkView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QGridLayout();
	//m_pMainLayout->setMargin(3);
	this->setLayout(m_pMainLayout);

	QLabel* pLabel = new QLabel(tr("Network"));
	m_pMainLayout->addWidget(pLabel, 0, 0);
	QFont font = pLabel->font();
	font.setPointSize(font.pointSize() * 1.5);
	pLabel->setFont(font);

	m_pMainLayout->addItem(new QSpacerItem(20, 30, QSizePolicy::Minimum, QSizePolicy::Minimum), 0, 1);
	//m_pNICModel = new QLabel();
	//m_pMainLayout->addWidget(m_pNICModel, 0, 2);
	//m_pNICModel->setFont(font);


	m_pScrollWidget = new QWidget();
	m_pScrollArea = new QScrollArea();
	m_pScrollLayout = new QGridLayout();
	m_pScrollLayout->setMargin(0);
	m_pScrollWidget->setLayout(m_pScrollLayout);
	m_pScrollArea->setFrameShape(QFrame::NoFrame);
	m_pScrollArea->setWidgetResizable(true);
	m_pScrollArea->setWidget(m_pScrollWidget);
	m_pMainLayout->addWidget(m_pScrollArea, 1, 0, 1, 3);
	QPalette pal = m_pScrollArea->palette();
	pal.setColor(QPalette::Background, Qt::transparent);
	m_pScrollArea->setPalette(pal);

	m_PlotLimit = theGUI->GetGraphLimit(true);
	connect(theGUI, SIGNAL(ReloadPlots()), this, SLOT(ReConfigurePlots()));
	QColor Back = theGUI->GetColor(CTaskExplorer::ePlotBack);
	QColor Front = theGUI->GetColor(CTaskExplorer::ePlotFront);
	QColor Grid = theGUI->GetColor(CTaskExplorer::ePlotGrid);

	m_pDlPlot = new CIncrementalPlot(Back, Front, Grid);
	m_pDlPlot->setMinimumHeight(120);
	m_pDlPlot->setMinimumWidth(50);
	m_pDlPlot->SetupLegend(Front, tr("Download Rate"), CIncrementalPlot::eDate, CIncrementalPlot::eBytes);
	m_pDlPlot->SetLimit(m_PlotLimit);
	m_pScrollLayout->addWidget(m_pDlPlot, 0, 0, 1, 3);

	m_pUlPlot = new CIncrementalPlot(Back, Front, Grid);
	m_pUlPlot->setMinimumHeight(120);
	m_pUlPlot->setMinimumWidth(50);
	m_pUlPlot->SetupLegend(Front, tr("Upload Rate"), CIncrementalPlot::eDate, CIncrementalPlot::eBytes);
	m_pUlPlot->SetLimit(m_PlotLimit);
	m_pScrollLayout->addWidget(m_pUlPlot, 1, 0, 1, 3);

	m_HiddenAdapters = theConf->GetStringList("Options/HiddenNICs");

	// NIC Details
	m_pNICList = new CPanelWidgetEx();

	m_pNICList->GetTree()->setItemDelegate(theGUI->GetItemDelegate());
	m_pNICList->GetTree()->setHeaderLabels(tr("Adapter|State|Speed|Receive Rate|Bytes Receive Delta|Bytes Receive|Receives Delta|Receives|Send Rate|Bytes Sent Delta|Bytes Sent|Send Delta|Sent|Address|Gateway|DNS|Domain|Interface").split("|"));
	m_pNICList->GetTree()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pNICList->GetTree()->setSortingEnabled(true);
	m_pNICList->GetTree()->setMinimumHeight(100);
	m_pNICList->GetTree()->setAutoFitMax(200 * theGUI->GetDpiScale());

	m_pNICList->GetTree()->setColumnReset(2);
	connect(m_pNICList->GetTree(), SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));

	m_pScrollLayout->addWidget(m_pNICList, 2, 0, 1, 3);
	//

	m_pShowDisconnected = new QAction("Show Disconnected");
	m_pShowDisconnected->setCheckable(true);
	m_pNICList->AddAction(m_pShowDisconnected);

	m_HoldValues = false;

	connect(m_pNICList->GetTree(), SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(OnAdapterCheck(QTreeWidgetItem*, int)));

	setObjectName(parent->objectName());
	QByteArray Columns = theConf->GetBlob(objectName() + "/NetworkView_Columns");
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pNICList->GetView()->header()->restoreState(Columns);

	m_pShowDisconnected->setChecked(theConf->GetValue(objectName() + "/ShowDisconnected", true).toBool());

}

CNetworkView::~CNetworkView()
{
	theConf->SetValue("Options/HiddenNICs", m_HiddenAdapters);
	theConf->SetBlob(objectName() + "/NetworkView_Columns", m_pNICList->GetView()->header()->saveState());
	theConf->SetValue(objectName() + "/ShowDisconnected", m_pShowDisconnected->isChecked());
}

void CNetworkView::OnResetColumns()
{
	for (int i = 0; i < eCount; i++)
		m_pNICList->GetView()->setColumnHidden(i, true);

	m_pNICList->GetView()->setColumnHidden(eAdapter, false);
	//m_pNICList->GetView()->setColumnHidden(eLinkState, false);
	//m_pNICList->GetView()->setColumnHidden(eLinkSpeed, false);

	m_pNICList->GetView()->setColumnHidden(eReadRate, false);
	m_pNICList->GetView()->setColumnHidden(eBytesRead, false);
	m_pNICList->GetView()->setColumnHidden(eWriteRate, false);
	m_pNICList->GetView()->setColumnHidden(eBytesWriten, false);

	m_pNICList->GetView()->setColumnHidden(eAddress, false);		
}

void CNetworkView::ReConfigurePlots()
{
	m_PlotLimit = theGUI->GetGraphLimit(true);
	QColor Back = theGUI->GetColor(CTaskExplorer::ePlotBack);
	QColor Front = theGUI->GetColor(CTaskExplorer::ePlotFront);
	QColor Grid = theGUI->GetColor(CTaskExplorer::ePlotGrid);

	m_pDlPlot->SetLimit(m_PlotLimit);
	m_pDlPlot->SetColors(Back, Front, Grid);
	m_pUlPlot->SetLimit(m_PlotLimit);
	m_pUlPlot->SetColors(Back, Front, Grid);
}

void CNetworkView::OnAdapterCheck(QTreeWidgetItem* item, int column)
{
	if (column != 0 || m_HoldValues)
		return;

	if (item->data(0, Qt::CheckStateRole) == Qt::Unchecked)
		m_HiddenAdapters.append(item->data(0, Qt::UserRole).toString());
	else
		m_HiddenAdapters.removeAll(item->data(0, Qt::UserRole).toString());
}

QStringList CNetworkView__JoinAddresses(const QList<QHostAddress>& Addresses, const QList<QHostAddress>& Masks = QList<QHostAddress>())
{
	QStringList List;
	for(int i=0; i < Addresses.size(); i++)
	{
		if(Masks.size() > i)
			List.append(Addresses[i].toString() + "/" + Masks[i].toString());
		else
			List.append(Addresses[i].toString());
	}
	return List;
}

void CNetworkView::Refresh()
{
	CNetMonitor* pNetMonitor = theAPI->GetNetMonitor();

	QMap<QString, CNetMonitor::SNicInfo> NicList = pNetMonitor->GetNicList(false);

	m_HoldValues = true;

	QMap<QString, QTreeWidgetItem*> OldDisks;
	for (int i = 0; i < m_pNICList->GetTree()->topLevelItemCount(); ++i)
	{
		QTreeWidgetItem* pItem = m_pNICList->GetTree()->topLevelItem(i);
		QString DeviceInterface = pItem->data(0, Qt::UserRole).toString();
		Q_ASSERT(!OldDisks.contains(DeviceInterface));
		OldDisks.insert(DeviceInterface, pItem);
	}

	for (QMap<QString, CNetMonitor::SNicInfo>::iterator I = NicList.begin(); I != NicList.end(); ++I)
	{
		CNetMonitor::SNicInfo &NicInfo = I.value();

		if (!m_pShowDisconnected->isChecked() && NicInfo.LinkState == CNetMonitor::SNicInfo::eDisconnected)
			continue;

		QTreeWidgetItem* pItem = OldDisks.take(I.key());
		if (!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, I.key());
			pItem->setText(eAdapter, NicInfo.DeviceName);
			pItem->setCheckState(0, m_HiddenAdapters.contains(NicInfo.DeviceInterface) ? Qt::Unchecked : Qt::Checked);
			m_pNICList->GetTree()->addTopLevelItem(pItem);
		}

		for(int j=0; j < pItem->columnCount(); j++)
			pItem->setForeground(j, NicInfo.LinkState != CNetMonitor::SNicInfo::eDisconnected ? QColor(Qt::black) : QColor(Qt::gray));

		pItem->setText(eLinkSpeed, FormatRate(NicInfo.LinkSpeed / 8));
		switch (NicInfo.LinkState)
		{
		case CNetMonitor::SNicInfo::eConnected:		pItem->setText(eLinkState, tr("Connected"));	break;
		case CNetMonitor::SNicInfo::eDisconnected:	pItem->setText(eLinkState, tr("Disconnected")); break;
		default:									pItem->setText(eLinkState, tr("Unknown")); break;
		}

		pItem->setText(eReadRate, FormatRate(NicInfo.ReceiveRate.Get()));
		pItem->setText(eBytesRead, FormatSize(NicInfo.ReceiveRawDelta.Value));
		pItem->setText(eBytesReadDelta, FormatSize(NicInfo.ReceiveRawDelta.Delta));
		pItem->setText(eReads, FormatNumber(NicInfo.ReceiveDelta.Value));
		pItem->setText(eReadsDelta, FormatNumber(NicInfo.ReceiveDelta.Delta));
		pItem->setText(eWriteRate, FormatRate(NicInfo.SendRate.Get()));
		pItem->setText(eBytesWriten, FormatSize(NicInfo.SendRawDelta.Value));
		pItem->setText(eBytesWritenDelta, FormatSize(NicInfo.SendRawDelta.Delta));
		pItem->setText(eWrites, FormatNumber(NicInfo.SendDelta.Value));
		pItem->setText(eWritesDelta, FormatNumber(NicInfo.SendDelta.Delta));

		pItem->setText(eAddress, CNetworkView__JoinAddresses(NicInfo.Addresses, NicInfo.NetMasks).join(", "));
		pItem->setText(eGateway, CNetworkView__JoinAddresses(NicInfo.Gateways).join(", "));
		pItem->setText(eDNS, CNetworkView__JoinAddresses(NicInfo.DNS).join(", "));
		pItem->setText(eDomain, NicInfo.Domains.toList().join(", "));

		
		pItem->setText(eDeviceInterface, NicInfo.DeviceInterface);
	}

	foreach(QTreeWidgetItem* pItem, OldDisks)
		delete pItem;

	m_HoldValues = false;
}

void CNetworkView::UpdateGraphs()
{
	CNetMonitor* pNetMonitor = theAPI->GetNetMonitor();

	QMap<QString, CNetMonitor::SNicInfo> NicList = pNetMonitor->GetNicList();

	if (m_Adapters.count() != (NicList.count() - m_HiddenAdapters.count()))
	{
		QSet<QString> OldAdapters = m_Adapters;

		QVector<QColor> Colors = theGUI->GetPlotColors();
		foreach(const CNetMonitor::SNicInfo& Nic, NicList)
		{
			if (m_HiddenAdapters.contains(Nic.DeviceInterface))
				continue;

			if (OldAdapters.contains(Nic.DeviceInterface))
			{
				OldAdapters.remove(Nic.DeviceInterface);
				continue;
			}
			m_Adapters.insert(Nic.DeviceInterface);

			m_pDlPlot->AddPlot("Nic_" + Nic.DeviceInterface, Colors[m_Adapters.size() % Colors.size()], Qt::SolidLine, false, Nic.DeviceName);
			m_pUlPlot->AddPlot("Nic_" + Nic.DeviceInterface, Colors[m_Adapters.size() % Colors.size()], Qt::SolidLine, false, Nic.DeviceName);
		}

		foreach(const QString& DeviceInterface, OldAdapters)
		{
			m_Adapters.remove(DeviceInterface);

			m_pDlPlot->RemovePlot("Nic_" + DeviceInterface);
			m_pUlPlot->RemovePlot("Nic_" + DeviceInterface);
		}
	}

	foreach(const CNetMonitor::SNicInfo& Nic, NicList)
	{
		if (m_HiddenAdapters.contains(Nic.DeviceName))
			continue;

		m_pDlPlot->AddPlotPoint("Nic_" + Nic.DeviceInterface, Nic.ReceiveRate.Get());
		m_pUlPlot->AddPlotPoint("Nic_" + Nic.DeviceInterface, Nic.SendRate.Get());
	}
}