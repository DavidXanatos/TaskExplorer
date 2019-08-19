#include "stdafx.h"
#include "CPUView.h"
#include "../TaskExplorer.h"


CCPUView::CCPUView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QGridLayout();
	//m_pMainLayout->setMargin(3);
	this->setLayout(m_pMainLayout);

	QLabel* pLabel = new QLabel(tr("CPU"));
	m_pMainLayout->addWidget(pLabel, 0, 0);
	QFont font = pLabel->font();
	font.setPointSize(font.pointSize() * 1.5);
	pLabel->setFont(font);

	m_pMainLayout->addItem(new QSpacerItem(20, 30, QSizePolicy::Minimum, QSizePolicy::Minimum), 0, 1);
	m_pCPUModel = new QLabel();
	m_pMainLayout->addWidget(m_pCPUModel, 0, 2);
	m_pCPUModel->setFont(font);

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

	QColor Back = QColor(0, 0, 64);
	QColor Front = QColor(187, 206, 239);
	QColor Grid = QColor(46, 44, 119);

	m_pCPUPlot = new CIncrementalPlot(Back, Front, Grid);
	m_pCPUPlot->setMinimumHeight(120);
	m_pCPUPlot->setMinimumWidth(50);
	m_pCPUPlot->SetupLegend(Front, tr("CPU Usage"), CIncrementalPlot::eDate);
	m_pCPUPlot->SetRagne(100);
	m_pScrollLayout->addWidget(m_pCPUPlot, 0, 0, 1, 3);

	QVector<QColor> Colors = theGUI->GetPlotColors();
	for (int i = 0; i < theAPI->GetCpuCount(); i++)
	{
		m_pCPUPlot->AddPlot("Cpu_" + QString::number(i), Colors[i % Colors.size()], Qt::SolidLine, false, tr("CPU %1").arg(i));
		m_pCPUPlot->AddPlot("CpuK_" + QString::number(i), Colors[i % Colors.size()], Qt::DotLine, false);
	}

	m_CpuSmoother.resize(theAPI->GetCpuCount());

	// todo: xxx - add option to show only one cpu per graph

	m_pInfoWidget = new QWidget();
	m_pInfoLayout = new QHBoxLayout();
	m_pInfoLayout->setMargin(0);
	m_pInfoWidget->setLayout(m_pInfoLayout);
	m_pScrollLayout->addWidget(m_pInfoWidget, 1, 0, 1, 3);

	m_pInfoWidget1 = new QWidget();
	m_pInfoLayout1 = new QVBoxLayout();
	m_pInfoLayout1->setMargin(0);
	m_pInfoWidget1->setLayout(m_pInfoLayout1);
	m_pInfoLayout->addWidget(m_pInfoWidget1);

	m_pInfoWidget2 = new QWidget();
	m_pInfoLayout2 = new QVBoxLayout();
	m_pInfoLayout2->setMargin(0);
	m_pInfoWidget2->setLayout(m_pInfoLayout2);
	m_pInfoLayout->addWidget(m_pInfoWidget2);

	m_pInfoLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Expanding, QSizePolicy::Minimum));

	////////////////////////////////////////

	m_pCPUBox = new QGroupBox(tr("CPU Info"));
	m_pCPUBox->setMinimumWidth(200);
	m_pCPULayout = new QGridLayout();
	m_pCPUBox->setLayout(m_pCPULayout);
	m_pInfoLayout1->addWidget(m_pCPUBox);

	int row = 0;
	m_pCPULayout->addWidget(new QLabel(tr("Utilization:")), row, 0);
	m_pCPUUsage = new QLabel();
	m_pCPULayout->addWidget(m_pCPUUsage, row++, 1);

	m_pCPULayout->addWidget(new QLabel(tr("Clock Speed:")), row, 0);
	m_pCPUSpeed = new QLabel();
	m_pCPULayout->addWidget(m_pCPUSpeed, row++, 1);

	m_pCPULayout->addWidget(new QLabel(tr("NUMA Count:")), row, 0);
	m_pCPUNumaCount = new QLabel();
	m_pCPULayout->addWidget(m_pCPUNumaCount, row++, 1);

	m_pCPULayout->addWidget(new QLabel(tr("CPU Count:")), row, 0);
	m_pCPUCoreCount = new QLabel();
	m_pCPULayout->addWidget(m_pCPUCoreCount, row++, 1);

	////////////////////////////////////////

	m_pOtherBox = new QGroupBox(tr("Other Info"));
	m_pOtherBox->setMinimumWidth(200);
	m_pOtherLayout = new QGridLayout();
	m_pOtherBox->setLayout(m_pOtherLayout);
	m_pInfoLayout2->addWidget(m_pOtherBox);

	row = 0;
	m_pOtherLayout->addWidget(new QLabel(tr("Context switches:")), row, 0);
	m_pSwitches = new QLabel();
	m_pOtherLayout->addWidget(m_pSwitches, row++, 1);

	m_pOtherLayout->addWidget(new QLabel(tr("Interrupts:")), row, 0);
	m_pInterrupts = new QLabel();
	m_pOtherLayout->addWidget(m_pInterrupts, row++, 1);

	m_pOtherLayout->addWidget(new QLabel(tr("DPCs:")), row, 0);
	m_pDPCs = new QLabel();
	m_pOtherLayout->addWidget(m_pDPCs, row++, 1);

	m_pOtherLayout->addWidget(new QLabel(tr("System calls:")), row, 0);
	m_pSysCalls = new QLabel();
	m_pOtherLayout->addWidget(m_pSysCalls, row++, 1);

	////////////////////////////////////////
}

CCPUView::~CCPUView()
{
}


void CCPUView::Refresh()
{
	m_pCPUModel->setText(theAPI->GetCpuModel());

	m_pCPUUsage->setText(tr("%1%").arg(int(100 * theAPI->GetCpuUsage())));
	m_pCPUSpeed->setText(tr("%1 / %2 GHz").arg(theAPI->GetCpuCurrentClock(), 2, 'g', 3).arg(theAPI->GetCpuBaseClock(), 2, 'g', 3));
	m_pCPUNumaCount->setText(tr("%1 sockets / %2 nodes").arg(theAPI->GetPackageCount()).arg(theAPI->GetNumaCount()));
	m_pCPUCoreCount->setText(tr("%1 threads / %2 cores").arg(theAPI->GetCpuCount()).arg(theAPI->GetCoreCount()));

	SCpuStatsEx Stats = theAPI->GetCpuStats();

	m_pSwitches->setText(tr("%1 / %2").arg(FormatNumber(Stats.ContextSwitchesDelta.Delta)).arg(FormatNumber(Stats.ContextSwitchesDelta.Value)));
	m_pInterrupts->setText(tr("%1 / %2").arg(FormatNumber(Stats.InterruptsDelta.Delta)).arg(FormatNumber(Stats.InterruptsDelta.Value)));
	m_pDPCs->setText(tr("%1 / %2").arg(FormatNumber(Stats.DpcsDelta.Delta)).arg(FormatNumber(Stats.DpcsDelta.Value)));
	m_pSysCalls->setText(tr("%1 / %2").arg(FormatNumber(Stats.SystemCallsDelta.Delta)).arg(FormatNumber(Stats.SystemCallsDelta.Value)));
}

void CCPUView::UpdateGraphs()
{
	for (int i = 0; i < theAPI->GetCpuCount(); i++)
	{
		SCpuStats Stats = theAPI->GetCpuStats(i);
		m_pCPUPlot->AddPlotPoint("Cpu_" + QString::number(i), m_CpuSmoother[i].first.Smooth(100 * (Stats.KernelUsage + Stats.UserUsage)));
		m_pCPUPlot->AddPlotPoint("CpuK_" + QString::number(i), m_CpuSmoother[i].second.Smooth(100 * (Stats.KernelUsage)));
	}
}