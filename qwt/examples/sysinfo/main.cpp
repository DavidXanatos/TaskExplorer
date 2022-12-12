/*****************************************************************************
 * Qwt Examples
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include <QwtThermo>
#include <QwtColorMap>

#include <QApplication>
#include <QWidget>
#include <QFont>
#include <QLabel>
#include <QGroupBox>
#include <QLayout>

namespace
{
    class Gauge : public QwtThermo
    {
      public:
        Gauge()
        {
            setScale( 0.0, 100.0 );
            setFont( QFont( "Helvetica", 8 ) );
            setPipeWidth( 6 );
            setScaleMaxMajor( 6 );
            setScaleMaxMinor( 5 );
            setFillBrush( Qt::darkMagenta );

#if 0
            QwtLinearColorMap* colorMap =
                new QwtLinearColorMap( Qt::blue, Qt::red );

            colorMap->addColorStop( 0.2, Qt::yellow );
            colorMap->addColorStop( 0.3, Qt::cyan );
            colorMap->addColorStop( 0.4, Qt::green );
            colorMap->addColorStop( 0.5, Qt::magenta );
            colorMap->setMode( QwtLinearColorMap::FixedColors );

            setColorMap( colorMap );
#endif
        }
    };

    class ValueBar : public QWidget
    {
      public:
        ValueBar( Qt::Orientation orientation,
            const QString& text, double value )
        {
            QLabel* label = new QLabel( text );
            label->setFont( QFont( "Helvetica", 10 ) );

            m_gauge = new Gauge();
            m_gauge->setValue( value );
            m_gauge->setOrientation( orientation );

            QVBoxLayout* layout = new QVBoxLayout( this );
            layout->setContentsMargins( QMargins() );
            layout->setSpacing( 0 );

            if ( orientation == Qt::Horizontal )
            {
                label->setAlignment( Qt::AlignCenter );
                m_gauge->setScalePosition( QwtThermo::LeadingScale );

                layout->addWidget( label );
                layout->addWidget( m_gauge );
            }
            else
            {
                label->setAlignment( Qt::AlignRight );
                m_gauge->setScalePosition( QwtThermo::TrailingScale );

                layout->addWidget( m_gauge, 10, Qt::AlignHCenter );
                layout->addWidget( label, 0 );
            }
        }

      private:
        QwtThermo* m_gauge;
    };

    class InfoBox : public QGroupBox
    {
      public:
        InfoBox( Qt::Orientation orientation, const QString& title )
            : QGroupBox( title )
        {
            const int margin = 15;
            const int spacing = 5;

            const QBoxLayout::Direction dir = ( orientation == Qt::Vertical )
                ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;

            QBoxLayout* layout = new QBoxLayout( dir, this );
            layout->setContentsMargins( margin, margin, margin, margin );
            layout->setSpacing( spacing );

            setFont( QFont( "Helvetica", 10 ) );
        }
    };

    class MemoryBox : public InfoBox
    {
      public:
        MemoryBox()
            : InfoBox( Qt::Vertical, "Memory Usage" )
        {
            const Qt::Orientation o = Qt::Horizontal;

            auto layout = qobject_cast< QBoxLayout* >( this->layout() );

            layout->addWidget( new ValueBar( o, "Used", 57 ) );
            layout->addWidget( new ValueBar( o, "Shared", 17 ) );
            layout->addWidget( new ValueBar( o, "Cache", 30 ) );
            layout->addWidget( new ValueBar( o, "Buffers", 22 ) );
            layout->addWidget( new ValueBar( o, "Swap Used", 57 ) );
            layout->addWidget( new QWidget(), 10 ); // spacer
        }
    };

    class CpuBox : public InfoBox
    {
      public:
        CpuBox()
            : InfoBox( Qt::Horizontal, "Cpu Usage" )
        {
            const Qt::Orientation o = Qt::Vertical;

            auto layout = qobject_cast< QBoxLayout* >( this->layout() );

            layout->addWidget( new ValueBar( o, "User", 57 ) );
            layout->addWidget( new ValueBar( o, "Total", 73 ) );
            layout->addWidget( new ValueBar( o, "System", 16 ) );
            layout->addWidget( new ValueBar( o, "Idle", 27 ) );
        }
    };

    class SystemBox : public QFrame
    {
      public:
        SystemBox( QWidget* parent = NULL )
            : QFrame( parent )
        {
            QHBoxLayout* layout = new QHBoxLayout( this );

            layout->setContentsMargins( 10, 10, 10, 10 );
            layout->addWidget( new MemoryBox(), 10 );
            layout->addWidget( new CpuBox(), 0 );
        }
    };
}

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    SystemBox box;
    box.resize( box.sizeHint().expandedTo( QSize( 600, 400 ) ) );
    box.show();

    return app.exec();
}
