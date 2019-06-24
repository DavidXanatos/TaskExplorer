/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "adminauthorization.h"

#include <QtCore/QFile>
#include <QDebug>

#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>

#include <cstdlib>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef Q_OS_LINUX
#include <linux/limits.h>
#include <pty.h>
#else
#include <util.h>
#endif

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <iostream>

#define SU_COMMAND "/usr/bin/sudo"
//#define SU_COMMAND "/bin/echo"

AdminAuthorization::AdminAuthorization()
{
}

bool AdminAuthorization::authorize()
{
    return true;
}
  
static QString getPassword(QWidget *)
{
#if QT_VERSION < 0x050000
    if (QApplication::type() == QApplication::GuiClient)
#else
    if (qobject_cast<QApplication*> (qApp) != 0)
#endif
    {
        bool ok = false;
        const QString result = QInputDialog::getText(0, QObject::tr("Authorization required"),
           QObject::tr("Enter your password to authorize for sudo:"),
           QLineEdit::Password, QString(), &ok);
        return ok ? result : QString();
    } else {
        std::cout << QObject::tr("Authorization required").toStdString() << std::endl;
        std::cout << QObject::tr("Enter your password to authorize for sudo:").toStdString() << std::endl;
        std::string password;
        std::cin >> password;
        return QString::fromStdString(password);
    }
}

static void printError(QWidget *parent, const QString &value)
{
#if QT_VERSION < 0x050000
    if (QApplication::type() == QApplication::GuiClient)
#else
    if (qobject_cast<QApplication*> (qApp) != 0)
#endif
    {
        QMessageBox::critical(parent, QObject::tr( "Error acquiring admin rights" ), value,
            QMessageBox::Ok, QMessageBox::Ok);
    } else {
        std::cout << value.toStdString() << std::endl;
    }
}

bool AdminAuthorization::execute(QWidget *parent, const QString &program, const QStringList &arguments)
{
    const QString fallback = program + QLatin1String(" ") + arguments.join(QLatin1String(" "));
    qDebug() << "Fallback:" << fallback;

    // as we cannot pipe the password to su in QProcess, we need to setup a pseudo-terminal for it
    int masterFD = -1;
    int slaveFD = -1;
    char ptsn[ PATH_MAX ];

    if (::openpty(&masterFD, &slaveFD, ptsn, 0, 0))
        return false;
    
    masterFD = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (masterFD < 0)
        return false;

    const QByteArray ttyName = ::ptsname(masterFD);

    if (::grantpt(masterFD)) {
        ::close(masterFD);
        return false;
    }
  
    ::revoke(ttyName);
    ::unlockpt(masterFD);

    slaveFD = ::open(ttyName, O_RDWR | O_NOCTTY);
    if (slaveFD < 0) {
        ::close(masterFD);
        return false;
    }

    ::fcntl(masterFD, F_SETFD, FD_CLOEXEC);
    ::fcntl(slaveFD, F_SETFD, FD_CLOEXEC);
    int pipedData[2];
    if (pipe(pipedData) != 0)
        return false;

    int flags = ::fcntl(pipedData[0], F_GETFD);
    if (flags != -1)
        ::fcntl(pipedData[0], F_SETFL, flags | O_NONBLOCK);

    pid_t child = fork();

    if (child < -1) {
        ::close(masterFD);
        ::close(slaveFD);
        ::close(pipedData[0]);
        ::close(pipedData[1]);
        return false;
    }

    // parent process
    else if (child > 0) {
        ::close(slaveFD);
        //close writing end of pipe
        ::close(pipedData[1]);

        QRegExp re(QLatin1String("[Pp]assword.*:"));
        QByteArray errData;
        flags = ::fcntl(masterFD, F_GETFD);
//        if (flags != -1)
//            ::fcntl(masterFD, F_SETFL, flags | O_NONBLOCK);
        int bytes = 0;
        int errBytes = 0;
        char buf[1024];
        while (bytes >= 0) {
            int state;
            if (::waitpid(child, &state, WNOHANG) == -1)
                break;
            bytes = ::read(masterFD, buf, 1023);
            errBytes = ::read(pipedData[0], buf, 1023);
            if (errBytes > 0)
                errData.append(buf, errBytes);
            if (bytes > 0) {
                const QString line = QString::fromLatin1(buf, bytes);
                if (re.indexIn(line) != -1) {
                    const QString password = getPassword(parent);
                    if (password.isEmpty()) {
                        QByteArray pwd = password.toLatin1();
                        for (int i = 0; i < 3; ++i) {
                            ::write(masterFD, pwd.data(), pwd.length());
                            ::write(masterFD, "\n", 1);
                        }
                        return false;
                    }
                    QByteArray pwd = password.toLatin1();
                    ::write(masterFD, pwd.data(), pwd.length());
                    ::write(masterFD, "\n", 1);
                    ::read(masterFD, buf, pwd.length() + 1);
                }                
            }
            if (bytes == 0)
                ::usleep(100000);
        }        
        if (!errData.isEmpty()) {
            printError(parent, QString::fromLocal8Bit(errData.constData()));
            return false;
        }

        int status;
        child = ::wait(&status);
        const int exited = WIFEXITED(status);
        const int exitStatus = WEXITSTATUS(status);
        ::close(pipedData[1]);
        if (exited)
            return exitStatus == 0;
            
        return false;
    }    

    // child process
    else {
        ::close(pipedData[0]);
        // Reset signal handlers
        for (int sig = 1; sig < NSIG; ++sig)
            signal(sig, SIG_DFL);
        signal(SIGHUP, SIG_IGN);
      
        ::setsid();

        ::ioctl(slaveFD, TIOCSCTTY, 1);
        int pgrp = ::getpid();
        ::tcsetpgrp(slaveFD, pgrp);

        ::dup2(slaveFD, 0);
        ::dup2(slaveFD, 1);
        ::dup2(pipedData[1], 2);

        // close all file descriptors
        struct rlimit rlp;
        getrlimit(RLIMIT_NOFILE, &rlp);
        for (int i = 3; i < static_cast<int>(rlp.rlim_cur); ++i)
            ::close(i);
 
        char **argp = (char **) ::malloc(arguments.count() + 4 * sizeof(char *));
        QList<QByteArray> args;
        args.push_back(SU_COMMAND);
        args.push_back("-b");
        args.push_back(program.toLocal8Bit());
        for (QStringList::const_iterator it = arguments.begin(); it != arguments.end(); ++it)
            args.push_back(it->toLocal8Bit());

        int i = 0;
        for (QList<QByteArray>::iterator it = args.begin(); it != args.end(); ++it, ++i)
            argp[i] = it->data();
        argp[i] = 0;

        ::unsetenv("LANG");
        ::unsetenv("LC_ALL");

        ::execv(SU_COMMAND, argp);
        _exit(0);
        return false;
    }
}

// has no guarantee to work
bool AdminAuthorization::hasAdminRights()
{
    return getuid() == 0;
}
