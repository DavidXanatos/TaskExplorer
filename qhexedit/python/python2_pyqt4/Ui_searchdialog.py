# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file 'searchdialog.ui'
#
# Created: Thu May 28 21:45:19 2015
#      by: PyQt4 UI code generator 4.10.4
#
# WARNING! All changes made in this file will be lost!

from PyQt4 import QtCore, QtGui

try:
    _fromUtf8 = QtCore.QString.fromUtf8
except AttributeError:
    def _fromUtf8(s):
        return s

try:
    _encoding = QtGui.QApplication.UnicodeUTF8
    def _translate(context, text, disambig):
        return QtGui.QApplication.translate(context, text, disambig, _encoding)
except AttributeError:
    def _translate(context, text, disambig):
        return QtGui.QApplication.translate(context, text, disambig)

class Ui_SearchDialog(object):
    def setupUi(self, SearchDialog):
        SearchDialog.setObjectName(_fromUtf8("SearchDialog"))
        SearchDialog.resize(436, 223)
        self.horizontalLayout_3 = QtGui.QHBoxLayout(SearchDialog)
        self.horizontalLayout_3.setObjectName(_fromUtf8("horizontalLayout_3"))
        self.verticalLayout_2 = QtGui.QVBoxLayout()
        self.verticalLayout_2.setObjectName(_fromUtf8("verticalLayout_2"))
        self.gbFind = QtGui.QGroupBox(SearchDialog)
        self.gbFind.setObjectName(_fromUtf8("gbFind"))
        self.horizontalLayout = QtGui.QHBoxLayout(self.gbFind)
        self.horizontalLayout.setObjectName(_fromUtf8("horizontalLayout"))
        self.cbFindFormat = QtGui.QComboBox(self.gbFind)
        self.cbFindFormat.setObjectName(_fromUtf8("cbFindFormat"))
        self.cbFindFormat.addItem(_fromUtf8(""))
        self.cbFindFormat.addItem(_fromUtf8(""))
        self.horizontalLayout.addWidget(self.cbFindFormat)
        self.cbFind = QtGui.QComboBox(self.gbFind)
        sizePolicy = QtGui.QSizePolicy(QtGui.QSizePolicy.Expanding, QtGui.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.cbFind.sizePolicy().hasHeightForWidth())
        self.cbFind.setSizePolicy(sizePolicy)
        self.cbFind.setEditable(True)
        self.cbFind.setObjectName(_fromUtf8("cbFind"))
        self.horizontalLayout.addWidget(self.cbFind)
        self.verticalLayout_2.addWidget(self.gbFind)
        self.gbReplace = QtGui.QGroupBox(SearchDialog)
        self.gbReplace.setObjectName(_fromUtf8("gbReplace"))
        self.horizontalLayout_2 = QtGui.QHBoxLayout(self.gbReplace)
        self.horizontalLayout_2.setObjectName(_fromUtf8("horizontalLayout_2"))
        self.cbReplaceFormat = QtGui.QComboBox(self.gbReplace)
        self.cbReplaceFormat.setObjectName(_fromUtf8("cbReplaceFormat"))
        self.cbReplaceFormat.addItem(_fromUtf8(""))
        self.cbReplaceFormat.addItem(_fromUtf8(""))
        self.horizontalLayout_2.addWidget(self.cbReplaceFormat)
        self.cbReplace = QtGui.QComboBox(self.gbReplace)
        sizePolicy = QtGui.QSizePolicy(QtGui.QSizePolicy.Expanding, QtGui.QSizePolicy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.cbReplace.sizePolicy().hasHeightForWidth())
        self.cbReplace.setSizePolicy(sizePolicy)
        self.cbReplace.setEditable(True)
        self.cbReplace.setObjectName(_fromUtf8("cbReplace"))
        self.horizontalLayout_2.addWidget(self.cbReplace)
        self.verticalLayout_2.addWidget(self.gbReplace)
        self.gbOptions = QtGui.QGroupBox(SearchDialog)
        self.gbOptions.setObjectName(_fromUtf8("gbOptions"))
        self.verticalLayout_3 = QtGui.QVBoxLayout(self.gbOptions)
        self.verticalLayout_3.setObjectName(_fromUtf8("verticalLayout_3"))
        self.cbBackwards = QtGui.QCheckBox(self.gbOptions)
        self.cbBackwards.setObjectName(_fromUtf8("cbBackwards"))
        self.verticalLayout_3.addWidget(self.cbBackwards)
        self.cbPrompt = QtGui.QCheckBox(self.gbOptions)
        self.cbPrompt.setObjectName(_fromUtf8("cbPrompt"))
        self.verticalLayout_3.addWidget(self.cbPrompt)
        self.verticalLayout_2.addWidget(self.gbOptions)
        self.horizontalLayout_3.addLayout(self.verticalLayout_2)
        self.verticalLayout = QtGui.QVBoxLayout()
        self.verticalLayout.setObjectName(_fromUtf8("verticalLayout"))
        self.pbFind = QtGui.QPushButton(SearchDialog)
        self.pbFind.setDefault(True)
        self.pbFind.setObjectName(_fromUtf8("pbFind"))
        self.verticalLayout.addWidget(self.pbFind)
        self.pbReplace = QtGui.QPushButton(SearchDialog)
        self.pbReplace.setObjectName(_fromUtf8("pbReplace"))
        self.verticalLayout.addWidget(self.pbReplace)
        self.pbReplaceAll = QtGui.QPushButton(SearchDialog)
        self.pbReplaceAll.setObjectName(_fromUtf8("pbReplaceAll"))
        self.verticalLayout.addWidget(self.pbReplaceAll)
        self.pbCancel = QtGui.QPushButton(SearchDialog)
        self.pbCancel.setObjectName(_fromUtf8("pbCancel"))
        self.verticalLayout.addWidget(self.pbCancel)
        spacerItem = QtGui.QSpacerItem(20, 40, QtGui.QSizePolicy.Minimum, QtGui.QSizePolicy.Expanding)
        self.verticalLayout.addItem(spacerItem)
        self.horizontalLayout_3.addLayout(self.verticalLayout)

        self.retranslateUi(SearchDialog)
        QtCore.QObject.connect(self.pbCancel, QtCore.SIGNAL(_fromUtf8("clicked()")), SearchDialog.hide)
        QtCore.QMetaObject.connectSlotsByName(SearchDialog)
        SearchDialog.setTabOrder(self.cbFind, self.cbReplace)
        SearchDialog.setTabOrder(self.cbReplace, self.cbFindFormat)
        SearchDialog.setTabOrder(self.cbFindFormat, self.cbReplaceFormat)
        SearchDialog.setTabOrder(self.cbReplaceFormat, self.cbBackwards)
        SearchDialog.setTabOrder(self.cbBackwards, self.cbPrompt)
        SearchDialog.setTabOrder(self.cbPrompt, self.pbFind)
        SearchDialog.setTabOrder(self.pbFind, self.pbReplace)
        SearchDialog.setTabOrder(self.pbReplace, self.pbReplaceAll)
        SearchDialog.setTabOrder(self.pbReplaceAll, self.pbCancel)

    def retranslateUi(self, SearchDialog):
        SearchDialog.setWindowTitle(_translate("SearchDialog", "QHexEdit - Find/Replace", None))
        self.gbFind.setTitle(_translate("SearchDialog", "Find", None))
        self.cbFindFormat.setItemText(0, _translate("SearchDialog", "Hex", None))
        self.cbFindFormat.setItemText(1, _translate("SearchDialog", "UTF-8", None))
        self.gbReplace.setTitle(_translate("SearchDialog", "Replace", None))
        self.cbReplaceFormat.setItemText(0, _translate("SearchDialog", "Hex", None))
        self.cbReplaceFormat.setItemText(1, _translate("SearchDialog", "UTF-8", None))
        self.gbOptions.setTitle(_translate("SearchDialog", "Options", None))
        self.cbBackwards.setText(_translate("SearchDialog", "&Backwards", None))
        self.cbPrompt.setText(_translate("SearchDialog", "&Prompt on replace", None))
        self.pbFind.setText(_translate("SearchDialog", "&Find", None))
        self.pbFind.setShortcut(_translate("SearchDialog", "F3", None))
        self.pbReplace.setText(_translate("SearchDialog", "&Replace", None))
        self.pbReplaceAll.setText(_translate("SearchDialog", "Replace &All", None))
        self.pbCancel.setText(_translate("SearchDialog", "&Close", None))

