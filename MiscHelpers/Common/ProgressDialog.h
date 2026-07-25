#pragma once

#include "../mischelpers_global.h"
#include "Common.h"

class MISCHELPERS_EXPORT CProgressDialog : public QDialog
{
	Q_OBJECT

public:
	CProgressDialog(const QString& Prompt, QWidget* parent = 0)
	 : QDialog(parent)
	{
		setWindowFlags(Qt::Tool);

		//m_pMainWidget = new QWidget();
		m_pMainLayout = new QGridLayout(this);
		this->setLayout(m_pMainLayout);
		//m_pMainWidget->setLayout(m_pMainLayout);
		//this->setCentralWidget(m_pMainWidget);
 
		m_pMessageLabel = new QLabel(Prompt);
		m_pMessageLabel->setMinimumWidth(300);
		//m_pMessageLabel->setMinimumHeight(40);
		m_pMessageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
		m_pMessageLabel->setWordWrap(true);
		//m_pMessageLabel->setAlignment(Qt::AlignTop);
		m_pMainLayout->addWidget(m_pMessageLabel, 0, 0, 1, 1);

		m_pProgressBar = new QProgressBar();
		m_pProgressBar->setTextVisible(false);
		m_pProgressBar->setMaximum(0);
		m_pProgressBar->setMinimum(0);
		m_pMainLayout->addWidget(m_pProgressBar, 1, 0, 1, 1);

		m_pButtonBox = new QDialogButtonBox();
		m_pButtonBox->setOrientation(Qt::Horizontal);
		m_pButtonBox->setStandardButtons(QDialogButtonBox::Cancel);
		m_pMainLayout->addWidget(m_pButtonBox, 2, 0, 1, 1);
 
		//setFixedSize(sizeHint());

		connect(m_pButtonBox,SIGNAL(rejected()),this,SLOT(OnCancel()));

		m_TimerId = startTimer(1000);
		m_CountDown = 0;
	}
	~CProgressDialog()
	{
		killTimer(m_TimerId);
	}

	void		ResetCanceled()		{ m_Cancelled = false; }
	bool		IsCancelled() const { return m_Cancelled; }

signals:
	void		Cancel();

public slots:
	void		Show(bool bWatchEsc = false)
	{
		m_bWatchEsc = bWatchEsc;
		SafeShow(this);
		SetFocus(this);
	}

	void		Exec(bool bWatchEsc = false)
	{
		m_bWatchEsc = bWatchEsc;
		exec();
	}

	void		ShowProgress(const QString& Message, int Progress = -1)
	{
		if(!Message.isEmpty())
			m_pMessageLabel->setText(Message);

		if (Progress == -1)
		{
			if (m_pProgressBar->maximum() != 0)
				m_pProgressBar->setMaximum(0);
		}
		else
		{
			if (m_pProgressBar->maximum() != 100)
				m_pProgressBar->setMaximum(100);

			m_pProgressBar->setValue(Progress);
		}
	}

	void		ShowStatus(const QString& Message, int Code = 0)
	{
		//if(Code == 0)
			m_pMessageLabel->setText(Message);
		//else // note: parent can't be set as this window may auto close
		//	QMessageBox::warning(NULL, this->windowTitle(), Message); 
	}

	void		OnFinished(int CountDown = 0)
	{
		m_Finished = true;
		if(!CountDown)
			close();
		else
		{
			m_pButtonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
			m_CountDown = CountDown;
		}
	}

private slots:

	void OnCancel()
	{
		m_Cancelled = true;
		emit Cancel();
	}

protected:
	void timerEvent(QTimerEvent *e)
	{
		if (e->timerId() != m_TimerId) 
		{
			QDialog::timerEvent(e);
			return;
		}
		
		if(m_CountDown != 0)
		{
			m_CountDown--;
			m_pButtonBox->button(QDialogButtonBox::Cancel)->setText(tr("Close (%1)").arg(m_CountDown));
			if (m_CountDown == 0)
				close();
		}
	}

	void keyPressEvent(QKeyEvent *event)
	{
		if (event->key() == Qt::Key_Escape && m_bWatchEsc)
			m_Cancelled = true;

		QWidget::keyPressEvent(event);
	}

	void closeEvent(QCloseEvent *e)
	{
		if(!m_Finished)
			OnCancel();
	}

	int					m_TimerId = 0;
	int					m_CountDown = 0;
	bool				m_bWatchEsc = false;
	bool				m_Cancelled = false;
	bool				m_Finished = false;

	QWidget*			m_pMainWidget = nullptr;
	QGridLayout*		m_pMainLayout = nullptr;
	QLabel*				m_pMessageLabel = nullptr;
	QProgressBar*		m_pProgressBar = nullptr;
	QDialogButtonBox*	m_pButtonBox = nullptr;
};

typedef QSharedPointer<CProgressDialog> CProgressDialogPtr;

class MISCHELPERS_EXPORT CProgressDialogHelper
{
public:
	CProgressDialogHelper(const QString& Prompt, int TotalCount, QWidget* parent = nullptr)
	{
		m_pDialog = CProgressDialogPtr(new CProgressDialog("", parent));
		m_pDialog->ResetCanceled();
		m_Prompt = Prompt;
		m_LastTime = GetCurTick();
		m_TotalCount = TotalCount;
	}

	bool Next(const QString& Name)
	{
		quint64 ElapsedTimeMs = GetCurTick() - m_LastTime;
		m_ProcessedCount++;
		if (m_pDialog->isVisible()) {
			if (ElapsedTimeMs > 250) {
				m_LastTime = GetCurTick();
				m_pDialog->ShowProgress(m_Prompt.arg(Name), 100 * m_ProcessedCount / m_TotalCount);
				QCoreApplication::processEvents();
			}
		} else if(ElapsedTimeMs > 500)
			m_pDialog->Show(true);

		return !m_pDialog->IsCancelled();
	}

	bool Done()
	{
		if (!m_pDialog->isVisible())
			return false;
		m_pDialog->hide();
		return true;
	}

protected:
	CProgressDialogPtr m_pDialog;
	QString m_Prompt;
	quint64 m_LastTime = 0;
	int m_TotalCount = 0;
	int m_ProcessedCount = 0;
};