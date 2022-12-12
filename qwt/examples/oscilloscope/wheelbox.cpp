/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "WheelBox.h"

#include <QwtWheel>
#include <QLabel>
#include <QLCDNumber>
#include <QLayout>
#include <QWheelEvent>
#include <QApplication>

namespace
{
    class Wheel : public QwtWheel
    {
      public:
        Wheel( QWidget* parent )
            : QwtWheel( parent )
            , m_ignoreWheelEvent( false )
        {
            setFocusPolicy( Qt::WheelFocus );
            parent->installEventFilter( this );
        }

        virtual bool eventFilter( QObject* object, QEvent* event ) QWT_OVERRIDE
        {
            if ( event->type() == QEvent::Wheel && !m_ignoreWheelEvent )
            {
                const QWheelEvent* we = static_cast< QWheelEvent* >( event );

                const QPoint pos = wheelRect().center();

#if QT_VERSION >= 0x050c00
                QWheelEvent wheelEvent(
                    pos, mapToGlobal( pos ),
                    we->pixelDelta(), we->angleDelta(),
                    we->buttons(), we->modifiers(),
                    we->phase(), we->inverted() );
#else
                QWheelEvent wheelEvent(
                    pos, we->delta(),
                    we->buttons(), we->modifiers(),
                    we->orientation() );
#endif

                m_ignoreWheelEvent = true;
                QApplication::sendEvent( this, &wheelEvent );
                m_ignoreWheelEvent = false;

                return true;
            }

            return QwtWheel::eventFilter( object, event );
        }
      private:
        bool m_ignoreWheelEvent;
    };
}

WheelBox::WheelBox( const QString& title,
        double min, double max, double stepSize, QWidget* parent )
    : QWidget( parent )
{
    m_number = new QLCDNumber();
    m_number->setSegmentStyle( QLCDNumber::Filled );
    m_number->setAutoFillBackground( true );
    m_number->setFixedHeight( m_number->sizeHint().height() * 2 );
    m_number->setFocusPolicy( Qt::WheelFocus );

    QPalette pal( Qt::black );
    pal.setColor( QPalette::WindowText, Qt::green );
    m_number->setPalette( pal );

    m_wheel = new Wheel( this );
    m_wheel->setOrientation( Qt::Vertical );
    m_wheel->setInverted( true );
    m_wheel->setRange( min, max );
    m_wheel->setSingleStep( stepSize );
    m_wheel->setPageStepCount( 5 );
    m_wheel->setFixedHeight( m_number->height() );

    m_number->setFocusProxy( m_wheel );

    QFont font( "Helvetica", 10 );
    font.setBold( true );

    m_label = new QLabel( title );
    m_label->setFont( font );

    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->setContentsMargins( 0, 0, 0, 0 );
    hLayout->setSpacing( 2 );
    hLayout->addWidget( m_number, 10 );
    hLayout->addWidget( m_wheel );

    QVBoxLayout* vLayout = new QVBoxLayout( this );
    vLayout->addLayout( hLayout, 10 );
    vLayout->addWidget( m_label, 0, Qt::AlignTop | Qt::AlignHCenter );

    connect( m_wheel, SIGNAL(valueChanged(double)),
        m_number, SLOT(display(double)) );

    connect( m_wheel, SIGNAL(valueChanged(double)),
        this, SIGNAL(valueChanged(double)) );
}

void WheelBox::setTheme( const QColor& color )
{
    m_wheel->setPalette( color );
}

QColor WheelBox::theme() const
{
    return m_wheel->palette().color( QPalette::Window );
}

void WheelBox::setValue( double value )
{
    m_wheel->setValue( value );
    m_number->display( value );
}

double WheelBox::value() const
{
    return m_wheel->value();
}

#include "moc_WheelBox.cpp"
