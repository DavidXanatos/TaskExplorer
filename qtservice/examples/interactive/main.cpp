/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Solutions component.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QApplication>
#include <QDesktopWidget>
#include <QLabel>
#include <QDir>
#include <QSettings>
#include "qtservice.h"

class InteractiveService : public QtService<QApplication>
{
public:
    InteractiveService(int argc, char **argv);
    ~InteractiveService();

protected:

    void start();
    void stop();
    void pause();
    void resume();
    void processCommand(int code);

private:
    QLabel *gui;
};

InteractiveService::InteractiveService(int argc, char **argv)
    : QtService<QApplication>(argc, argv, "Qt Interactive Service"), gui(0)
{
    setServiceDescription("A Qt service with user interface.");
    setServiceFlags(QtServiceBase::CanBeSuspended);
}

InteractiveService::~InteractiveService()
{
}

void InteractiveService::start()
{
#if defined(Q_OS_WIN)
    if ((QSysInfo::WindowsVersion & QSysInfo::WV_NT_based) &&
        (QSysInfo::WindowsVersion >= QSysInfo::WV_VISTA)) {
        logMessage( "Service GUI not allowed on Windows Vista. See the documentation for this example for more information.", QtServiceBase::Error );
        return;
    }
#endif

    qApp->setQuitOnLastWindowClosed(false);

    gui = new QLabel("Service", 0, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    gui->move(QApplication::desktop()->availableGeometry().topLeft());
    gui->show();
}

void InteractiveService::stop()
{
    delete gui;
}

void InteractiveService::pause()
{
    if (gui)
	gui->hide();
}

void InteractiveService::resume()
{
    if (gui)
	gui->show();
}

void InteractiveService::processCommand(int code)
{
    gui->setText("Command code " + QString::number(code));
    gui->adjustSize();
}

int main(int argc, char **argv)
{
#if !defined(Q_OS_WIN)
    // QtService stores service settings in SystemScope, which normally require root privileges.
    // To allow testing this example as non-root, we change the directory of the SystemScope settings file.
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, QDir::tempPath());
    qWarning("(Example uses dummy settings file: %s/QtSoftware.conf)", QDir::tempPath().toLatin1().constData());
#endif
    InteractiveService service(argc, argv);
    return service.exec();
}
