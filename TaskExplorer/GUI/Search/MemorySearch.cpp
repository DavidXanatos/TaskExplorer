#include "stdafx.h"
#include "MemorySearch.h"
#include "../TaskExplorer.h"
#include "../../API/Finders/AbstractFinder.h"
#ifdef WIN32
#include "../../API/Windows/ProcessHacker.h"
#endif

CMemorySearch::CMemorySearch(const CProcessPtr& pProcess, QWidget *parent) 
	: CSearchWindow(parent)
{
	setObjectName("MemorySearch");

	m_CurProcess = pProcess;

	this->setWindowTitle(tr("Memory search..."));

	m_pType->show();
	m_pType->addItem(tr("Strings"), 0);
	m_pType->addItem(tr("Raw Hex"), 1);
	connect(m_pType, SIGNAL(currentIndexChanged(int)), this, SLOT(OnType()));

	m_pStringWidget = new QWidget();
	m_pStringLayout = new QHBoxLayout();
	m_pStringLayout->setMargin(3);
	m_pStringWidget->setLayout(m_pStringLayout);
	m_pMainLayout->insertWidget(1, m_pStringWidget);

	m_pStringLayout->addWidget(new QLabel(tr("Minimum length:")));
	m_pMinLength = new QSpinBox();
	m_pMinLength->setMinimum(4);
	m_pMinLength->setMaximum(1024);
	m_pStringLayout->addWidget(m_pMinLength);

	m_pUnicode = new QCheckBox(tr("Unicode"));
	m_pStringLayout->addWidget(m_pUnicode);

	m_pExtUnicode = new QCheckBox(tr("Extended Unicode"));
	m_pStringLayout->addWidget(m_pExtUnicode);

	m_pStringLayout->addWidget(m_pRegExp); // move reg exp checkbox from the generic search widget to the string sidget

	// add region options to the search widget
	int pos = m_pFinderLayout->count() - 1;

	m_pFinderLayout->insertWidget(pos++, new QLabel(tr("Regions:")));

	m_pPrivate = new QCheckBox(tr("Private"));
	m_pFinderLayout->insertWidget(pos++, m_pPrivate);

	m_pImage = new QCheckBox(tr("Image"));
	m_pFinderLayout->insertWidget(pos++, m_pImage);

	m_pMapped = new QCheckBox(tr("Mapped"));
	m_pFinderLayout->insertWidget(pos++, m_pMapped);
	//

	m_pStringLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));


	m_pStringView = new CStringView(pProcess.isNull(), this);
	m_pMainLayout->addWidget(m_pStringView);


	m_pMinLength->setValue(theConf->GetInt("MemorySearch/MinLength", 2));
	m_pUnicode->setChecked(theConf->GetBool("MemorySearch/Unicode", true));
	m_pExtUnicode->setChecked(theConf->GetBool("MemorySearch/ExtUnicode", false));

	m_pPrivate->setChecked(theConf->GetBool("MemorySearch/Private", true));
	m_pImage->setChecked(theConf->GetBool("MemorySearch/Image", false));
	m_pMapped->setChecked(theConf->GetBool("MemorySearch/Mapped", false));

	m_pType->setCurrentIndex(m_pType->findText(theConf->GetString("MemorySearch/Type", "")));
	if(m_pType->currentIndex() == -1)
		m_pType->setCurrentIndex(0);

	restoreGeometry(theConf->GetBlob("MemorySearch/Window_Geometry"));
}

CMemorySearch::~CMemorySearch()
{
	theConf->SetValue("MemorySearch/MinLength", m_pMinLength->value());
	theConf->SetValue("MemorySearch/Unicode", m_pUnicode->isChecked());
	theConf->SetValue("MemorySearch/ExtUnicode", m_pExtUnicode->isChecked());

	theConf->SetValue("MemorySearch/Private", m_pPrivate->isChecked());
	theConf->SetValue("MemorySearch/Image", m_pImage->isChecked());
	theConf->SetValue("MemorySearch/Mapped", m_pMapped->isChecked());

	theConf->SetValue("MemorySearch/Type", m_pType->currentText());

	theConf->SetBlob("MemorySearch/Window_Geometry",saveGeometry());
}

void CMemorySearch::OnType()
{
	m_pStringWidget->setEnabled(m_pType->currentData().toInt() != 1);
}

CAbstractFinder* CMemorySearch::NewFinder()
{
	m_Strings.clear();
	m_pStringView->ShowStrings(m_Strings);

	if (m_pType->currentData().toInt() == 1)
	{
		QByteArray hexStr = m_pSearch->text().toLatin1().simplified().replace(" ", "");
		QByteArray hex = QByteArray::fromHex(hexStr);
		if (hex.length() < 2 || hex.length() != hexStr.length() / 2) {
			QMessageBox::warning(this, tr("TaskExplorer"), tr("Invalid Hex String, or shorter than 2 bytes."));
			return NULL;
		}
	}

	QRegExp RegExp = QRegExp(m_pSearch->text(), Qt::CaseInsensitive, m_pRegExp->isChecked() ? QRegExp::RegExp : QRegExp::FixedString);

	CAbstractFinder::SMemOptions Options;
	if (m_pType->currentData().toInt() == 1)
		Options.MinLength = -1; // hex search
	else
		Options.MinLength = m_pMinLength->value();
	Options.Unicode = m_pUnicode->isChecked();
	Options.ExtUnicode = m_pExtUnicode->isChecked();

	Options.Private = m_pPrivate->isChecked();
	Options.Image = m_pImage->isChecked();
	Options.Mapped = m_pMapped->isChecked();

	return CAbstractFinder::FindStrings(Options, RegExp, m_CurProcess);
}

void CMemorySearch::OnResults(QList<QSharedPointer<QObject>> List)
{
	foreach(const QSharedPointer<QObject>& pObject, List)
	{
		CStringInfoPtr pModule = pObject.staticCast<CStringInfo>();
		m_Strings.insert(m_Strings.count(), pModule);
	}

	if (!CheckCountAndAbbort(m_Strings.count()))
		return;

	m_pStringView->ShowStrings(m_Strings);
}
