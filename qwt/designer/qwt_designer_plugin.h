/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#ifndef QWT_DESIGNER_PLUGIN_H
#define QWT_DESIGNER_PLUGIN_H

#include <QDesignerCustomWidgetInterface>
#include <QDesignerTaskMenuExtension>
#include <QExtensionFactory>

namespace QwtDesignerPlugin
{
    class CustomWidgetInterface: public QObject,
        public QDesignerCustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        CustomWidgetInterface( QObject *parent );

        virtual bool isContainer() const;
        virtual bool isInitialized() const;
        virtual QIcon icon() const;
        virtual QString codeTemplate() const;
        virtual QString domXml() const;
        virtual QString group() const;
        virtual QString includeFile() const;
        virtual QString name() const;
        virtual QString toolTip() const;
        virtual QString whatsThis() const;
        virtual void initialize( QDesignerFormEditorInterface * );

    protected:
        QString d_name;
        QString d_include;
        QString d_toolTip;
        QString d_whatsThis;
        QString d_domXml;
        QString d_codeTemplate;
        QIcon d_icon;

    private:
        bool d_isInitialized;
    };

    class CustomWidgetCollectionInterface: public QObject,
        public QDesignerCustomWidgetCollectionInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetCollectionInterface )

#if QT_VERSION >= 0x050000
        Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QDesignerCustomWidgetCollectionInterface" )
#endif


    public:
        CustomWidgetCollectionInterface( QObject *parent = NULL );

        virtual QList<QDesignerCustomWidgetInterface*> customWidgets() const;

    private:
        QList<QDesignerCustomWidgetInterface*> d_plugins;
    };

#ifndef NO_QWT_PLOT
    class PlotInterface: public CustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        PlotInterface( QObject *parent );
        virtual QWidget *createWidget( QWidget *parent );
    };

    class PlotCanvasInterface: public CustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        PlotCanvasInterface( QObject *parent );
        virtual QWidget *createWidget( QWidget *parent );
    };
#endif

#ifndef NO_QWT_WIDGETS
    class AnalogClockInterface: public CustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        AnalogClockInterface( QObject *parent );
        virtual QWidget *createWidget( QWidget *parent );
    };
#endif

#ifndef NO_QWT_WIDGETS
    class CompassInterface: public CustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        CompassInterface( QObject *parent );
        virtual QWidget *createWidget( QWidget *parent );
    };
#endif

#ifndef NO_QWT_WIDGETS
    class CounterInterface: public CustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        CounterInterface( QObject *parent );
        virtual QWidget *createWidget( QWidget *parent );
    };
#endif

#ifndef NO_QWT_WIDGETS
    class DialInterface: public CustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        DialInterface( QObject *parent );
        virtual QWidget *createWidget( QWidget *parent );
    };
#endif

#ifndef NO_QWT_WIDGETS
    class KnobInterface: public CustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        KnobInterface( QObject *parent );
        virtual QWidget *createWidget( QWidget *parent );
    };
#endif

#ifndef NO_QWT_PLOT
    class ScaleWidgetInterface: public CustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        ScaleWidgetInterface( QObject *parent );
        virtual QWidget *createWidget( QWidget *parent );
    };
#endif

#ifndef NO_QWT_WIDGETS
    class SliderInterface: public CustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        SliderInterface( QObject *parent );
        virtual QWidget *createWidget( QWidget *parent );
    };
#endif

    class TextLabelInterface: public CustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        TextLabelInterface( QObject *parent );
        virtual QWidget *createWidget( QWidget *parent );
    };

#ifndef NO_QWT_WIDGETS
    class ThermoInterface: public CustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        ThermoInterface( QObject *parent );
        virtual QWidget *createWidget( QWidget *parent );
    };
#endif

#ifndef NO_QWT_WIDGETS
    class WheelInterface: public CustomWidgetInterface
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerCustomWidgetInterface )

    public:
        WheelInterface( QObject *parent );
        virtual QWidget *createWidget( QWidget *parent );
    };
#endif

    class TaskMenuFactory: public QExtensionFactory
    {
        Q_OBJECT

    public:
        TaskMenuFactory( QExtensionManager *parent = 0 );

    protected:
        QObject *createExtension( QObject *object,
            const QString &iid, QObject *parent ) const;
    };

    class TaskMenuExtension: public QObject,
        public QDesignerTaskMenuExtension
    {
        Q_OBJECT
        Q_INTERFACES( QDesignerTaskMenuExtension )

    public:
        TaskMenuExtension( QWidget *widget, QObject *parent );

        QAction *preferredEditAction() const;
        QList<QAction *> taskActions() const;

    private Q_SLOTS:
        void editProperties();
        void applyProperties( const QString & );

    private:
        QAction *d_editAction;
        QWidget *d_widget;
    };

};

#endif
