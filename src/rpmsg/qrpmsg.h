// Copyright (C) 2025 Phan Duc Manh <manhpd9@viettel.com.vn>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QRPMSG_H
#define QRPMSG_H
#include <QtCore/qiodevice.h>

#include <QtRPMsg/qrpmsgglobal.h>

QT_BEGIN_NAMESPACE
class QRPMsgInfo;
class QRPMsgPrivate;
class Q_RPMSG_EXPORT QRPMsg : public QIODevice
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QRPMsg)

    typedef int Handle;

public:

    enum Direction  {
        Input = 1,
        Output = 2,
        AllDirections = Input | Output
    };
    Q_FLAG(Direction)
    Q_DECLARE_FLAGS(Directions, Direction)

    enum RPMsgError {
        NoError,
        DeviceNotFoundError,
        PermissionError,
        OpenError,
        WriteError,
        ReadError,
        ResourceError,
        UnsupportedOperationError,
        UnknownError,
        TimeoutError,
        NotOpenError
    };
    Q_ENUM(RPMsgError)

    explicit QRPMsg(QObject *parent = nullptr);
    explicit QRPMsg(const QString &name, QObject *parent = nullptr);
    explicit QRPMsg(const QRPMsgInfo &info, QObject *parent = nullptr);
    virtual ~QRPMsg();

    void setChannelName(const QString &name);
    QString channelName() const;

    bool open(OpenMode mode) override;
    void close() override;

    RPMsgError error() const;
    void clearError();

    qint64 readBufferSize() const;
    void setReadBufferSize(qint64 size);

    qint64 writeBufferSize() const;
    void setWriteBufferSize(qint64 size);

    qint64 bytesAvailable() const override;
    qint64 bytesToWrite() const override;

Q_SIGNALS:
    void requestToSendChanged(bool set);
    void errorOccurred(QRPMsg::RPMsgError error);

    // QIODevice interface
protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

private:
    QRPMsgPrivate * const d_dummy;
    Q_DISABLE_COPY(QRPMsg)


};

QT_END_NAMESPACE
#endif
