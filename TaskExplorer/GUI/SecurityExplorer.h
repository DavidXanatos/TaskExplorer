#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_SecurityExplorer.h"

class CSecurityExplorer : public QMainWindow
{
	Q_OBJECT

public:
	CSecurityExplorer(QWidget *parent = Q_NULLPTR);
	~CSecurityExplorer();

public slots:
	void OnSecPolEdit();
	void OnAccount(QTreeWidgetItem* pItem);
	void OnUser(QTreeWidgetItem* pItem);
	void OnGroupe(QTreeWidgetItem* pItem);
	//void OnDeleteAccount(QTreeWidgetItem* pItem);

	//void accept();
	void reject();

protected:
	void LoadAccounts();
	void LoadPriviledges();
	void LoadSessions();
	void LoadUsers();
	void LoadGroups();
	void LoadCredentials();

	void closeEvent(QCloseEvent *e);

private:
	Ui::SecurityExplorer ui;
};
