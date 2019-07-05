#!/usr/bin/env python

import sys
from PyQt4 import QtCore, QtGui

from Ui_optionsdialog import Ui_OptionsDialog


class OptionsDialog(QtGui.QDialog):
    accepted = QtCore.pyqtSignal()

    def __init__(self, fileName=None):
        super(OptionsDialog, self).__init__()
        self.ui = Ui_OptionsDialog()
        self.ui.setupUi(self)
        
        self.readSettings()
        self.writeSettings()

    def show(self):
        self.readSettings()
        super(OptionsDialog, self).show()
        
    def accept(self):
        self.writeSettings()
        self.accepted.emit()
        super(OptionsDialog, self).hide()
        
    def readSettings(self):
        settings = QtCore.QSettings()
        
        self.setColor(self.ui.lbHighlightingColor, QtGui.QColor(settings.value("HighlightingColor", QtGui.QColor(0xff, 0xff, 0x99, 0xff))))
        self.setColor(self.ui.lbAddressAreaColor, QtGui.QColor(settings.value("AddressAreaColor", QtGui.QColor(0xd4, 0xd4, 0xd4, 0xff))))
        self.setColor(self.ui.lbSelectionColor, QtGui.QColor(settings.value("SelectionColor", QtGui.QColor(0x6d, 0x9e, 0xff, 0xff))))
        self.ui.leWidgetFont.setFont(QtGui.QFont(settings.value("WidgetFont", QtGui.QFont(QtGui.QFont("Courier New", 10)))))

        if sys.version_info >= (3, 0):
            self.ui.sbAddressAreaWidth.setValue(int(settings.value("AddressAreaWidth", 4)))
            self.ui.cbAddressArea.setChecked(settings.value("AddressArea", 'true')=='true')
            self.ui.cbAsciiArea.setChecked(settings.value("AsciiArea", 'true')=='true')
            self.ui.cbHighlighting.setChecked(settings.value("Highlighting", 'true')=='true')
            self.ui.cbOverwriteMode.setChecked(settings.value("OverwriteMode", 'true')=='true')
            self.ui.cbReadOnly.setChecked(settings.value("ReadOnly", 'false')=='true')

        else:
            self.ui.sbAddressAreaWidth.setValue(settings.value("AddressAreaWidth", 4).toInt()[0])
            self.ui.cbAddressArea.setChecked(settings.value("AddressArea", True).toBool())
            self.ui.cbAsciiArea.setChecked(settings.value("AsciiArea", True).toBool())
            self.ui.cbHighlighting.setChecked(settings.value("Highlighting", True).toBool())
            self.ui.cbOverwriteMode.setChecked(settings.value("OverwriteMode", True).toBool())
            self.ui.cbReadOnly.setChecked(settings.value("ReadOnly", False).toBool())


    def writeSettings(self):
        settings = QtCore.QSettings()
        if sys.version_info >= (3, 0):
            def b(b):
                if b: return 'true'
                else: return 'false'
            settings.setValue("AddressArea", b(self.ui.cbAddressArea.isChecked()))
            settings.setValue("AsciiArea", b(self.ui.cbAsciiArea.isChecked()))
            settings.setValue("Highlighting", b(self.ui.cbHighlighting.isChecked()))
            settings.setValue("OverwriteMode", b(self.ui.cbOverwriteMode.isChecked()))
            settings.setValue("ReadOnly", b(self.ui.cbReadOnly.isChecked()))
        else:
            settings.setValue("AddressArea", self.ui.cbAddressArea.isChecked())
            settings.setValue("AsciiArea", self.ui.cbAsciiArea.isChecked())
            settings.setValue("Highlighting", self.ui.cbHighlighting.isChecked())
            settings.setValue("OverwriteMode", self.ui.cbOverwriteMode.isChecked())
            settings.setValue("ReadOnly", self.ui.cbReadOnly.isChecked())
        
        settings.setValue("HighlightingColor", self.ui.lbHighlightingColor.palette().color(QtGui.QPalette.Background))
        settings.setValue("AddressAreaColor", self.ui.lbAddressAreaColor.palette().color(QtGui.QPalette.Background))
        settings.setValue("SelectionColor", self.ui.lbSelectionColor.palette().color(QtGui.QPalette.Background))
        settings.setValue("WidgetFont", self.ui.leWidgetFont.font())
        
        settings.setValue("AddressAreaWidth", self.ui.sbAddressAreaWidth.value())
        
    def reject(self):
        super(OptionsDialog, self).hide()
        
    def setColor(self, label, color):
        palette = label.palette()
        palette.setColor(QtGui.QPalette.Background, color)
        label.setPalette(palette)
        label.setAutoFillBackground(True)

    def on_pbHighlightingColor_pressed(self):
        print("hier")
        color = QtGui.QColorDialog.getColor(self.ui.lbHighlightingColor.palette().color(QtGui.QPalette.Background), self)
        if color.isValid():
            self.setColor(self.ui.lbHighlightingColor, color)
        
    def on_pbAddressAreaColor_pressed(self):
        color = QtGui.QColorDialog.getColor(self.ui.lbAddressAreaColor.palette().color(QtGui.QPalette.Background), self)
        if color.isValid():
            self.setColor(self.ui.lbAddressAreaColor, color)
        
    def on_pbSelectionColor_pressed(self):
        color = QtGui.QColorDialog.getColor(self.ui.lbSelectionColor.palette().color(QtGui.QPalette.Background), self)
        if color.isValid():
            self.setColor(self.ui.lbSelectioncColor, color)
        
    def on_pbWidgetFont_pressed(self):
        font, ok = QtGui.QFontDialog().getFont(self.ui.leWidgetFont.font(), self)
        if ok:
            self.ui.leWidgetFont.setFont(font)
