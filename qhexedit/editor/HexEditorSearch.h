#pragma once

#include <QDialog>
#include <QtCore>
#include "../../qhexedit/src/qhexedit.h"

namespace Ui {
    class HexEditorSearch;
}

class QHexEdit;

class QHEXEDIT_API QHexEditorSearch : public QDialog
{
    Q_OBJECT
public:
    explicit QHexEditorSearch(QHexEdit *hexEdit, QWidget *parent = 0);
    virtual ~QHexEditorSearch();
    virtual qint64 findNext();
    Ui::HexEditorSearch *ui;

protected slots:
    virtual void on_pbFind_clicked();
    virtual void on_pbReplace_clicked();
    virtual void on_pbReplaceAll_clicked();

protected:
    QByteArray getContent(int comboIndex, const QString &input);
    qint64 replaceOccurrence(qint64 idx, const QByteArray &replaceBa);

    QHexEdit *_hexEdit;
    QByteArray _findBa;
};
