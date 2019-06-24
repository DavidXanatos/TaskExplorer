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

#include <Security/Authorization.h>
#include <Security/AuthorizationTags.h>

#include <QtCore/QStringList>
#include <QtCore/QVector>

#include <unistd.h>


// -- AdminAuthorization::Private

class AdminAuthorization::Private
{
public:
    Private() : auth(0) { }

    AuthorizationRef auth;
};


// -- AdminAuthorization

AdminAuthorization::AdminAuthorization()
    : d(new Private)
{
    AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment, kAuthorizationFlagDefaults, &d->auth);
}

AdminAuthorization::~AdminAuthorization()
{
    AuthorizationFree(d->auth, kAuthorizationFlagDestroyRights);
    delete d;
}

bool AdminAuthorization::authorize()
{
    if (geteuid() == 0)
        setAuthorized();

    if (isAuthorized())
        return true;

    AuthorizationItem item;
    item.name = kAuthorizationRightExecute;
    item.valueLength = 0;
    item.value = NULL;
    item.flags = 0;

    AuthorizationRights rights;
    rights.count = 1;
    rights.items = &item;

    const AuthorizationFlags flags = kAuthorizationFlagDefaults | kAuthorizationFlagInteractionAllowed
        | kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights;

    const OSStatus result = AuthorizationCopyRights(d->auth, &rights, kAuthorizationEmptyEnvironment,
        flags, 0);
    if (result != errAuthorizationSuccess)
        return false;

    seteuid(0);
    setAuthorized();
    emit authorized();
    return true;
}

bool AdminAuthorization::execute(QWidget *, const QString &program, const QStringList &arguments)
{
    QVector<char *> args;
    QVector<QByteArray> utf8Args;
    foreach (const QString &argument, arguments) {
        utf8Args.push_back(argument.toUtf8());
        args.push_back(utf8Args.last().data());
    }
    args.push_back(0);

    const QByteArray utf8Program = program.toUtf8();
    const OSStatus result = AuthorizationExecuteWithPrivileges(d->auth, utf8Program.data(),
        kAuthorizationFlagDefaults, args.data(), 0);
    return result == errAuthorizationSuccess;
}

bool AdminAuthorization::hasAdminRights()
{
    return geteuid() == 0;
}
