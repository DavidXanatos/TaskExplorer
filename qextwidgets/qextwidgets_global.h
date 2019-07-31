#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(QEXTWIDGETS_LIB)
#  define QEXTWIDGETS_EXPORT Q_DECL_EXPORT
# else
#  define QEXTWIDGETS_EXPORT Q_DECL_IMPORT
# endif
#else
# define QEXTWIDGETS_EXPORT
#endif
