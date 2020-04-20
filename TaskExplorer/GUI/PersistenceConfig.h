#pragma once

#include <QTableWidget>

#include "../API/SystemAPI.h"

class CPersistenceConfig : public QDialog
{
	Q_OBJECT

public:
	CPersistenceConfig(QWidget *parent = Q_NULLPTR);
	virtual ~CPersistenceConfig();


public slots:
	void accept();
	void reject();

	void OnCmdLine();
	void OnPermission(int index);
	void OnCPUPriority(int index);
	void OnCPUAffinity();
	void OnIOPriority(int index);
	void OnPagePriority(int index);
	void OnRemovePreset();
	void OnAddPreset();

protected:
	void closeEvent(QCloseEvent *e);

	void LoadPersistentList();
	void StorePersistentList();

	QList<CPersistentPresetDataPtr> m_PersistentPreset;

private:
	enum eColumns
	{
		eCmdLine = 0,
		ePermission,
		eCPUPriority,
		eCPUAffinity,
		eIOPriority,
		ePagePriority,
		eRemovePreset,
		eColumnCount
	};

	void LoadPersistent(const CPersistentPresetDataPtr& pPreset, int RowCounter);

	CPersistentPresetDataPtr* FindPreset(int Column);

	QGridLayout*		m_pMainLayout;

	QTableWidget*		m_pPresets;

	QDialogButtonBox *	m_pButtonBox;

	int					m_ItemHeight;
};
