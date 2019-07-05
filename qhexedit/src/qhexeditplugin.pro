#! [0] #! [1]
greaterThan(QT_MAJOR_VERSION, 4) {
    QT      += widgets uiplugin
}

lessThan(QT_MAJOR_VERSION, 5) {
    CONFIG  += designer
}

CONFIG      += plugin
#! [0]
TARGET      = $$qtLibraryTarget($$TARGET)
#! [2]
TEMPLATE    = lib
#! [1] #! [2]
QTDIR_build:DESTDIR     = $$QT_BUILD_TREE/plugins/designer

#! [3]
HEADERS = \
    qhexedit.h \
    chunks.h \
    commands.h \
	QHexEditPlugin.h


SOURCES = \
    qhexedit.cpp \
    chunks.cpp \
    commands.cpp \
	QHexEditPlugin.cpp
	
#! [3]

# install
target.path = $$[QT_INSTALL_PLUGINS]/designer
sources.files = $$SOURCES $$HEADERS *.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/designer/QHexEditPlugin
INSTALLS += target 

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
