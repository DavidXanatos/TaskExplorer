#pragma once

#include <QtWidgets/QMainWindow>
#include "../API/Windows/WinProcess.h"
#include "../Common/PanelView.h"

class CWsWatchDialog : public QMainWindow
{
	Q_OBJECT

public:
	CWsWatchDialog(const CProcessPtr& pProcess, QWidget *parent = Q_NULLPTR);
	~CWsWatchDialog();

public slots:
	//void accept();
	void reject();
	void OnEnable();

private slots:
	void OnSymbolFromAddress(quint64 ProcessId, quint64 Address, int ResolveLevel, const QString& StartAddressString, const QString& FileName, const QString& SymbolName);

protected:
	bool Refresh();

	void closeEvent(QCloseEvent *e);
	void timerEvent(QTimerEvent *e);

	int					m_TimerId;

	QMap<quint64, QTreeWidgetItem*> m_FailtList;

private:
	QWidget*			m_pMainWidget;
    QGridLayout*		m_pMainLayout;
	QLabel*				m_pInfoLabel;
    QPushButton*		m_pEnableBtn;
	QLabel*				m_pEnabledLbl;
	CPanelWidgetEx*		m_pFaultList;
	QDialogButtonBox*	m_pButtonBox;
    
	struct SWorkingSetWatch* m;
};
