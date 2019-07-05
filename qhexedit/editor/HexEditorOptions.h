#pragma once

#include <QtCore>
#include <QDialog>
#include "../../qhexedit/src/qhexedit.h"

namespace Ui {
    class HexEditorOptions;
}

class QHexEdit;

class QHEXEDIT_API QHexEditorOptions : public QDialog
{
    Q_OBJECT

public:
    explicit QHexEditorOptions(QHexEdit *hexEdit, QWidget *parent = 0);
    virtual ~QHexEditorOptions();
    Ui::HexEditorOptions *ui;
    virtual void show();

public slots:
    virtual void accept();

protected slots:
    virtual void on_pbHighlightingColor_clicked();
    virtual void on_pbAddressAreaColor_clicked();
    virtual void on_pbSelectionColor_clicked();
    virtual void on_pbWidgetFont_clicked();

protected:
    virtual void readSettings();
    virtual void writeSettings();
    virtual void setColor(QWidget *widget, QColor color);

	QHexEdit *_hexEdit;
};

