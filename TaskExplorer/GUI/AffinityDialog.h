#pragma once
#include "../Common/TreeViewEx.h"
#include "..\API\ProcessInfo.h"


class CAffinityDialog : public QDialog
{
	Q_OBJECT

public:
	CAffinityDialog(int CPUCount, QWidget* parent = 0);
	virtual ~CAffinityDialog();

	void		SetName(const QString& name);

	QVector<int>	GetAffinity() const;
	void		SetAffinity(const QVector<int>& Affinity) const;

private slots:
	void		OnSelectAll();
	void		OnDeselectAll();

private:
	QGridLayout*			m_pMainLayout;

	QList<QCheckBox*>		m_CheckBoxes;

	QPushButton*			m_pSelectAll;
	QPushButton*			m_pDeselectAll;

	QDialogButtonBox *		m_pButtonBox;
};