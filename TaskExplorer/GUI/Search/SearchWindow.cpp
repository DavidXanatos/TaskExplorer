#include "stdafx.h"
#include "SearchWindow.h"
#include "../TaskExplorer.h"
#include "../../API/Finders/AbstractFinder.h"

CSearchWindow::CSearchWindow(QWidget *parent) 
	: QMainWindow(parent)
{
	setObjectName("SearchWindow");

	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	m_pMainWidget->setLayout(m_pMainLayout);
	this->setCentralWidget(m_pMainWidget);

	m_pFinderWidget = new QWidget();
	m_pFinderLayout = new QHBoxLayout();
	m_pFinderLayout->setMargin(3);
	m_pFinderWidget->setLayout(m_pFinderLayout);
	m_pMainLayout->addWidget(m_pFinderWidget);


	m_pType = new QComboBox();
	m_pFinderLayout->addWidget(m_pType);
	//connect(m_pType, SIGNAL(currentIndexChanged(int)), this, SLOT(OnUpdate()));


	m_pSearch = new QLineEdit();
	m_pSearch->setMinimumWidth(150);
	m_pSearch->setMaximumWidth(350);
	m_pFinderLayout->addWidget(m_pSearch);
	//QObject::connect(m_pSearch, SIGNAL(textChanged(QString)), this, SLOT(OnUpdate()));
	connect(m_pSearch, SIGNAL(returnPressed()), this, SLOT(OnFind()));

	m_pRegExp = new QCheckBox(tr("RegExp"));
	m_pFinderLayout->addWidget(m_pRegExp);
	//connect(m_pRegExp, SIGNAL(stateChanged(int)), this, SLOT(OnUpdate()));
	
	m_pFinderLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));

	m_pFind = new QPushButton(tr("Search"));
	m_pFinderLayout->addWidget(m_pFind);
	connect(m_pFind, SIGNAL(pressed()), this, SLOT(OnFind()));


	m_pProgress = new QProgressBar();
	m_pProgress->setMaximumHeight(14);
	//m_pProgress->setTextVisible(false);
	statusBar()->addPermanentWidget(m_pProgress);
	m_pProgress->hide();

	m_pResultCount = new QLabel();
	statusBar()->addPermanentWidget(m_pResultCount);

	restoreGeometry(theConf->GetBlob("SearchWindow/Window_Geometry"));

	m_pFinder = NULL;
}

CSearchWindow::~CSearchWindow()
{
	if (m_pFinder)
		m_pFinder->Cancel();

	theConf->SetBlob("SearchWindow/Window_Geometry",saveGeometry());
}

void CSearchWindow::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CSearchWindow::OnFind()
{
	if (m_pFinder) {
		m_pFinder->Cancel();
		return;
	}

	m_pProgress->setValue(0);
	m_pProgress->show();

	m_pFinder = NewFinder();
	if (m_pFinder == NULL)
		return;

	QObject::connect(m_pFinder, SIGNAL(Progress(float, const QString&)), this, SLOT(OnProgress(float, const QString&)));
	QObject::connect(m_pFinder, SIGNAL(Results(QList<QSharedPointer<QObject>>)), this, SLOT(OnResults(QList<QSharedPointer<QObject>>)));
	QObject::connect(m_pFinder, SIGNAL(Error(const QString&, int)), this, SLOT(OnError(const QString&, int)));
	QObject::connect(m_pFinder, SIGNAL(Finished()), this, SLOT(OnFinished()));

	QObject::connect(m_pFinder, SIGNAL(Finished()), m_pFinder, SLOT(deleteLater()));

	m_pFinder->start();

	m_pFind->setText(tr("Cancel"));
}

void CSearchWindow::OnProgress(float value, const QString& Info)
{
	if(!Info.isEmpty())
		statusBar()->showMessage(tr("Searching %1").arg(Info));
	m_pProgress->setValue(100.0f*value);
}

bool CSearchWindow::CheckCountAndAbbort(int Count)
{
	int Limit = theConf->GetInt("Options/ResultLimit", 2000000);
	if (Count <= Limit) 
	{
		m_pResultCount->setText(tr("Results: %1").arg(Count));
		return true;
	}
	
	if (!m_pFinder->IsCanceled())
	{
		m_pFinder->Cancel();
		QMessageBox::critical(this, tr("TaskExplorer"), tr("Result limit, of %1, reached.\r\n"
			"Search canceled to prevent the UI from becoming unusably slow.\r\n\r\n"
			"To change the limit adjust the option 'Options/ResultLimit'.").arg(FormatNumber(Limit)));
	}
	return false;
}

void CSearchWindow::OnError(const QString& Error, int Code)
{
	STATUS Status = ERR(Error, Code);
	CTaskExplorer::CheckErrors(QList<STATUS>() << Status);
}

void CSearchWindow::OnFinished()
{
	m_pFinder->deleteLater();
	QObject::disconnect(this, SLOT(Progress(float)));
	QObject::disconnect(this, SLOT(Results(const QList<QSharedPointer<QObject>>&)));
	QObject::disconnect(this, SLOT(Finished()));
	m_pFinder = NULL;

	m_pFind->setText(tr("Search"));
	m_pProgress->hide();
}
