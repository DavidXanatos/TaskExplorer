#include "stdafx.h"
#include "SmartGridWidget.h"
#include <math.h>

CSmartGridWidget::CSmartGridWidget(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QGridLayout();
	this->setLayout(m_pMainLayout);

	m_pMainLayout->setContentsMargins(1,1,1,1);
	m_pMainLayout->setSpacing(2);

	m_bReArangePending = false;
}

void CSmartGridWidget::SetBackground(const QColor& BackColor)
{
	QPalette pal = palette();
	pal.setColor(QPalette::Window, BackColor);
	this->setAutoFillBackground(true);
	this->setPalette(pal);
}

void CSmartGridWidget::AddWidget(QWidget* pWidget)
{
	m_Widgets.append(pWidget);
	if(!pWidget->parent())
		pWidget->setParent(this);

	if (!m_bReArangePending)
	{
		m_bReArangePending = true;
		QTimer::singleShot(0, this, SLOT(ReArange()));
	}
}

void CSmartGridWidget::ReArange()
{
	m_bReArangePending = false;

	int count = 0;
	while (count < m_Widgets.size())
	{
		if (m_Widgets[count] == NULL)
			m_Widgets.removeAt(count);
		else
			count++;
	}

	//
	// double rather than float deliberately: the float overload resolves to
	// sqrtf, and glibc 2.43 introduced a new symbol version of that function.
	// Linking against it raised the binary's minimum glibc from 2.34 to 2.43 -
	// i.e. from "most distributions of the last few years" to "only the newest"
	// - for one square root of a small integer. sqrt(double) has carried the
	// same GLIBC_2.2.5 version since forever.
	//
	float columns = ceil(sqrt((double)count));
	float rows = columns > 0 ? ceil(count / columns) : 0;

	for (int row = 0; row < rows; row++)
	{
		for (int column = 0; column < columns; column++)
		{
			int index = row * columns + column;
			if (index >= count)
				break;

			m_pMainLayout->addWidget(m_Widgets.at(index), row, column);
		}
	}
}