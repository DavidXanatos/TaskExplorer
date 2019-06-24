#ifndef QTSERVICE_GLOBAL_H
#define QTSERVICE_GLOBAL_H

#include <QtCore/qglobal.h>

#ifdef QTSERVICE_LIB
# define QTSERVICE_EXPORT Q_DECL_EXPORT
#else
# define QTSERVICE_EXPORT Q_DECL_IMPORT
#endif

#endif // QTSERVICE_GLOBAL_H
