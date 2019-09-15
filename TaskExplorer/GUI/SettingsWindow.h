#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_SettingsWindow.h"

class CSettingsWindow : public QMainWindow
{
	Q_OBJECT

public:
	CSettingsWindow(QWidget *parent = Q_NULLPTR);
	~CSettingsWindow();

signals:
	void OptionsChanged();

public slots:
	void apply();
	void accept();
	void reject();

private slots:
	void OnChangeColor(QListWidgetItem* pItem);
	void OnChange();

protected:
	void closeEvent(QCloseEvent *e);

private:
	Ui::SettingsWindow ui;
};
