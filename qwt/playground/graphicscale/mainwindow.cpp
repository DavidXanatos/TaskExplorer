/*****************************************************************************
 * Qwt Examples - Copyright (C) 2002 Uwe Rathmann
 * This file may be used under the terms of the 3-clause BSD License
 *****************************************************************************/

#include "MainWindow.h"
#include "Canvas.h"

#include <QToolBar>
#include <QToolButton>
#include <QLayout>
#include <QLabel>
#include <QFileDialog>
#include <QBuffer>
#include <QPainter>
#include <QSvgGenerator>
#include <QStatusBar>

MainWindow::MainWindow()
{
    QWidget* w = new QWidget( this );

    m_canvas[0] = new Canvas( Canvas::Svg, this );
    m_canvas[0]->setAutoFillBackground( true );
    m_canvas[0]->setPalette( Qt::gray );

    m_canvas[1] = new Canvas( Canvas::VectorGraphic, this );
    m_canvas[1]->setAutoFillBackground( true );
    m_canvas[1]->setPalette( Qt::gray );

    QVBoxLayout* vBox1 = new QVBoxLayout();
    vBox1->setContentsMargins( 0, 0, 0, 0 );
    vBox1->setSpacing( 5 );
    vBox1->addWidget( new QLabel( "SVG" ), 0, Qt::AlignCenter );
    vBox1->addWidget( m_canvas[0], 10 );

    QVBoxLayout* vBox2 = new QVBoxLayout();
    vBox2->setContentsMargins( 0, 0, 0, 0 );
    vBox2->setSpacing( 5 );
    vBox2->addWidget( new QLabel( "Vector Graphic" ), 0, Qt::AlignCenter );
    vBox2->addWidget( m_canvas[1], 10 );

    QHBoxLayout* layout = new QHBoxLayout( w );
    layout->addLayout( vBox1 );
    layout->addLayout( vBox2 );

    setCentralWidget( w );

    QToolBar* toolBar = new QToolBar( this );

    QToolButton* btnLoad = new QToolButton( toolBar );

    btnLoad->setText( "Load SVG" );
    btnLoad->setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
    toolBar->addWidget( btnLoad );

    addToolBar( toolBar );

    connect( btnLoad, SIGNAL(clicked()), this, SLOT(loadSVG()) );

#if 0
    QPainterPath path;
    path.addRect( QRectF( 1.0, 1.0, 2.0, 2.0 ) );
    loadPath( path );
#endif
}

MainWindow::~MainWindow()
{
}

void MainWindow::loadSVG()
{
    QString dir = "/home1/uwe/qwt/qwt/tests/svg";
    const QString fileName = QFileDialog::getOpenFileName( NULL,
        "Load a Scaleable Vector Graphic (SVG) Document",
        dir, "SVG Files (*.svg)" );

    if ( !fileName.isEmpty() )
        loadSVG( fileName );

    statusBar()->showMessage( fileName );
}

void MainWindow::loadSVG( const QString& fileName )
{
    QFile file( fileName );
    if ( !file.open(QIODevice::ReadOnly | QIODevice::Text) )
        return;

    const QByteArray document = file.readAll();
    file.close();

    m_canvas[0]->setSvg( document );
    m_canvas[1]->setSvg( document );
}


void MainWindow::loadPath( const QPainterPath& path )
{
    QBuffer buf;

    QSvgGenerator generator;
    generator.setOutputDevice( &buf );

    QPainter painter( &generator );
    painter.setRenderHint( QPainter::Antialiasing, false );
    painter.setPen( QPen( Qt::blue, 0 ) );
    painter.setBrush( Qt::darkCyan );
    painter.drawPath( path );
    painter.end();

    m_canvas[0]->setSvg( buf.data() );
    m_canvas[1]->setSvg( buf.data() );
}

#include "moc_MainWindow.cpp"
