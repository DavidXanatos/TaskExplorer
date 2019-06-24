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

#ifndef ADMINAUTHORIZATION_H
#define ADMINAUTHORIZATION_H

#include "../src/qtservice.h"

#include <QtCore/QObject>

class AdminAuthorizationBase
{
protected:
    AdminAuthorizationBase();

public:
    virtual ~AdminAuthorizationBase() {}

    virtual bool authorize() = 0;
    bool isAuthorized() const;

protected:  
    void setAuthorized();

private:
    bool m_authorized;
};

class QT_QTSERVICE_EXPORT AdminAuthorization : public QObject, public AdminAuthorizationBase
{
    Q_OBJECT
    Q_PROPERTY(bool authorized READ isAuthorized)

public:
    AdminAuthorization();
#ifdef Q_OS_MAC
    ~AdminAuthorization();
#endif

    bool execute(QWidget *dialogParent, const QString &programs, const QStringList &arguments);
    static bool hasAdminRights();

public Q_SLOTS:
    bool authorize();

Q_SIGNALS:
    void authorized();

#ifdef Q_OS_MAC
private:
    class Private;
    Private *d;
#endif
};

#endif // ADMINAUTHORIZATION_H
