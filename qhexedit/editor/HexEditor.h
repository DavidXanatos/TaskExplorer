#pragma once

#include <QMainWindow>

#include "../src/qhexedit_global.h"

#include "../src/qhexedit.h"
#include "HexEditorOptions.h"
#include "HexEditorSearch.h"

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QUndoStack;
class QLabel;
class QDragEnterEvent;
class QDropEvent;
QT_END_NAMESPACE

class QHEXEDIT_API QHexEditor : public QMainWindow
{
    Q_OBJECT

public:
    QHexEditor(QWidget *parent = Q_NULLPTR);
	virtual ~QHexEditor();

public slots:
	virtual void sellect(qint64 address, qint64 length);

protected:
    virtual void closeEvent(QCloseEvent *event);

protected slots:
    virtual void about();
    virtual void dataChanged();
	virtual void empty();
    virtual bool open();
    virtual void optionsAccepted();
    virtual void findNext();
    virtual bool save();
    virtual bool saveAs();
    virtual void saveSelectionToReadableFile();
    virtual void saveToReadableFile();
    virtual void setAddress(qint64 address);
    virtual void setOverwriteMode(bool mode);
    virtual void setSize(qint64 size);
    virtual void showOptionsDialog();
    virtual void showSearchDialog();

	virtual void OnMenu(const QPoint &);

protected:
    virtual void init();
    virtual void createActions();
    virtual void createMenus();
    virtual void createStatusBar();
    virtual void createToolBars();
    virtual bool loadFile(const QString &fileName);
    virtual void readSettings();
    virtual bool saveFile(const QString &fileName);
    virtual void writeSettings();

    QFile file;
    
    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *helpMenu;

    QToolBar *fileToolBar;
    QToolBar *editToolBar;

	QAction *emptyAct;
    QAction *openAct;
    QAction *saveAct;
    QAction *saveAsAct;
    QAction *saveReadable;
    QAction *closeAct;
    QAction *exitAct;

	QAction *copyAct;
	QAction *cutAct;
	QAction *pasteAct;
    QAction *undoAct;
    QAction *redoAct;
    QAction *saveSelectionReadable;

    QAction *aboutAct;
    QAction *aboutQtAct;
    QAction *optionsAct;
    QAction *findAct;
    QAction *findNextAct;

    QHexEdit *hexEdit;
    QHexEditorOptions *optionsDialog;
    QHexEditorSearch *searchDialog;
    QLabel *lbAddress, *lbAddressName;
    QLabel *lbOverwriteMode, *lbOverwriteModeName;
    QLabel *lbSize, *lbSizeName;
};
