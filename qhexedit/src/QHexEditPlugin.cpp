#include "QHexEditPlugin.h"
#include "qhexedit.h"

#include <QtPlugin>

QHexEditPlugin::QHexEditPlugin(QObject * parent) : QObject(parent)
{
    initialized = false;
}


bool QHexEditPlugin::isContainer() const
{
    return false;
}

bool QHexEditPlugin::isInitialized() const
{
    return initialized;
}

QIcon QHexEditPlugin::icon() const
{
    return QIcon();
}

QString QHexEditPlugin::domXml() const
{
    return "<ui language=\"c++\">\n"
        " <widget class=\"QHexEdit\" name=\"hexEdit\">\n"
        "  <property name=\"geometry\">\n"
        "   <rect>\n"
        "    <x>0</x>\n"
        "    <y>0</y>\n"
        "    <width>100</width>\n"
        "    <height>100</height>\n"
        "   </rect>\n"
        "  </property>\n"
        "  <property name=\"toolTip\" >\n"
        "   <string>QHexEditWidget</string>\n"
        "  </property>\n"
        "  <property name=\"whatsThis\" >\n"
        "   <string>QHexEdit widget allow to edit the data in hex view.</string>\n"
        "  </property>\n"
        " </widget>\n"
        "</ui>\n";
}

QString QHexEditPlugin::group() const
{
    return "Input Widgets";
}

QString QHexEditPlugin::includeFile() const
{
    return "qhexedit.h";
}

QString QHexEditPlugin::name() const
{
    return "QHexEdit";
}

QString QHexEditPlugin::toolTip() const
{
    return "";
}

QString QHexEditPlugin::whatsThis() const
{
    return "";
}

QWidget * QHexEditPlugin::createWidget(QWidget *parent) 
{
    return new QHexEdit(parent);
}

void QHexEditPlugin::initialize(QDesignerFormEditorInterface * /*core*/)
{
    if (initialized)
        return;

    initialized = true;
}

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
Q_EXPORT_PLUGIN2(QHexEditPlugin, QHexEditPlugin)
#endif
