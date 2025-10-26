// Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
// Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QRPMSGPORTGLOBAL_H
#define QRPMSGPORTGLOBAL_H

#include <QtCore/qstring.h>
#include <QtCore/qglobal.h>
QT_BEGIN_NAMESPACE

#ifndef QT_STATIC
#  if defined(QT_BUILD_QTRPMSG_LIB)
#    define Q_RPMSG_EXPORT Q_DECL_EXPORT
#  else
#    define Q_RPMSG_EXPORT Q_DECL_IMPORT
#  endif
#else
#  define Q_RPMSG_EXPORT
#endif

QT_END_NAMESPACE
#endif // QRPMSGGLOBAL_H
