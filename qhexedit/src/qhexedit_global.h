#ifndef QHEXEDIT_GLOBAL_H
#define QHEXEDIT_GLOBAL_H

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# ifdef QHEXEDIT_EXPORTS
#  define QHEXEDIT_API Q_DECL_EXPORT
# else
#  define QHEXEDIT_API Q_DECL_IMPORT
# endif
#else
# define QHEXEDIT_API
#endif

/*
#ifdef QHEXEDIT_EXPORTS
#define QHEXEDIT_API Q_DECL_EXPORT
#elif QHEXEDIT_IMPORTS
#define QHEXEDIT_API Q_DECL_IMPORT
#else
#define QHEXEDIT_API
#endif
*/

#endif // QHEXEDIT_GLOBAL_H