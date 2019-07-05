%Module(name=qhexedit, version=1)

%Import QtCore/QtCoremod.sip
%Import QtGui/QtGuimod.sip

%If (Qt_5_0_0 -)
%Import QtWidgets/QtWidgetsmod.sip
%End

class QHexEdit : QAbstractScrollArea
{
%TypeHeaderCode
#include "../src/qhexedit.h"
%End

public:
    explicit QHexEdit(QWidget *parent /TransferThis/ = 0);
    virtual ~QHexEdit();

    bool setData(QIODevice &);
    QByteArray dataAt(qint64, qint64=-1);
    bool write(QIODevice &iODevice, qint64=0, qint64=-1);
    
    void insert(qint64, char);
    void remove(qint64, qint64);
    void replace(qint64, char);

    void insert(qint64, QByteArray &);
    void replace(qint64, qint64, QByteArray &);

    bool addressArea();
    bool addressWidth();
    bool asciiArea();
    qint64 cursorPosition(QPoint);
    void ensureVisible();
    qint64 indexOf(QByteArray &, qint64);
    bool isModified();
    bool highlighting();
    qint64 lastIndexOf(QByteArray &, qint64);
    QString selectionToReadableString();
    QString selectedData();
    void setFont(const QFont &);
    QString toReadableString();

    QColor addressAreaColor();
    void setAddressAreaColor(const QColor &);

    qint64 addressOffset();
    void setAddressOffset(qint64);

    qint64 cursorPosition();
    void setCursorPosition(qint64);

    QByteArray data();
    void setData(const QByteArray &);

    QColor highlightingColor();
    void setHighlightingColor(const QColor &);

    bool overwriteMode();
    void setOverwriteMode(bool);

    bool isReadOnly();
    void setReadOnly(bool);

    QColor selectionColor();
    void setSelectionColor(const QColor &);

public slots:
    void redo();
    void setAddressArea(bool);
    void setAddressWidth(int);
    void setAsciiArea(bool);
    void setHighlighting(bool);
    void undo();

signals:
    void currentAddressChanged(qint64);
    void currentSizeChanged(qint64);
    void dataChanged();
    void overwriteModeChanged(bool);
};
