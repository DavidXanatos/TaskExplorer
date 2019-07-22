#pragma once

class CFinder: public QWidget
{
	Q_OBJECT

public:
	CFinder(QSortFilterProxyModel* pSortProxy, QWidget *parent = NULL);
	~CFinder();

signals:
	void				SetFilter(const QRegExp& Exp, int Column = -1);

public slots:
	void				Open();
	void				OnUpdate();
	void				Close();

private:

	QHBoxLayout*		m_pSearchLayout;

	QLineEdit*			m_pSearch;
	QCheckBox*			m_pCaseSensitive;
	QCheckBox*			m_pRegExp;
	QComboBox*			m_pColumn;

	QSortFilterProxyModel* m_pSortProxy;
};