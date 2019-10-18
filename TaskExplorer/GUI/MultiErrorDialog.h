#pragma once
#include "../Common/PanelView.h"
#include "../Common/FlexError.h"

class CMultiErrorDialog : public QDialog
{
	Q_OBJECT

public:
	CMultiErrorDialog(const QString& Message, QList<STATUS> Errors, QWidget* parent = 0);
	virtual ~CMultiErrorDialog();

private:
	enum EColumns
	{
		eMessage,
		eErrorCode,
		eErrorText,
		eCount
	};

	QGridLayout*			m_pMainLayout;

	CPanelWidgetEx*			m_pErrors;

	QDialogButtonBox *		m_pButtonBox;
};