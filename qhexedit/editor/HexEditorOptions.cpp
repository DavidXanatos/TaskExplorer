#include <QColorDialog>
#include <QFontDialog>

#include "../src/qhexedit.h"
#include "HexEditorOptions.h"
#include "ui_HexEditorOptions.h"


QHexEditorOptions::QHexEditorOptions(QHexEdit *hexEdit, QWidget *parent) :
    QDialog(parent),
	_hexEdit(hexEdit),
    ui(new Ui::HexEditorOptions)
{
    ui->setupUi(this);
    readSettings();
    writeSettings();
}

QHexEditorOptions::~QHexEditorOptions()
{
    delete ui;
}

void QHexEditorOptions::show()
{
    readSettings();
    QWidget::show();
}

void QHexEditorOptions::accept()
{
    writeSettings();
    emit accepted();
    QDialog::hide();
}

void QHexEditorOptions::readSettings()
{
    ui->cbAddressArea->setChecked(_hexEdit->addressArea());
    ui->cbAsciiArea->setChecked(_hexEdit->asciiArea());
    ui->cbHighlighting->setChecked(_hexEdit->highlighting());
	ui->cbOverwriteMode->setEnabled(!_hexEdit->disableInsert());
    ui->cbOverwriteMode->setChecked(_hexEdit->overwriteMode());
    ui->cbReadOnly->setChecked(_hexEdit->isReadOnly());

    setColor(ui->lbHighlightingColor, _hexEdit->highlightingColor());
    setColor(ui->lbAddressAreaColor, _hexEdit->addressAreaColor());
    setColor(ui->lbSelectionColor, _hexEdit->selectionColor());
	ui->leWidgetFont->setFont(_hexEdit->font());

    ui->sbAddressAreaWidth->setValue(_hexEdit->addressWidth());
    ui->sbBytesPerLine->setValue(_hexEdit->bytesPerLine());
}

void QHexEditorOptions::writeSettings()
{
    _hexEdit->setAddressArea(ui->cbAddressArea->isChecked());
    _hexEdit->setAsciiArea(ui->cbAsciiArea->isChecked());
    _hexEdit->setHighlighting(ui->cbHighlighting->isChecked());
    _hexEdit->setOverwriteMode(ui->cbOverwriteMode->isChecked());
    _hexEdit->setReadOnly(ui->cbReadOnly->isChecked());

    _hexEdit->setHighlightingColor(ui->lbHighlightingColor->palette().color(QPalette::Background));
    _hexEdit->setAddressAreaColor(ui->lbAddressAreaColor->palette().color(QPalette::Background));
    _hexEdit->setSelectionColor(ui->lbSelectionColor->palette().color(QPalette::Background));
    _hexEdit->setFont(ui->leWidgetFont->font());

    _hexEdit->setAddressWidth(ui->sbAddressAreaWidth->value());
    _hexEdit->setBytesPerLine(ui->sbBytesPerLine->value());
}

void QHexEditorOptions::setColor(QWidget *widget, QColor color)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Background, color);
    widget->setPalette(palette);
    widget->setAutoFillBackground(true);
}

void QHexEditorOptions::on_pbHighlightingColor_clicked()
{
    QColor color = QColorDialog::getColor(ui->lbHighlightingColor->palette().color(QPalette::Background), this);
    if (color.isValid())
        setColor(ui->lbHighlightingColor, color);
}

void QHexEditorOptions::on_pbAddressAreaColor_clicked()
{
    QColor color = QColorDialog::getColor(ui->lbAddressAreaColor->palette().color(QPalette::Background), this);
    if (color.isValid())
        setColor(ui->lbAddressAreaColor, color);
}

void QHexEditorOptions::on_pbSelectionColor_clicked()
{
    QColor color = QColorDialog::getColor(ui->lbSelectionColor->palette().color(QPalette::Background), this);
    if (color.isValid())
        setColor(ui->lbSelectionColor, color);
}

void QHexEditorOptions::on_pbWidgetFont_clicked()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, ui->leWidgetFont->font(), this);
    if (ok)
        ui->leWidgetFont->setFont(font);
}
