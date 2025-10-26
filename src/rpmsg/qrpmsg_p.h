// Copyright (C) 2025 Phan Duc Manh <manhpd9@viettel.com.vn>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QRPMSG_P_H
#define QRPMSG_P_H
//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qrpmsg.h"

#include <qdeadlinetimer.h>

#include <private/qiodevice_p.h>
#include <private/qproperty_p.h>

#include <memory>

#  include <QtCore/qlockfile.h>
#  include <QtCore/qfileinfo.h>
#  include <QtCore/qstringlist.h>
#  include <limits.h>
#  include <termios.h>
#include <linux/rpmsg.h>

#ifndef QRPMSG_BUFFERSIZE
#define QRPMSG_BUFFERSIZE 32768
#endif
QT_BEGIN_NAMESPACE
class QWinOverlappedIoNotifier;
class QTimer;
class QSocketNotifier;

QString rpMsgLockFilePath(const QString &channelName);

class QRPMsgErrorInfo
{
public:
    QRPMsgErrorInfo(QRPMsg::RPMsgError newErrorCode = QRPMsg::UnknownError,
                         const QString &newErrorString = QString());
    QRPMsg::RPMsgError errorCode = QRPMsg::UnknownError;
    QString errorString;
};

class QRPMsgPrivate : public QIODevicePrivate
{
public:
    Q_DECLARE_PUBLIC(QRPMsg)
    QRPMsgPrivate();

    bool open(QIODevice::OpenMode mode);
    void close();

    QRPMsgErrorInfo getSystemError(int systemErrorCode = -1) const;

    void setError(const QRPMsgErrorInfo &errorInfo);

    qint64 writeData(const char *data, qint64 maxSize);

    bool initialize(QIODevice::OpenMode mode);

    bool startAsyncRead();

    bool emittedReadyRead = false;

    bool isReadNotificationEnabled() const;
    void setReadNotificationEnabled(bool enable);
    bool isWriteNotificationEnabled() const;
    void setWriteNotificationEnabled(bool enable);

    void setBindableError(QRPMsg::RPMsgError error)
    { setError(error); }
    Q_OBJECT_COMPAT_PROPERTY_WITH_ARGS(QRPMsgPrivate, QRPMsg::RPMsgError, error,
                                       &QRPMsgPrivate::setBindableError, QRPMsg::NoError)

    QString systemLocation;
    qint64 readBufferMaxSize = 0;
    qint64 writeBufferMaxSize = 0;

    qint64 readFromEndpoint(char *data, qint64 maxSize);
    qint64 writeToEndpoint(const char *data, qint64 maxSize);

    bool readNotification();
    bool startAsyncWrite();
    bool completeAsyncWrite();
    void handleException();

    int descriptor = -1;

    QSocketNotifier *readNotifier = nullptr;
    QSocketNotifier *writeNotifier = nullptr;
    QSocketNotifier *exceptionNotifier = nullptr;

    bool readPortNotifierCalled = false;
    bool readPortNotifierState = false;
    bool readPortNotifierStateSet = false;

    bool emittedBytesWritten = false;

    qint64 pendingBytesWritten = 0;
    bool writeSequenceStarted = false;

    bool gotException = false;

    std::string name {""};
    int charfd {-1};
    int datafd {-1};
    struct rpmsg_endpoint_info ept;
    std::function<void(char*, int)> callback;
};

QT_END_NAMESPACE
#endif
