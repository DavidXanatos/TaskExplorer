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

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QDateTime>
#include <QStringList>
#include <QDir>
#include <QSettings>

#include "qtservice.h"

// HttpDaemon is the the class that implements the simple HTTP server.
class HttpDaemon : public QTcpServer
{
    Q_OBJECT
public:
    HttpDaemon(quint16 port, QObject* parent = 0)
        : QTcpServer(parent), disabled(false)
    {
        listen(QHostAddress::Any, port);
    }

    void incomingConnection(int socket)
    {
        if (disabled)
            return;

        // When a new client connects, the server constructs a QTcpSocket and all
        // communication with the client is done over this QTcpSocket. QTcpSocket
        // works asynchronously, this means that all the communication is done
        // in the two slots readClient() and discardClient().
        QTcpSocket* s = new QTcpSocket(this);
        connect(s, SIGNAL(readyRead()), this, SLOT(readClient()));
        connect(s, SIGNAL(disconnected()), this, SLOT(discardClient()));
        s->setSocketDescriptor(socket);

        QtServiceBase::instance()->logMessage("New Connection");
    }

    void pause()
    {
        disabled = true;
    }

    void resume()
    {
        disabled = false;
    }

private slots:
    void readClient()
    {
        if (disabled)
            return;

        // This slot is called when the client sent data to the server. The
        // server looks if it was a get request and sends a very simple HTML
        // document back.
        QTcpSocket* socket = (QTcpSocket*)sender();
        if (socket->canReadLine()) {
            QStringList tokens = QString(socket->readLine()).split(QRegExp("[ \r\n][ \r\n]*"));
            if (tokens[0] == "GET") {
                QTextStream os(socket);
                os.setAutoDetectUnicode(true);
                os << "HTTP/1.0 200 Ok\r\n"
                    "Content-Type: text/html; charset=\"utf-8\"\r\n"
                    "\r\n"
                    "<h1>Nothing to see here</h1>\n"
                    << QDateTime::currentDateTime().toString() << "\n";
                socket->close();

                QtServiceBase::instance()->logMessage("Wrote to client");

                if (socket->state() == QTcpSocket::UnconnectedState) {
                    delete socket;
                    QtServiceBase::instance()->logMessage("Connection closed");
                }
            }
        }
    }
    void discardClient()
    {
        QTcpSocket* socket = (QTcpSocket*)sender();
        socket->deleteLater();

        QtServiceBase::instance()->logMessage("Connection closed");
    }

private:
    bool disabled;
};

class HttpService : public QtService<QCoreApplication>
{
public:
    HttpService(int argc, char **argv)
	: QtService<QCoreApplication>(argc, argv, "Qt HTTP Daemon")
    {
        setServiceDescription("A dummy HTTP service implemented with Qt");
        setServiceFlags(QtServiceBase::CanBeSuspended);
    }

protected:
    void start()
    {
        QCoreApplication *app = application();

#if QT_VERSION < 0x040100
        quint16 port = (app->argc() > 1) ?
                QString::fromLocal8Bit(app->argv()[1]).toUShort() : 8080;
#else
        const QStringList arguments = QCoreApplication::arguments();
        quint16 port = (arguments.size() > 1) ?
                arguments.at(1).toUShort() : 8080;
#endif
        daemon = new HttpDaemon(port, app);

        if (!daemon->isListening()) {
            logMessage(QString("Failed to bind to port %1").arg(daemon->serverPort()), QtServiceBase::Error);
            app->quit();
        }
    }

    void pause()
    {
	daemon->pause();
    }

    void resume()
    {
	daemon->resume();
    }

private:
    HttpDaemon *daemon;
};

#include "main.moc"

int main(int argc, char **argv)
{
#if !defined(Q_OS_WIN)
    // QtService stores service settings in SystemScope, which normally require root privileges.
    // To allow testing this example as non-root, we change the directory of the SystemScope settings file.
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, QDir::tempPath());
    qWarning("(Example uses dummy settings file: %s/QtSoftware.conf)", QDir::tempPath().toLatin1().constData());
#endif
    HttpService service(argc, argv);
    return service.exec();
}
