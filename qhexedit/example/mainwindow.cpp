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

#include "mainwindow.h"

/*****************************************************************************/
/* Public methods */
/*****************************************************************************/
MainWindow::MainWindow()
{
    setAcceptDrops( true );
    init();
    setCurrentFile("");
}

/*****************************************************************************/
/* Protected methods */
/*****************************************************************************/

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->accept();
}


void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        QList<QUrl> urls = event->mimeData()->urls();
        QString filePath = urls.at(0).toLocalFile();
        loadFile(filePath);
        event->accept();
    }
}

/*****************************************************************************/
/* Private Slots */
/*****************************************************************************/
void MainWindow::about()
{
   QMessageBox::about(this, tr("About QHexEdit"),
            tr("The QHexEdit example is a short Demo of the QHexEdit Widget."));
}

bool MainWindow::open()
{
    QString fileName = QFileDialog::getOpenFileName(this);
    if (!fileName.isEmpty()) {
		if (loadFile(fileName)) {
			setCurrentFile(fileName);
			return true;
		}
    }
	return false;
}

bool MainWindow::save()
{
    if (isUntitled) {
        return saveAs();
    } else {
		if (!saveFile(curFile))
			return false;
		setCurrentFile(curFile);
		return true;
    }
}

bool MainWindow::saveAs()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                                    curFile);
    if (fileName.isEmpty())
        return false;

	if (!saveFile(fileName))
		return false;
	setCurrentFile(fileName);
	return true;
}

/*****************************************************************************/
/* Private Methods */
/*****************************************************************************/

void MainWindow::setCurrentFile(const QString &fileName)
{
    curFile = QFileInfo(fileName).canonicalFilePath();
    isUntitled = fileName.isEmpty();
    setWindowModified(false);
    if (fileName.isEmpty())
        setWindowFilePath("QHexEdit");
    else
        setWindowFilePath(curFile + " - QHexEdit");
}
