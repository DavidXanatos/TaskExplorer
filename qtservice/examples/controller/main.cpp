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

#include <QStringList>
#include <QDir>
#include <QSettings>
#include "qtservice.h"

int processArgs(int argc, char **argv)
{
    if (argc > 2) {
        QString arg1(argv[1]);
	if (arg1 == QLatin1String("-i") ||
            arg1 == QLatin1String("-install")) {
	    if (argc > 2) {
                QString account;
                QString password;
		QString path(argv[2]);
                if (argc > 3)
                    account = argv[3];
                if (argc > 4)
                    password = argv[4];
                printf("The service %s installed.\n",
                       (QtServiceController::install(path, account, password) ? "was" : "was not"));
                return 0;
            }
        } else {
            QString serviceName(argv[1]);
            QtServiceController controller(serviceName);
            QString option(argv[2]);
            if (option == QLatin1String("-u") ||
                option == QLatin1String("-uninstall")) {
                printf("The service \"%s\" %s uninstalled.\n",
                            controller.serviceName().toLatin1().constData(),
                            (controller.uninstall() ? "was" : "was not"));
                return 0;
            } else if (option == QLatin1String("-s") ||
                       option == QLatin1String("-start")) {
                QStringList args;
                for (int i = 3; i < argc; ++i)
                    args.append(QString::fromLocal8Bit(argv[i]));
                printf("The service \"%s\" %s started.\n",
                       controller.serviceName().toLatin1().constData(),
                            (controller.start(args) ? "was" : "was not"));
                return 0;
            } else if (option == QLatin1String("-t") ||
                       option == QLatin1String("-terminate")) {
                printf("The service \"%s\" %s stopped.\n",
                       controller.serviceName().toLatin1().constData(),
                       (controller.stop() ? "was" : "was not"));
                return 0;
            } else if (option == QLatin1String("-p") ||
                    option == QLatin1String("-pause")) {
                printf("The service \"%s\" %s paused.\n",
                       controller.serviceName().toLatin1().constData(),
                       (controller.pause() ? "was" : "was not"));
                return 0;
            } else if (option == QLatin1String("-r") ||
                       option == QLatin1String("-resume")) {
                printf("The service \"%s\" %s resumed.\n",
                       controller.serviceName().toLatin1().constData(),
                       (controller.resume() ? "was" : "was not"));
                return 0;
            } else if (option == QLatin1String("-c") ||
                       option == QLatin1String("-command")) {
                if (argc > 3) {
                    QString codestr(argv[3]);
                    int code = codestr.toInt();
                    printf("The command %s sent to the service \"%s\".\n",
                           (controller.sendCommand(code) ? "was" : "was not"),
                           controller.serviceName().toLatin1().constData());
                    return 0;
                }
            } else if (option == QLatin1String("-v") ||
                    option == QLatin1String("-version")) {
                bool installed = controller.isInstalled();
                printf("The service\n"
                        "\t\"%s\"\n\n", controller.serviceName().toLatin1().constData());
                printf("is %s", (installed ? "installed" : "not installed"));
                printf(" and %s\n\n", (controller.isRunning() ? "running" : "not running"));
                if (installed) {
                    printf("path: %s\n", controller.serviceFilePath().toLatin1().data());
                    printf("description: %s\n", controller.serviceDescription().toLatin1().data());
                    printf("startup: %s\n", controller.startupType() == QtServiceController::AutoStartup ? "Auto" : "Manual");
                }
                return 0;
            }
        }
    }
    printf("controller [-i PATH | SERVICE_NAME [-v | -u | -s | -t | -p | -r | -c CODE] | -h] [-w]\n\n"
            "\t-i(nstall) PATH\t: Install the service\n"
            "\t-v(ersion)\t: Print status of the service\n"
            "\t-u(ninstall)\t: Uninstall the service\n"
            "\t-s(tart)\t: Start the service\n"
            "\t-t(erminate)\t: Stop the service\n"
            "\t-p(ause)\t: Pause the service\n"
            "\t-r(esume)\t: Resume the service\n"
            "\t-c(ommand) CODE\t: Send a command to the service\n"
            "\t-h(elp)\t\t: Print this help info\n"
            "\t-w(ait)\t\t: Wait for keypress when done\n");
    return 0;
}


int main(int argc, char **argv)
{
#if !defined(Q_OS_WIN)
    // QtService stores service settings in SystemScope, which normally require root privileges.
    // To allow testing this example as non-root, we change the directory of the SystemScope settings file.
    QSettings::setPath(QSettings::NativeFormat, QSettings::SystemScope, QDir::tempPath());
    qWarning("(Example uses dummy settings file: %s/QtSoftware.conf)", QDir::tempPath().toLatin1().constData());
#endif

    int result = processArgs(argc, argv);

    if (QString::fromLocal8Bit(argv[argc-1]) == QLatin1String("-w") ||
        QString::fromLocal8Bit(argv[argc-1]) == QLatin1String("-wait")) {
        printf("\nPress Enter to continue...");
        QFile input;
        input.open(stdin, QIODevice::ReadOnly);
        input.readLine();
        printf("\n");
    }

    return result;
}
