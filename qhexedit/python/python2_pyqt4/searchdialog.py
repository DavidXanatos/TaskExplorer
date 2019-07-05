#!/usr/bin/env python

from PyQt4 import QtCore, QtGui

from Ui_searchdialog import Ui_SearchDialog


class SearchDialog(QtGui.QDialog):
    def __init__(self, parent, hexEdit):
        super(SearchDialog, self).__init__()
        self.ui = Ui_SearchDialog()
        self.ui.setupUi(self)
        self._hexEdit = hexEdit
        
    def findNext(self):
        startIdx = self._hexEdit.cursorPosition()
        findBa = self.getContent(self.ui.cbFindFormat.currentIndex(), self.ui.cbFind.currentText())
        idx = -1
        
        if findBa.length() > 0:
            if self.ui.cbBackwards.isChecked():
                idx = self._hexEdit.lastIndexOf(findBa, startIdx)
            else:
                idx = self._hexEdit.indexOf(findBa, startIdx)
        
        return idx
        
    @QtCore.pyqtSlot()
    def on_pbFind_clicked(self):
        self.findNext()
        
    @QtCore.pyqtSlot()
    def on_pbReplace_clicked(self):
        idx = self.findNext()
        if idx >= 0:
            replaceBa = self.getContent(self.ui.cbReplaceFormat.currentIndex(), self.ui.cbReplace.currentText())
            self.replaceOccurrence(idx, replaceBa)
            
    @QtCore.pyqtSlot()
    def on_pbReplaceAll_clicked(self):
        replaceCounter = 0
        idx = 0
        goOn = QtGui.QMessageBox.Yes
        
        while (idx >= 0) and (goOn == QtGui.QMessageBox.Yes):
            idx = self.findNext()
            if idx >= 0:
                replaceBa = self.getContent(self.ui.cbReplaceFormat.currentIndex(), self.ui.cbReplace.currentText())
                result = self.replaceOccurrence(idx, replaceBa)
                
                if result == QtGui.QMessageBox.Yes:
                    replaceCounter += 1
                    
                if result == QtGui.QMessageBox.Cancel:
                    goOn = QtGui.QMessageBox.Cancel
                    
        if replaceCounter > 0:
            QtGui.QMessageBox.information(self, "QHexEdit", "%s occurrences replaced" % replaceCounter)
            
    def getContent(self, comboIndex, inputStr):
        if comboIndex == 0:     # hex
            findBa = QtCore.QByteArray.fromHex(inputStr.toAscii())
        elif comboIndex == 1:   # text
            findBa = inputStr.toUtf8()
        return findBa   
    
    def replaceOccurrence(self, idx, replaceBa):
        result = QtGui.QMessageBox.Yes
        
        if replaceBa.length() >= 0:
            if self.ui.cbPrompt.isChecked():
                result = QtGui.QMessageBox.question(self, "QHexEdit", "Replace occurrence?", 
                             QtGui.QMessageBox.Yes | QtGui.QMessageBox.No | QtGui.QMessageBox.Cancel)
                
                if result == QtGui.QMessageBox.Yes:
                    self._hexEdit.replace(idx, replaceBa.length(), replaceBa)
                    self._hexEdit.update()
                    
            else:
                self._hexEdit.replace(idx, replaceBa.length(), replaceBa)
                    
        return result

        