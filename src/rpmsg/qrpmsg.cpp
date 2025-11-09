// Copyright (C) 2025 Phan Duc Manh <manhpd9@viettel.com.vn>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qrpmsg.h"
#include "qrpmsginfo.h"
#include "qrpmsginfo_p.h"

#include "qrpmsg_p.h"

#include <QtCore/qdebug.h>
QT_BEGIN_NAMESPACE

QRPMsgErrorInfo::QRPMsgErrorInfo(QRPMsg::RPMsgError newErrorCode, const QString &newErrorString)
:     errorCode(newErrorCode)
    , errorString(newErrorString)
{
    if (errorString.isNull()) {
        switch (errorCode) {
        case QRPMsg::NoError:
            errorString = QRPMsg::tr("No error");
            break;
        case QRPMsg::OpenError:
            errorString = QRPMsg::tr("Device is already open");
            break;
        case QRPMsg::NotOpenError:
            errorString = QRPMsg::tr("Device is not open");
            break;
        case QRPMsg::TimeoutError:
            errorString = QRPMsg::tr("Operation timed out");
            break;
        case QRPMsg::ReadError:
            errorString = QRPMsg::tr("Error reading from device");
            break;
        case QRPMsg::WriteError:
            errorString = QRPMsg::tr("Error writing to device");
            break;
        case QRPMsg::ResourceError:
            errorString = QRPMsg::tr("Device disappeared from the system");
            break;
        default:
            // an empty string will be interpreted as "Unknown error"
            // from the QIODevice::errorString()
            break;
        }
    }
}

QRPMsgPrivate::QRPMsgPrivate()
{
    writeBufferChunkSize = QRPMSG_BUFFERSIZE;
    readBufferChunkSize = QRPMSG_BUFFERSIZE;
}

void QRPMsgPrivate::setError(const QRPMsgErrorInfo &errorInfo)
{
    Q_Q(QRPMsg);

    error = errorInfo.errorCode;
    q->setErrorString(errorInfo.errorString);
    emit q->errorOccurred(error);
}

QRPMsg::QRPMsg(QObject *parent)
    : QIODevice(*new QRPMsgPrivate, parent),
    d_dummy(0)
{

}

QRPMsg::QRPMsg(const QString &name, QObject *parent)
    : QIODevice(*new QRPMsgPrivate, parent),
    d_dummy(0)
{
    setChannelName(name);
}

QRPMsg::QRPMsg(const QRPMsgInfo &info, QObject *parent)
    : QIODevice(*new QRPMsgPrivate, parent),
    d_dummy(0)
{
    Q_UNUSED(info)
    // TODO: set channel info
}

QRPMsg::~QRPMsg()
{
    if (isOpen())
        close();
}

void QRPMsg::setChannelName(const QString &name)
{
    Q_D(QRPMsg);
    // d->systemLocation = QRPMsgInfoPrivate::channelNameToSystemLocation(name);
    d->name = name.toStdString();
}

QString QRPMsg::channelName() const
{
    Q_D(const QRPMsg);
    return QString::fromStdString(d->name);
}


bool QRPMsg::open(OpenMode mode)
{
    Q_D(QRPMsg);

    if (isOpen()) {
        d->setError(QRPMsgErrorInfo(QRPMsg::OpenError));
        return false;
    }

    // Define while not supported modes.
    static const OpenMode unsupportedModes = Append | Truncate | Text | Unbuffered;
    if ((mode & unsupportedModes) || mode == NotOpen) {
        d->setError(QRPMsgErrorInfo(QRPMsg::UnsupportedOperationError, tr("Unsupported open mode")));
        return false;
    }

    if (!d->open(mode))
        return false;

    QIODevice::open(mode);

    return true;
}

void QRPMsg::close()
{
    Q_D(QRPMsg);
    if (!isOpen()) {
        d->setError(QRPMsgErrorInfo(QRPMsg::NotOpenError));
        return;
    }

    d->close();
    QIODevice::close();
}

void QRPMsg::clearError()
{
    Q_D(QRPMsg);
    d->setError(QRPMsgErrorInfo(QRPMsg::NoError));
}

qint64 QRPMsg::readBufferSize() const
{
    Q_D(const QRPMsg);
    return d->readBufferMaxSize;
}

void QRPMsg::setReadBufferSize(qint64 size)
{
    Q_D(QRPMsg);
    d->readBufferMaxSize = size;
    if (isReadable())
        d->startAsyncRead();
}

qint64 QRPMsg::bytesAvailable() const
{
    return QIODevice::bytesAvailable();
}

qint64 QRPMsg::bytesToWrite() const
{
    qint64 pendingBytes = QIODevice::bytesToWrite();
    return pendingBytes;
}

qint64 QRPMsg::readData(char *data, qint64 maxlen)
{
    Q_UNUSED(data);
    Q_UNUSED(maxlen);

    // In any case we need to start the notifications if they were
    // disabled by the read handler. If enabled, next call does nothing.
    d_func()->startAsyncRead();

    // return 0 indicating there may be more data in the future
    return qint64(0);
}

qint64 QRPMsg::writeData(const char *data, qint64 len)
{
    Q_D(QRPMsg);
    return d->writeData(data, len);
}

QT_END_NAMESPACE

#include "moc_qrpmsg.cpp"


