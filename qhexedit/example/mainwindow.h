#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "../editor/HexEditor.h"


class MainWindow : public QHexEditor
{
    Q_OBJECT

public:
    MainWindow();

protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

private slots:
    void about();
    bool open();
    bool save();
    bool saveAs();
	void setCurrentFile(const QString &fileName);

private:
    QString curFile;
    bool isUntitled;
};

#endif
