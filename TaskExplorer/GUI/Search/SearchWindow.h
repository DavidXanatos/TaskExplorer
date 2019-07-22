#pragma once

class CAbstractFinder;

class CSearchWindow : public QMainWindow
{
	Q_OBJECT
public:
	CSearchWindow(QWidget *parent = Q_NULLPTR);
	virtual ~CSearchWindow();

private slots:
	virtual void		OnFind();

	virtual void		OnProgress(float value, const QString& Info);
	virtual void		OnResults(QList<QSharedPointer<QObject>> List) = 0;
	virtual void		OnError(const QString& Error, int Code);
	virtual void		OnFinished();

protected:
	virtual void closeEvent(QCloseEvent *e);

	virtual CAbstractFinder* NewFinder() = 0;

	bool CheckCountAndAbbort(int Count);

	CAbstractFinder*	m_pFinder;

	QWidget*			m_pMainWidget;
	QVBoxLayout*		m_pMainLayout;

	QWidget*			m_pFinderWidget;
	QHBoxLayout*		m_pFinderLayout;

	QComboBox*			m_pType;
	QLineEdit*			m_pSearch;
	QCheckBox*			m_pRegExp;
	QPushButton*		m_pFind;

	QProgressBar*		m_pProgress;
	QLabel*				m_pResultCount;
};