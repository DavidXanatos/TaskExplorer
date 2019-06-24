#include "stdafx.h"
#include "TaskExplorer.h"
#include "AffinityDialog.h"

CAffinityDialog::CAffinityDialog(int CPUCount, QWidget* parent)
	: QDialog(parent)
{
	m_pMainLayout = new QGridLayout(this);
	m_pMainLayout->addWidget(new QLabel(tr("Affinity controlls which CPUs tasks are allowed to be executed on.")), 0, 0, 1, 4);

	for (int j = 0; j < 4; j++)
	{
		for (int i = 1; i <= 16; i++)
		{
			QCheckBox* pCheck = new QCheckBox(tr("CPU %1").arg(m_CheckBoxes.size()));
			//pCheck->setTristate(true);
			m_CheckBoxes.append(pCheck);
			if (CPUCount < m_CheckBoxes.size())
				pCheck->setEnabled(false);
			m_pMainLayout->addWidget(pCheck, i, j);
		}
	}

	m_pSelectAll = new QPushButton(tr("Sselect all"));
	connect(m_pSelectAll, SIGNAL(pressed()), this, SLOT(OnSelectAll()));
	m_pMainLayout->addWidget(m_pSelectAll, 17, 0);

	m_pDeselectAll = new QPushButton(tr("Deselect all"));
	connect(m_pDeselectAll, SIGNAL(pressed()), this, SLOT(OnDeselectAll()));
	m_pMainLayout->addWidget(m_pDeselectAll, 17, 1);

	m_pButtonBox = new QDialogButtonBox();
	m_pButtonBox->setOrientation(Qt::Horizontal);
	m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
	m_pMainLayout->addWidget(m_pButtonBox, 18, 0, 1, 4);
 
	connect(m_pButtonBox,SIGNAL(accepted()),this,SLOT(accept()));
	connect(m_pButtonBox,SIGNAL(rejected()),this,SLOT(reject()));
}
 
CAffinityDialog::~CAffinityDialog()
{
}

void CAffinityDialog::OnSelectAll()
{
	foreach(QCheckBox* pCheck, m_CheckBoxes)
	{
		if (!pCheck->isEnabled())
			continue;
		pCheck->setChecked(true);
	}
}

void CAffinityDialog::OnDeselectAll()
{
	foreach(QCheckBox* pCheck, m_CheckBoxes)
		pCheck->setChecked(false);
}

void CAffinityDialog::SetName(const QString& name)
{
	this->setWindowTitle(tr("CPU Affinity for: %1").arg(name));
}

void CAffinityDialog::SetAffinity(const QVector<int>& Affinity) const
{
	for(int i=0; i < Affinity.size(); i++)
	{
		switch (Affinity[i])
		{
		case 0: m_CheckBoxes[i]->setCheckState(Qt::Unchecked); break;
		case 1:	m_CheckBoxes[i]->setCheckState(Qt::Checked); break;
		case 2:	m_CheckBoxes[i]->setCheckState(Qt::PartiallyChecked); break;
		}
	}
}

QVector<int> CAffinityDialog::GetAffinity() const
{
	QVector<int> Affinity(64, 2);
	for(int i=0; i < m_CheckBoxes.size(); i++)
	{
		switch (m_CheckBoxes[i]->checkState())
		{
		case Qt::Checked:	Affinity[i] = 1; break;
		//case Qt::PartiallyChecked:
		case Qt::Unchecked:	Affinity[i] = 0; break;
		}
	}
	return Affinity;
}
