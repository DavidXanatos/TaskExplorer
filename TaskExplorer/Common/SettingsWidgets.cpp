#include "stdafx.h"
#include "SettingsWidgets.h"
#include "Common/CheckableMessageBox.h"

///////////////////////////////////////////////////
// CPathEdit

CPathEdit::CPathEdit(bool bDirs, QWidget *parent)
 : CTxtEdit(parent) 
{
	m_bDirs = bDirs;

	QHBoxLayout* pLayout = new QHBoxLayout(this);
	pLayout->setMargin(0);
	m_pEdit = new QLineEdit(this);
	connect(m_pEdit, SIGNAL(textChanged(const QString &)), this, SIGNAL(textChanged(const QString &)));
	pLayout->addWidget(m_pEdit);
	QPushButton* pButton = new QPushButton("...");
	pButton->setMaximumWidth(25);
	connect(pButton, SIGNAL(pressed()), this, SLOT(Browse()));
	pLayout->addWidget(pButton);
}

void CPathEdit::Browse()
{
	QString FilePath = m_bDirs
		? QFileDialog::getExistingDirectory(this, tr("Select Directory"))
		: QFileDialog::getOpenFileName(0, tr("Browse"), "", QString("Any File (*.*)"));
	if(!FilePath.isEmpty())
		SetText(FilePath);
}

///////////////////////////////////////////////////
// CProxyEdit

CProxyEdit::CProxyEdit(QWidget *parent)
 : CTxtEdit(parent) 
{
	QHBoxLayout* pLayout = new QHBoxLayout(this);
	pLayout->setMargin(0);

	m_pType = new QComboBox();
	m_pType->addItem(QString("No"));
	m_pType->addItem(QString("http"));
	m_pType->addItem(QString("socks5"));
	connect(m_pType, SIGNAL(activated(int)), this, SLOT(OnType(int)));
	pLayout->addWidget(m_pType);

	m_pEdit = new QLineEdit(this);
	connect(m_pEdit, SIGNAL(textChanged(const QString &)), this, SIGNAL(textChanged(const QString &)));
	pLayout->addWidget(m_pEdit);
}

void CProxyEdit::SetText(const QString& Text)
{
	QUrl Url(Text);
	m_pType->setCurrentText(Url.scheme());
	m_pEdit->setText(Text);
}

void CProxyEdit::OnType(int Index)
{
	if(Index == 0)
		m_pEdit->setEnabled(false);
	else
	{
		m_pEdit->setEnabled(true);
		m_pEdit->setText(m_pType->currentText() + "://");
	}
}

///////////////////////////////////////////////////
// CProxyEdit

CUSSEdit::CUSSEdit(QWidget *parent)
 : CTxtEdit(parent) 
{
	QHBoxLayout* pLayout = new QHBoxLayout(this);
	pLayout->setMargin(0);

	m_bHold = false;

	m_pEdit = new QLineEdit(this);
	m_pEdit->setValidator(new QIntValidator(0, INT_MAX, this));
	connect(m_pEdit, SIGNAL(textChanged(const QString &)), this, SIGNAL(textChanged(const QString &)));
	pLayout->addWidget(m_pEdit);

	m_pType = new QComboBox();
	m_pType->addItem(QString("Off"));
	m_pType->addItem(QString("Tolerance"));
	m_pType->addItem(QString("(%) Relative"));
	m_pType->addItem(QString("(ms) Absolute"));
	connect(m_pType, SIGNAL(activated(int)), this, SLOT(OnType(int)));
	pLayout->addWidget(m_pType);
}

void CUSSEdit::SetText(const QString& Text)
{
	QString Tolerance = Text;
	m_bHold = true;
	m_pEdit->setDisabled(Text.isEmpty());
	if(Tolerance.right(1) == "%")
	{
		Tolerance.truncate(Tolerance.length() - 1);
		m_pEdit->setText(Tolerance);
		m_pType->setCurrentIndex(2);
	}
	else if(Tolerance.right(2) == "ms")
	{
		Tolerance.truncate(Tolerance.length() - 2);
		m_pEdit->setText(Tolerance);
		m_pType->setCurrentIndex(3);
	}
	else
	{
		m_pEdit->setText(Tolerance);
		m_pType->setCurrentIndex(Text.isEmpty() ? 0 : 1);
	}
	m_bHold = false;
}

QString CUSSEdit::GetText()
{
	switch(m_pType->currentIndex())
	{
	case 0: return ""; // Off
	case 1: return m_pEdit->text(); // Tolerance
	case 2: return m_pEdit->text() + "%"; // Relative
	case 3: return m_pEdit->text() + "ms"; // Absolute
	}
	return "";
}

void CUSSEdit::OnType(int Index)
{
	if(m_bHold)
		return;

	m_pEdit->setDisabled(Index == 0);
	switch(Index)
	{
	case 0: m_pEdit->setText(""); break; // Off
	case 1: m_pEdit->setText("12"); break; // Tolerance
	case 2: m_pEdit->setText("150"); break; // Relative
	case 3: m_pEdit->setText("75"); break;// Absolute
	}
}


///////////////////////////////////////////////////
// QSortOption

QSortOptions::QSortOptions(QWidget* pParent) 
	: QComboBox(pParent) 
{
	// Yes
	// No
	// Custom
	//	File List
	//	File Transfer List
	//	Transfer List
	//	Search List
	//
	model = new QStandardItemModel(7, 1, this); // 3 rows, 1 col
	model->setItem(0, 0, new QStandardItem(tr("No")));
	model->setItem(1, 0, new QStandardItem(tr("Yes")));
	model->setItem(2, 0, new QStandardItem(tr("Custom")));
	QStringList Custom = tr("Lists|File Transfer List|Global Transfer List|Searches").split("|");
	for (int r = 3; r < 7; ++r)
	{
		QStandardItem* item = new QStandardItem(Custom[r-3]);
		item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
		item->setData(Qt::Unchecked, Qt::CheckStateRole);
		model->setItem(r, 0, item);
	}
	setModel(model);
	opts[3] = "F";
	opts[4] = "L";
	opts[5] = "T";
	opts[6] = "S";
}

void QSortOptions::SetValue(const QString& str)
{
	if(str == "false")
		setCurrentIndex(0);
	else if(str == "true")
		setCurrentIndex(1);
	else
	{
		setCurrentIndex(2);
		for (int r = 3; r < 7; ++r)
		{
			QStandardItem* item = model->item(r, 0);
			item->setData(str.contains(opts[r]) ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
		}
	}
}

QString QSortOptions::GetValue()
{
	switch(currentIndex())
	{
		case 0:	return "false";
		case 1:	return "true";
		default:
		{
			QString str;
			for (int r = 3; r < 7; ++r)
			{
				QStandardItem* item = model->item(r, 0);
				if(item->data(Qt::CheckStateRole) == Qt::Checked)
					str += opts[r];
			}
			return str;
		}
	}
}

///////////////////////////////////////////////////
// CAnonSlider

CAnonSlider::CAnonSlider(QWidget *parent)
:QWidget(parent)
{
	m_pSlider = new QSlider(Qt::Horizontal);
	m_pSlider->setMaximum(4);
	m_pSlider->setTickInterval(1);
	m_pSlider->setTickPosition(QSlider::TicksBelow);
	//connect(m_pSlider, SIGNAL(sliderMoved(int)), this, SLOT(OnSlider(int)));

	QVBoxLayout* pLayout = new QVBoxLayout();
	pLayout->setMargin(0);
	pLayout->addWidget(new QLabel(tr("Select Speed vs. Anonymity for NeoShare")));
	pLayout->addWidget(m_pSlider);
		QWidget* pVs = new QWidget();
		QHBoxLayout* pVsL = new QHBoxLayout();
		pVsL->setMargin(0);
		pVsL->addWidget(new QLabel("Speed"));
		QWidget* pSpacer = new QWidget();
		pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		pVsL->addWidget(pSpacer);
		pVsL->addWidget(new QLabel("Anonymity"));
		pVs->setLayout(pVsL);
	pLayout->addWidget(pVs);
	setLayout(pLayout);
}
