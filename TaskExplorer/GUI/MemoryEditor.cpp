#include "stdafx.h"
#include "MemoryEditor.h"
#include "TaskExplorer.h"

CMemoryEditor::CMemoryEditor(QWidget *parent)
	: QHexEditor(parent)
{
	m_pDevice = NULL;

	init();
	emptyAct->setEnabled(false);
	hexEdit->setDisableInsert(true);

	this->setWindowTitle(tr("Memory Editor"));
}

CMemoryEditor::~CMemoryEditor()
{
	
}

void CMemoryEditor::readSettings()
{
    QPoint pos = theConf->GetValue("MemoryEditor/pos", QPoint(200, 200)).toPoint();
    QSize size = theConf->GetValue("MemoryEditor/size", QSize(610, 460)).toSize();
    move(pos);
    resize(size);

    hexEdit->setAddressArea(theConf->GetValue("MemoryEditor/AddressArea", true).toBool());
    hexEdit->setAsciiArea(theConf->GetValue("MemoryEditor/AsciiArea", true).toBool());
    hexEdit->setHighlighting(theConf->GetValue("MemoryEditor/Highlighting", true).toBool());
    hexEdit->setOverwriteMode(theConf->GetValue("MemoryEditor/OverwriteMode", true).toBool());
    hexEdit->setReadOnly(theConf->GetValue("MemoryEditor/ReadOnly", false).toBool());
	
    hexEdit->setHighlightingColor(theConf->GetValue("MemoryEditor/HighlightingColor", QColor(0xff, 0xff, 0x99, 0xff)).value<QColor>());
    hexEdit->setAddressAreaColor(theConf->GetValue("MemoryEditor/AddressAreaColor", this->palette().alternateBase().color()).value<QColor>());
    hexEdit->setSelectionColor(theConf->GetValue("MemoryEditor/SelectionColor", this->palette().highlight().color()).value<QColor>());
#ifdef Q_OS_WIN32
    hexEdit->setFont(theConf->GetValue("MemoryEditor/WidgetFont", QFont("Courier", 10)).value<QFont>());
#else
    hexEdit->setFont(theConf->GetValue("MemoryEditor/WidgetFont", QFont("Monospace", 10)).value<QFont>());
#endif

    hexEdit->setAddressWidth(theConf->GetValue("MemoryEditor/AddressAreaWidth", 16).toInt());
    hexEdit->setBytesPerLine(theConf->GetValue("MemoryEditor/BytesPerLine", 16).toInt());
}

void CMemoryEditor::writeSettings()
{
    theConf->SetValue("MemoryEditor/pos", pos());
    theConf->SetValue("MemoryEditor/size", size());

    theConf->SetValue("MemoryEditor/AddressArea", hexEdit->addressArea());
    theConf->SetValue("MemoryEditor/AsciiArea", hexEdit->asciiArea());
    theConf->SetValue("MemoryEditor/Highlighting", hexEdit->highlighting());
    theConf->SetValue("MemoryEditor/OverwriteMode", hexEdit->overwriteMode());
    theConf->SetValue("MemoryEditor/ReadOnly", hexEdit->isReadOnly());

    theConf->SetValue("MemoryEditor/HighlightingColor", hexEdit->highlightingColor());
    theConf->SetValue("MemoryEditor/AddressAreaColor", hexEdit->addressAreaColor());
    theConf->SetValue("MemoryEditor/SelectionColor", hexEdit->selectionColor());
    theConf->SetValue("MemoryEditor/WidgetFont", hexEdit->font());

    theConf->SetValue("MemoryEditor/AddressAreaWidth", hexEdit->addressWidth());
    theConf->SetValue("MemoryEditor/BytesPerLine", hexEdit->bytesPerLine());
}

bool CMemoryEditor::open()
{
	return hexEdit->refreshData();
}

bool CMemoryEditor::save()
{
	QList<QHexEditChunk> chunks = hexEdit->getChunks();
	if (chunks.isEmpty())
		return true; // nothing to do

	if (QMessageBox("TaskExplorer", "Modifying memory regions may cause the process to crash.", QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return false;

	if (!m_pDevice->open(QIODevice::ReadWrite))
		return false;

	int ErrorCount = 0;
	foreach(const QHexEditChunk& chunk, chunks)
	{
		ASSERT(chunk.data.size() == chunk.initSize); // no insert mode with memory editing

		if (!m_pDevice->seek(chunk.absPos) || m_pDevice->write(chunk.data) != chunk.data.size())
			ErrorCount++;
	}

	m_pDevice->close();

	if (ErrorCount > 0) {
		QMessageBox("TaskExplorer", tr("Failed to write %1 chunks to memory").arg(ErrorCount), QMessageBox::Critical, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton).exec();
		return false;
	}

	return hexEdit->refreshData();
}

void CMemoryEditor::setDevice(QIODevice* pDevice, quint64 BaseAddress)
{
	m_pDevice = pDevice;

	pDevice->setParent(this);
	hexEdit->setData(pDevice);
	hexEdit->setAddressOffset(BaseAddress);
}