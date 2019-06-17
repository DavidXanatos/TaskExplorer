#pragma once
#include <qwidget.h>
class CMemoryView : public QWidget
{
	Q_OBJECT
public:
	CMemoryView(QWidget *parent = 0);
	virtual ~CMemoryView();
};

