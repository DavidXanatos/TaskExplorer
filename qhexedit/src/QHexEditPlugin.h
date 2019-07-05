#ifndef QHEXEDITPLUGIN_H
#define QHEXEDITPLUGIN_H

#include <QObject>

#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
#include <QtDesigner/QDesignerCustomWidgetInterface>
#else
#include <QDesignerCustomWidgetInterface>
#endif

class QHexEditPlugin : public QObject, public QDesignerCustomWidgetInterface
{
    Q_OBJECT
    Q_INTERFACES(QDesignerCustomWidgetInterface)
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    Q_PLUGIN_METADATA(IID "com.qt-project.Qt.QHexEditPlugin")
#endif

public:
    QHexEditPlugin(QObject * parent = 0);

    bool isContainer() const;
    bool isInitialized() const;
    QIcon icon() const;
    QString domXml() const;
    QString group() const;
    QString includeFile() const;
    QString name() const;
    QString toolTip() const;
    QString whatsThis() const;
    QWidget *createWidget(QWidget *parent);
    void initialize(QDesignerFormEditorInterface *core);

private:
    bool initialized;

};

#endif
