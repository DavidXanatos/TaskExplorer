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

	void OnSelectUiFont();
	void OnResetUiFont();

	void GetUpdates();
	void OnUpdateData(const QVariantMap& Data, const QVariantMap& Params);
	void OnUpdate(const QString& Channel);
	void UpdateUpdater();

	void OnTab();

protected:
	void closeEvent(QCloseEvent *e);

	QVariantMap m_UpdateData;

private:
	Ui::SettingsWindow ui;
};
