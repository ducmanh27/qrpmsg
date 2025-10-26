// Copyright (C) 2025 Phan Duc Manh <manhpd9@viettel.com.vn>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qrpmsginfo_p.h"
#include "qrpmsg_p.h"

#include <QtCore/qdeadlinetimer.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qmap.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qstandardpaths.h>

#include <private/qcore_unix_p.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

#include "rpmsglinuxhelper.h"
QT_BEGIN_NAMESPACE

QString QRPMsgInfoPrivate::channelNameToSystemLocation(const QString &source)
{
    return (source.startsWith(QLatin1Char('/'))
            || source.startsWith(QLatin1String("./"))
            || source.startsWith(QLatin1String("../")))
               ? source : (QLatin1String("/dev/") + source);
}

QString QRPMsgInfoPrivate::channelNameFromSystemLocation(const QString &source)
{
    return source.startsWith(QLatin1String("/dev/"))
    ? source.mid(5) : source;
}

QT_END_NAMESPACE
