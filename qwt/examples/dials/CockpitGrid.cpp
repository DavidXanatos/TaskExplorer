/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "CockpitGrid.h"
#include "AttitudeIndicator.h"
#include "SpeedoMeter.h"

#include <QwtAnalogClock>
#include <QwtRoundScaleDraw>
#include <QwtDialNeedle>

#include <QLayout>
#include <QTimer>
#include <QTimerEvent>

static QPalette colorTheme( const QColor& base )
{
    QPalette palette;
    palette.setColor( QPalette::Base, base );
    palette.setColor( QPalette::Window, base.darker( 150 ) );
    palette.setColor( QPalette::Mid, base.darker( 110 ) );
    palette.setColor( QPalette::Light, base.lighter( 170 ) );
    palette.setColor( QPalette::Dark, base.darker( 170 ) );
    palette.setColor( QPalette::Text, base.darker( 200 ).lighter( 800 ) );
    palette.setColor( QPalette::WindowText, base.darker( 200 ) );

    return palette;
}

namespace
{
    class Clock : public QwtAnalogClock
    {
      public:
        Clock( QWidget* parent = NULL )
            : QwtAnalogClock( parent )
        {
#if 0
            // disable minor ticks
            scaleDraw()->setTickLength( QwtScaleDiv::MinorTick, 0 );
#endif

            const QColor knobColor = QColor( Qt::gray ).lighter( 130 );

            for ( int i = 0; i < QwtAnalogClock::NHands; i++ )
            {
                QColor handColor = QColor( Qt::gray ).lighter( 150 );
                int width = 8;

                if ( i == QwtAnalogClock::SecondHand )
                {
                    handColor = Qt::gray;
                    width = 5;
                }

                QwtDialSimpleNeedle* hand = new QwtDialSimpleNeedle(
                    QwtDialSimpleNeedle::Arrow, true, handColor, knobColor );
                hand->setWidth( width );

                setHand( static_cast< QwtAnalogClock::Hand >( i ), hand );
            }

            QTimer* timer = new QTimer( this );
            timer->connect( timer, SIGNAL(timeout()), this, SLOT(setCurrentTime()) );
            timer->start( 1000 );
        }
    };

    class Speedo : public SpeedoMeter
    {
      public:
        Speedo( QWidget* parent = NULL )
            : SpeedoMeter( parent )
        {
            setScaleStepSize( 20.0 );
            setScale( 0.0, 240.0 );
            scaleDraw()->setPenWidthF( 2 );

            m_timerId = startTimer( 50 );
        }

      protected:
        virtual void timerEvent( QTimerEvent* event ) QWT_OVERRIDE
        {
            if ( event->timerId() == m_timerId )
            {
                changeSpeed();
                return;
            }

            SpeedoMeter::timerEvent( event );
        }

      private:
        void changeSpeed()
        {
            static double offset = 0.8;

            double speed = value();

            if ( ( speed < 7.0 && offset < 0.0 ) ||
                ( speed > 203.0 && offset > 0.0 ) )
            {
                offset = -offset;
            }

            static int counter = 0;

            switch( counter++ % 12 )
            {
                case 0:
                case 2:
                case 7:
                case 8:
                    break;
                default:
                    setValue( speed + offset );
            }
        }

        int m_timerId;
    };

    class AttitudeInstrument : public AttitudeIndicator
    {
      public:
        AttitudeInstrument( QWidget* parent = NULL )
            : AttitudeIndicator( parent )
        {
            scaleDraw()->setPenWidthF( 3 );
            m_timerId = startTimer( 100 );
        }

      protected:
        virtual void timerEvent( QTimerEvent* event ) QWT_OVERRIDE
        {
            if ( event->timerId() == m_timerId )
            {
                changeAngle();
                changeGradient();

                return;
            }

            AttitudeIndicator::timerEvent( event );
        }

      private:
        void changeAngle()
        {
            static double offset = 0.05;

            double angle = this->angle();
            if ( angle > 180.0 )
                angle -= 360.0;

            if ( ( angle < -5.0 && offset < 0.0 ) ||
                ( angle > 5.0 && offset > 0.0 ) )
            {
                offset = -offset;
            }

            setAngle( angle + offset );
        }

        void changeGradient()
        {
            static double offset = 0.005;

            double gradient = this->gradient();

            if ( ( gradient < -0.05 && offset < 0.0 ) ||
                ( gradient > 0.05 && offset > 0.0 ) )
            {
                offset = -offset;
            }

            setGradient( gradient + offset );
        }

        int m_timerId;
    };
}

CockpitGrid::CockpitGrid( QWidget* parent )
    : QFrame( parent )
{
    setAutoFillBackground( true );

    setPalette( colorTheme( QColor( Qt::darkGray ).darker( 150 ) ) );

    QGridLayout* layout = new QGridLayout( this );
    layout->setSpacing( 5 );
    layout->setContentsMargins( QMargins() );

    for ( int i = 0; i < 3; i++ )
    {
        QwtDial* dial = createDial( i );
        layout->addWidget( dial, 0, i );
    }

    for ( int i = 0; i < layout->columnCount(); i++ )
        layout->setColumnStretch( i, 1 );
}

QwtDial* CockpitGrid::createDial( int pos )
{
    QwtDial* dial = NULL;

    switch( pos )
    {
        case 0:
        {
            dial = new Clock( this );
            break;
        }
        case 1:
        {
            dial = new Speedo( this );
            break;
        }
        case 2:
        {
            dial = new AttitudeInstrument( this );
            break;
        }
    }

    if ( dial )
    {
        dial->setReadOnly( true );
        dial->setLineWidth( 4 );
        dial->setFrameShadow( QwtDial::Sunken );
    }

    return dial;
}
