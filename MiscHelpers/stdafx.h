#pragma once

#define _CRT_SECURE_NO_WARNINGS



// std includes
#include <string>
#include <sstream>
#include <deque>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <memory>


// Qt includes
#include <QObject>
#include <QList>
#include <QVector>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QFile>
#include <qglobal.h>
#include <QTime>
#include <QTimer>
#include <QTimerEvent>
#include <QThread>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkProxy>
#include <QNetworkDiskCache>
#include <QTextStream>
#include <QFileInfo>
#include <QXmlStreamWriter>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QBuffer>
#include <QDir>
#include <QTemporaryFile>
#include <QMutex>
#include <QMutexLocker>
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QBitArray>
#include <QPointer>
#include <QSharedPointer>
#include <QFutureWatcher>
#include <QHostInfo>
#include <QApplication>
#include <QClipboard>

#include <QMainWindow>
#include <QWidget>
#include <QHBoxLayout>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QToolBar>
#include <QScrollBar>
#include <QStyleFactory>
#include <QSortFilterProxyModel>
#include <QStackedLayout>
#include <QTreeWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QWidgetAction>
#include <QCheckBox>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QStandardItemModel>
#include <QPainter>
#include <QGroupBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QDesktopServices>
#include <QFileDialog>
#include <QProgressBar>
#include <QInputDialog>
#include <QToolTip>
#include <QColorDialog>
#include <QToolButton>
#include <QScreen>
#include <QIdentityProxyModel>
#include <QRandomGenerator>
#include <QElapsedTimer>

// other includes

#define _T(x)      L ## x

#define STR2(X) #X
#define STR(X) STR2(X)

#define ARRSIZE(x)	(sizeof(x)/sizeof(x[0]))

#ifndef Max
#define Max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef Min
#define Min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifdef _DEBUG
#define SAFE_MODE
#endif

#include "Common/DebugHelpers.h"
