/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "RandomPlot.h"
#include "MainWindow.h"
#include "start.xpm"
#include "clear.xpm"

#include <QAction>
#include <QLabel>
#include <QLayout>
#include <QStatusBar>
#include <QToolBar>
#include <QSpinBox>
#include <QCheckBox>
#include <QWhatsThis>
#include <QPixmap>

namespace
{
    class ToolBar : public QToolBar
    {
      public:
        ToolBar( QMainWindow* parent )
            : QToolBar( parent )
        {
        }

        void addSpacing( int spacing )
        {
            QLabel* label = new QLabel();
            label->setFixedWidth( spacing );

            addWidget( label );
        }
    };
}

class Counter : public QWidget
{
  public:
    Counter( const QString& prefix, const QString& suffix,
            int min, int max, int step, QWidget* parent = NULL )
        : QWidget( parent )
    {
        QHBoxLayout* layout = new QHBoxLayout( this );

        if ( !prefix.isEmpty() )
            layout->addWidget( new QLabel( prefix + " " ) );

        m_counter = new QSpinBox();
        m_counter->setRange( min, max );
        m_counter->setSingleStep( step );

        layout->addWidget( m_counter );

        if ( !suffix.isEmpty() )
            layout->addWidget( new QLabel( QString( " " ) + suffix ) );
    }

    void setValue( int value ) { m_counter->setValue( value ); }
    int value() const { return m_counter->value(); }

  private:
    QSpinBox* m_counter;
};

MainWindow::MainWindow()
{
    addToolBar( toolBar() );
#ifndef QT_NO_STATUSBAR
    ( void )statusBar();
#endif

    m_plot = new RandomPlot();

    const int margin = 4;
    m_plot->setContentsMargins( margin, margin, margin, margin );

    setCentralWidget( m_plot );

    connect( m_startAction, SIGNAL(toggled(bool)), this, SLOT(appendPoints(bool)) );
    connect( m_clearAction, SIGNAL(triggered()), m_plot, SLOT(clear()) );
    connect( m_symbolType, SIGNAL(toggled(bool)), m_plot, SLOT(showSymbols(bool)) );
    connect( m_plot, SIGNAL(running(bool)), this, SLOT(showRunning(bool)) );
    connect( m_plot, SIGNAL(elapsed(int)), this, SLOT(showElapsed(int)) );

    initWhatsThis();

    setContextMenuPolicy( Qt::NoContextMenu );
}

QToolBar* MainWindow::toolBar()
{
    setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
    setIconSize( QSize( 22, 22 ) );

    m_startAction = new QAction( QPixmap( start_xpm ), "Start", this );
    m_startAction->setCheckable( true );
    m_clearAction = new QAction( QPixmap( clear_xpm ), "Clear", this );

    QAction* whatsThisAction = QWhatsThis::createAction();
    whatsThisAction->setText( "Help" );

    QWidget* hBox = new QWidget();

    m_symbolType = new QCheckBox( "Symbols", hBox );
    m_symbolType->setChecked( true );

    m_randomCount = new Counter( "Points", QString(), 1, 100000, 100 );
    m_randomCount->setValue( 1000 );

    m_timerCount = new Counter( "Delay", "ms", 0, 100000, 100 );
    m_timerCount->setValue( 0 );

    QHBoxLayout* layout = new QHBoxLayout( hBox );
    layout->setContentsMargins( QMargins() );
    layout->setSpacing( 0 );
    layout->addSpacing( 10 );
    layout->addWidget( new QWidget( hBox ), 10 ); // spacer
    layout->addWidget( m_symbolType );
    layout->addSpacing( 5 );
    layout->addWidget( m_randomCount );
    layout->addSpacing( 5 );
    layout->addWidget( m_timerCount );

    showRunning( false );

    ToolBar* toolBar = new ToolBar( this );
    toolBar->setAllowedAreas( Qt::TopToolBarArea | Qt::BottomToolBarArea );
    toolBar->addAction( m_startAction );
    toolBar->addAction( m_clearAction );
    toolBar->addAction( whatsThisAction );
    toolBar->addWidget( hBox );

    return toolBar;
}

void MainWindow::appendPoints( bool on )
{
    if ( on )
    {
        m_plot->append( m_timerCount->value(),
            m_randomCount->value() );
    }
    else
    {
        m_plot->stop();
    }
}

void MainWindow::showRunning( bool running )
{
    m_randomCount->setEnabled( !running );
    m_timerCount->setEnabled( !running );
    m_startAction->setChecked( running );
    m_startAction->setText( running ? "Stop" : "Start" );
}

void MainWindow::showElapsed( int ms )
{
    QString text;
    text.setNum( ms );
    text += " ms";

    statusBar()->showMessage( text );
}

void MainWindow::initWhatsThis()
{
    const char* text1 =
        "Zooming is enabled until the selected area gets "
        "too small for the significance on the axes.\n\n"
        "You can zoom in using the left mouse button.\n"
        "The middle mouse button is used to go back to the "
        "previous zoomed area.\n"
        "The right mouse button is used to unzoom completely.";

    const char* text2 =
        "Number of random points that will be generated.";

    const char* text3 =
        "Delay between the generation of two random points.";

    const char* text4 =
        "Start generation of random points.\n\n"
        "The intention of this example is to show how to implement "
        "growing curves. The points will be generated and displayed "
        "one after the other.\n"
        "To check the performance, a small delay and a large number "
        "of points are useful. To watch the curve growing, a delay "
        " > 300 ms and less points are better.\n"
        "To inspect the curve, stacked zooming is implemented using the "
        "mouse buttons on the plot.";

    const char* text5 = "Remove all points.";

    m_plot->setWhatsThis( text1 );
    m_randomCount->setWhatsThis( text2 );
    m_timerCount->setWhatsThis( text3 );
    m_startAction->setWhatsThis( text4 );
    m_clearAction->setWhatsThis( text5 );
}

#include "moc_MainWindow.cpp"
