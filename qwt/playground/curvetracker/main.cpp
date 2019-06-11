#include <qapplication.h>
#include "plot.h"

int main( int argc, char **argv )
{
    QApplication a( argc, argv );

    Plot plot;
    plot.resize( 600, 400 );
    plot.show();

    return a.exec();
}
