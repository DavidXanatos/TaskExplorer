#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QStatusBar>
#include <QLabel>
#include <QAction>
#include <QMenuBar>
#include <QToolBar>
#include <QColorDialog>
#include <QFontDialog>
#include <QDragEnterEvent>
#include <QDropEvent>

#include "HexEditor.h"

QHexEditor::QHexEditor(QWidget *parent)
	: QMainWindow(parent)
{
}

QHexEditor::~QHexEditor()
{
}

void QHexEditor::closeEvent(QCloseEvent *)
{
    writeSettings();
}

void QHexEditor::about()
{
   QMessageBox::about(this, tr("About QHexEditor"),
            tr("The QHexEditor is a fully features hex editor frontent using the QHexEdit control.\r\n\r\nCopyright (c) 2019 by David Xanatos."));
}

void QHexEditor::dataChanged()
{
    setWindowModified(hexEdit->isModified());
}

void QHexEditor::empty()
{
	hexEdit->setData(QByteArray());
	file.setFileName("");
}

bool QHexEditor::open()
{
    QString fileName = QFileDialog::getOpenFileName(this);
    if (!fileName.isEmpty()) {
		if (loadFile(fileName))
			return true;
    }
	return false;
}

void QHexEditor::optionsAccepted()
{
    writeSettings();
    readSettings();
}

void QHexEditor::findNext()
{
    searchDialog->findNext();
}

bool QHexEditor::save()
{
    if (file.fileName().isEmpty()) {
        return saveAs();
    } else {
        return saveFile(file.fileName());
    }
	return false;
}

bool QHexEditor::saveAs()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"), file.fileName());

    if (fileName.isEmpty())
        return false;

    return saveFile(fileName);
}

void QHexEditor::saveSelectionToReadableFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save To Readable File"));
    if (!fileName.isEmpty())
    {
        QFile file(fileName);
        if (!file.open(QFile::WriteOnly | QFile::Text)) {
            QMessageBox::warning(this, tr("QHexEditor"),
                                 tr("Cannot write file %1:\n%2.")
                                 .arg(fileName)
                                 .arg(file.errorString()));
            return;
        }

        QApplication::setOverrideCursor(Qt::WaitCursor);
        file.write(hexEdit->selectionToReadableString().toLatin1());
        QApplication::restoreOverrideCursor();

        statusBar()->showMessage(tr("File saved"), 2000);
    }
}

void QHexEditor::saveToReadableFile()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save To Readable File"));
    if (!fileName.isEmpty())
    {
        QFile file(fileName);
        if (!file.open(QFile::WriteOnly | QFile::Text)) {
            QMessageBox::warning(this, tr("QHexEditor"),
                                 tr("Cannot write file %1:\n%2.")
                                 .arg(fileName)
                                 .arg(file.errorString()));
            return;
        }

        QApplication::setOverrideCursor(Qt::WaitCursor);
        file.write(hexEdit->toReadableString().toLatin1());
        QApplication::restoreOverrideCursor();

        statusBar()->showMessage(tr("File saved"), 2000);
    }
}

void QHexEditor::setAddress(qint64 address)
{
    lbAddress->setText(QString("%1").arg(address, 1, 16));
}

void QHexEditor::setOverwriteMode(bool mode)
{
	lbOverwriteMode->setEnabled(!hexEdit->disableInsert());
    if (mode)
        lbOverwriteMode->setText(tr("Overwrite"));
    else
        lbOverwriteMode->setText(tr("Insert"));
}

void QHexEditor::setSize(qint64 size)
{
    lbSize->setText(QString("%1").arg(size));
}

void QHexEditor::showOptionsDialog()
{
    optionsDialog->show();
}

void QHexEditor::showSearchDialog()
{
    searchDialog->show();
}

void QHexEditor::sellect(qint64 address, qint64 length)
{
	hexEdit->sellect(address, length);
}

void QHexEditor::init()
{
    setAttribute(Qt::WA_DeleteOnClose);

    hexEdit = new QHexEdit;
    setCentralWidget(hexEdit);
    connect(hexEdit, SIGNAL(overwriteModeChanged(bool)), this, SLOT(setOverwriteMode(bool)));
    connect(hexEdit, SIGNAL(dataChanged()), this, SLOT(dataChanged()));
	hexEdit->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(hexEdit, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

	readSettings();

    searchDialog = new QHexEditorSearch(hexEdit, this);
	optionsDialog = new QHexEditorOptions(hexEdit, this);
    connect(optionsDialog, SIGNAL(accepted()), this, SLOT(optionsAccepted()));

    setUnifiedTitleAndToolBarOnMac(true);
}

void QHexEditor::createActions()
{
	// File Menu
	emptyAct = new QAction(QIcon(":/images/new.png"), tr("&New"), this);
    emptyAct->setShortcuts(QKeySequence::Open);
    emptyAct->setStatusTip(tr("Open an existing file"));
    connect(emptyAct, SIGNAL(triggered()), this, SLOT(empty()));

    openAct = new QAction(QIcon(":/images/open.png"), tr("&Open..."), this);
    openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Open an existing file"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()));

    saveAct = new QAction(QIcon(":/images/save.png"), tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save the document to disk"));
    connect(saveAct, SIGNAL(triggered()), this, SLOT(save()));

    saveAsAct = new QAction(QIcon(":/images/save_as.png"), tr("Save &As..."), this);
    saveAsAct->setShortcuts(QKeySequence::SaveAs);
    saveAsAct->setStatusTip(tr("Save the document under a new name"));
    connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAs()));

    saveReadable = new QAction(tr("Save &Readable..."), this);
    saveReadable->setStatusTip(tr("Save document in readable form"));
    connect(saveReadable, SIGNAL(triggered()), this, SLOT(saveToReadableFile()));

    exitAct = new QAction(QIcon(":/images/exit.png"), tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), qApp, SLOT(closeAllWindows()));

	// Edit Menu
	copyAct = new QAction(QIcon(":/images/copy.png"), tr("&Copy"), this);
    copyAct->setShortcuts(QKeySequence::Undo);
    connect(copyAct, SIGNAL(triggered()), hexEdit, SLOT(copy()));

	cutAct = new QAction(QIcon(":/images/cut.png"), tr("Cu&t"), this);
    cutAct->setShortcuts(QKeySequence::Undo);
    connect(cutAct, SIGNAL(triggered()), hexEdit, SLOT(cut()));

	pasteAct = new QAction(QIcon(":/images/paste.png"), tr("&Paste"), this);
    pasteAct->setShortcuts(QKeySequence::Undo);
    connect(pasteAct, SIGNAL(triggered()), hexEdit, SLOT(paste()));

    undoAct = new QAction(QIcon(":/images/undo.png"), tr("&Undo"), this);
    undoAct->setShortcuts(QKeySequence::Undo);
    connect(undoAct, SIGNAL(triggered()), hexEdit, SLOT(undo()));

    redoAct = new QAction(QIcon(":/images/redo.png"), tr("&Redo"), this);
    redoAct->setShortcuts(QKeySequence::Redo);
    connect(redoAct, SIGNAL(triggered()), hexEdit, SLOT(redo()));

    saveSelectionReadable = new QAction(tr("&Save Selection Readable..."), this);
    saveSelectionReadable->setStatusTip(tr("Save selection in readable form"));
    connect(saveSelectionReadable, SIGNAL(triggered()), this, SLOT(saveSelectionToReadableFile()));

	// About
    aboutAct = new QAction(tr("&About"), this);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));

    aboutQtAct = new QAction(tr("About &Qt"), this);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
    connect(aboutQtAct, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

	// Search
    findAct = new QAction(QIcon(":/images/find.png"), tr("&Find/Replace"), this);
    findAct->setShortcuts(QKeySequence::Find);
    findAct->setStatusTip(tr("Show the Dialog for finding and replacing"));
    connect(findAct, SIGNAL(triggered()), this, SLOT(showSearchDialog()));

    findNextAct = new QAction(tr("Find &next"), this);
    findNextAct->setShortcuts(QKeySequence::FindNext);
    findNextAct->setStatusTip(tr("Find next occurrence of the searched pattern"));
    connect(findNextAct, SIGNAL(triggered()), this, SLOT(findNext()));

	// Options
    optionsAct = new QAction(tr("&Options"), this);
    optionsAct->setStatusTip(tr("Show the Dialog to select applications options"));
    connect(optionsAct, SIGNAL(triggered()), this, SLOT(showOptionsDialog()));
}

void QHexEditor::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
	fileMenu->addAction(emptyAct);
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addAction(saveAsAct);
    fileMenu->addAction(saveReadable);
    fileMenu->addSeparator();
    fileMenu->addAction(optionsAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    editMenu = menuBar()->addMenu(tr("&Edit"));
	editMenu->addAction(copyAct);
	editMenu->addAction(cutAct);
	editMenu->addAction(pasteAct);
	editMenu->addSeparator();
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addAction(saveSelectionReadable);
    editMenu->addSeparator();
    editMenu->addAction(findAct);
    editMenu->addAction(findNextAct);

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);
    helpMenu->addAction(aboutQtAct);
}

void QHexEditor::createStatusBar()
{
    // Address Label
    lbAddressName = new QLabel();
    lbAddressName->setText(tr("Address:"));
    statusBar()->addPermanentWidget(lbAddressName);
    lbAddress = new QLabel();
    lbAddress->setFrameShape(QFrame::Panel);
    lbAddress->setFrameShadow(QFrame::Sunken);
    lbAddress->setMinimumWidth(70);
    statusBar()->addPermanentWidget(lbAddress);
    connect(hexEdit, SIGNAL(currentAddressChanged(qint64)), this, SLOT(setAddress(qint64)));

    // Size Label
    lbSizeName = new QLabel();
    lbSizeName->setText(tr("Size:"));
    statusBar()->addPermanentWidget(lbSizeName);
    lbSize = new QLabel();
    lbSize->setFrameShape(QFrame::Panel);
    lbSize->setFrameShadow(QFrame::Sunken);
    lbSize->setMinimumWidth(70);
    statusBar()->addPermanentWidget(lbSize);
    connect(hexEdit, SIGNAL(currentSizeChanged(qint64)), this, SLOT(setSize(qint64)));

    // Overwrite Mode Label
    lbOverwriteModeName = new QLabel();
    lbOverwriteModeName->setText(tr("Mode:"));
    statusBar()->addPermanentWidget(lbOverwriteModeName);
    lbOverwriteMode = new QLabel();
    lbOverwriteMode->setFrameShape(QFrame::Panel);
    lbOverwriteMode->setFrameShadow(QFrame::Sunken);
    lbOverwriteMode->setMinimumWidth(70);
    statusBar()->addPermanentWidget(lbOverwriteMode);
    setOverwriteMode(hexEdit->overwriteMode());

    statusBar()->showMessage(tr("Ready"), 2000);
}

void QHexEditor::createToolBars()
{
    fileToolBar = addToolBar(tr("File"));
	fileToolBar->addAction(emptyAct);
    fileToolBar->addAction(openAct);
    fileToolBar->addAction(saveAct);
	fileToolBar->addAction(saveAsAct);

    editToolBar = addToolBar(tr("Edit"));
	editToolBar->addAction(copyAct);
	editToolBar->addAction(cutAct);
	editToolBar->addAction(pasteAct);
	editToolBar->addSeparator();
    editToolBar->addAction(undoAct);
    editToolBar->addAction(redoAct);
    editToolBar->addAction(findAct);
}

bool QHexEditor::loadFile(const QString &fileName)
{
    file.setFileName(fileName);
    if (!hexEdit->setData(&file)) {
        QMessageBox::warning(this, tr("QHexEdit"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
		return false;
    }
    statusBar()->showMessage(tr("File loaded"), 2000);
	return true;
}

void QHexEditor::readSettings()
{
    hexEdit->setAddressArea(true);
    hexEdit->setAsciiArea(true);
    hexEdit->setHighlighting(true);
    hexEdit->setOverwriteMode(true);
    hexEdit->setReadOnly(false);

    hexEdit->setHighlightingColor(QColor(0xff, 0xff, 0x99, 0xff));
    hexEdit->setAddressAreaColor(this->palette().alternateBase().color());
    hexEdit->setSelectionColor(this->palette().highlight().color());
#ifdef Q_OS_WIN32
	hexEdit->setFont(QFont("Courier", 10));
#else
    hexEdit->setFont(QFont("Monospace", 10));
#endif

    hexEdit->setAddressWidth(4);
    hexEdit->setBytesPerLine(16);
}

bool QHexEditor::saveFile(const QString &fileName)
{
    QString tmpFileName = fileName + ".~tmp";

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QFile file(tmpFileName);
    bool ok = hexEdit->write(file);
    if (QFile::exists(fileName))
        ok = QFile::remove(fileName);
    if (ok)
    {
        file.setFileName(tmpFileName);
        ok = file.copy(fileName);
        if (ok)
            ok = QFile::remove(tmpFileName);
    }
    QApplication::restoreOverrideCursor();

    if (!ok) {
        QMessageBox::warning(this, tr("QHexEdit"),
                             tr("Cannot write file %1.")
                             .arg(fileName));
        return false;
    }

    statusBar()->showMessage(tr("File saved"), 2000);
    return true;
}

void QHexEditor::writeSettings()
{
}

void QHexEditor::OnMenu(const QPoint &)
{
	editMenu->popup(QCursor::pos());
}