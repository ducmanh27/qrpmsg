#include "qrpmsg_p.h"
#include "qrpmsginfo_p.h"
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

//
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/rpmsg.h>
#include <sys/epoll.h>
#include <string>
#include "rpmsglinuxhelper.h"


QString rpMsgLockFilePath(const QString &channelName)
{
    static const QStringList lockDirectoryPaths = QStringList()
    << QStringLiteral("/var/lock")
    << QStringLiteral("/etc/locks")
    << QStringLiteral("/var/spool/locks")
    << QStringLiteral("/var/spool/uucp")
    << QStringLiteral("/tmp")
    << QStringLiteral("/var/tmp")
    << QStringLiteral("/var/lock/lockdev")
    << QStringLiteral("/run/lock")
    << QStandardPaths::writableLocation(QStandardPaths::TempLocation);

    QString fileName = channelName;
    fileName.replace(QLatin1Char('/'), QLatin1Char('_'));
    fileName.prepend(QLatin1String("/LCK.."));

    QString lockFilePath;

    for (const QString &lockDirectoryPath : lockDirectoryPaths) {
        const QString filePath = lockDirectoryPath + fileName;

        QFileInfo lockDirectoryInfo(lockDirectoryPath);
        if (lockDirectoryInfo.isReadable()) {
            if (QFile::exists(filePath) || lockDirectoryInfo.isWritable()) {
                lockFilePath = filePath;
                break;
            }
        }
    }

    if (lockFilePath.isEmpty()) {
        qWarning("The following directories are not readable or writable for detaling with lock files\n");
        for (const QString &lockDirectoryPath : lockDirectoryPaths)
            qWarning("\t%s\n", qPrintable(lockDirectoryPath));
        return QString();
    }

    return lockFilePath;
}

class ReadNotifier : public QSocketNotifier
{
public:
    explicit ReadNotifier(QRPMsgPrivate *d, QObject *parent)
        : QSocketNotifier(d->descriptor, QSocketNotifier::Read, parent)
        , dptr(d)
    {
    }

protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::SockAct) {
            dptr->readNotification();
            return true;
        }
        return QSocketNotifier::event(e);
    }

private:
    QRPMsgPrivate * const dptr;
};

class WriteNotifier : public QSocketNotifier
{
public:
    explicit WriteNotifier(QRPMsgPrivate *d, QObject *parent)
        : QSocketNotifier(d->descriptor, QSocketNotifier::Write, parent)
        , dptr(d)
    {
    }

protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::SockAct) {
            dptr->completeAsyncWrite();
            return true;
        }
        return QSocketNotifier::event(e);
    }

private:
    QRPMsgPrivate * const dptr;
};

class ExceptionNotifier : public QSocketNotifier
{
public:
    explicit ExceptionNotifier(QRPMsgPrivate *d, QObject *parent)
        : QSocketNotifier(d->descriptor, QSocketNotifier::Exception, parent),
        dptr(d)
    {
    }

protected:
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::SockAct) {
            dptr->handleException();
            return true;
        }
        return QSocketNotifier::event(e);
    }

private:
    QRPMsgPrivate * const dptr;
};

bool QRPMsgPrivate::startAsyncRead()
{
    setReadNotificationEnabled(true);
    return true;
}

bool QRPMsgPrivate::isReadNotificationEnabled() const
{
    return readNotifier && readNotifier->isEnabled();
}

void QRPMsgPrivate::setReadNotificationEnabled(bool enable)
{
    Q_Q(QRPMsg);

    if (readNotifier) {
        readNotifier->setEnabled(enable);
    } else if (enable) {
        readNotifier = new ReadNotifier(this, q);
        readNotifier->setEnabled(true);
    }
}

bool QRPMsgPrivate::isWriteNotificationEnabled() const
{
    return writeNotifier && writeNotifier->isEnabled();
}

void QRPMsgPrivate::setWriteNotificationEnabled(bool enable)
{
    Q_Q(QRPMsg);

    if (writeNotifier) {
        writeNotifier->setEnabled(enable);
    } else if (enable) {
        writeNotifier = new WriteNotifier(this, q);
        writeNotifier->setEnabled(true);
    }
}

qint64 QRPMsgPrivate::readFromEndpoint(char *data, qint64 maxSize)
{
    return qt_safe_read(descriptor, data, maxSize);
}

qint64 QRPMsgPrivate::writeToEndpoint(const char *data, qint64 maxSize)
{
    qint64 bytesWritten = 0;
    bytesWritten = qt_safe_write(descriptor, data, maxSize);
    return bytesWritten;
}

bool QRPMsgPrivate::readNotification()
{
    Q_Q(QRPMsg);

    // Always buffered, read data from the port into the read buffer
    qint64 newBytes = buffer.size();
    qint64 bytesToRead = QRPMSG_BUFFERSIZE;

    if (readBufferMaxSize && bytesToRead > (readBufferMaxSize - buffer.size())) {
        bytesToRead = readBufferMaxSize - buffer.size();
        if (bytesToRead <= 0) {
            // Buffer is full. User must read data from the buffer
            // before we can read more from the port.
            setReadNotificationEnabled(false);
            return false;
        }
    }

    char *ptr = buffer.reserve(bytesToRead);
    const qint64 readBytes = readFromEndpoint(ptr, bytesToRead);

    buffer.chop(bytesToRead - qMax(readBytes, qint64(0)));

    if (readBytes < 0) {
        QRPMsgErrorInfo error = getSystemError();
        if (error.errorCode != QRPMsg::ResourceError)
            error.errorCode = QRPMsg::ReadError;
        else
            setReadNotificationEnabled(false);
        setError(error);
        return false;
    } else if (readBytes == 0) {
        // We can get here at least in two cases:
        // * there is no data in the port
        // * the device was disconnected (unplugged)
        // The first case is perfectly valid, and we should simply keep
        // reading. The second case should be reported as a ResourceError.
        // We use exception notifier to detect this case.
        if (gotException) {
            setReadNotificationEnabled(false);
            // Force a specific error
            QRPMsgErrorInfo error = getSystemError(EIO);
            setError(error);
        }
        return false;
    }

    newBytes = buffer.size() - newBytes;

    // only emit readyRead() when not recursing, and only if there is data available
    const bool hasData = newBytes > 0;

    if (!emittedReadyRead && hasData) {
        emittedReadyRead = true;
        emit q->readyRead();
        emittedReadyRead = false;
    }

    return true;
}

bool QRPMsgPrivate::startAsyncWrite()
{
    if (writeBuffer.isEmpty() || writeSequenceStarted)
        return true;

    // Attempt to write it all in one chunk.
    qint64 written = writeToEndpoint(writeBuffer.readPointer(), writeBuffer.nextDataBlockSize());
    if (written < 0) {
        QRPMsgErrorInfo error = getSystemError();
        if (error.errorCode != QRPMsg::ResourceError)
            error.errorCode = QRPMsg::WriteError;
        setError(error);
        return false;
    }

    writeBuffer.free(written);
    pendingBytesWritten += written;
    writeSequenceStarted = true;

    if (!isWriteNotificationEnabled())
        setWriteNotificationEnabled(true);
    return true;
}

bool QRPMsgPrivate::completeAsyncWrite()
{
    Q_Q(QRPMsg);

    if (pendingBytesWritten > 0) {
        if (!emittedBytesWritten) {
            emittedBytesWritten = true;
            emit q->bytesWritten(pendingBytesWritten);
            pendingBytesWritten = 0;
            emittedBytesWritten = false;
        }
    }

    writeSequenceStarted = false;

    if (writeBuffer.isEmpty()) {
        setWriteNotificationEnabled(false);
        return true;
    }

    return startAsyncWrite();
}

void QRPMsgPrivate::handleException()
{
    gotException = true;
}

QRPMsgPrivate::QRPMsgPrivate()
{

}

bool QRPMsgPrivate::open(QIODevice::OpenMode mode)
{
    // TODO: lock file
    // QString lockFilePath = rpMsgLockFilePath(QRPMsgInfoPrivate::channelNameFromSystemLocation(systemLocation));
    // bool isLockFileEmpty = lockFilePath.isEmpty();
    // if (isLockFileEmpty) {
    //     qWarning("Failed to create a lock file for opening the device");
    //     setError(QRPMsgErrorInfo(QRPMsg::PermissionError, QRPMsg::tr("Permission error while creating lock file")));
    //     return false;
    // }

    // auto newLockFileScopedPointer = std::make_unique<QLockFile>(lockFilePath);

    // if (!newLockFileScopedPointer->tryLock()) {
    //     setError(QSerialPortErrorInfo(QSerialPort::PermissionError, QSerialPort::tr("Permission error while locking the device")));
    //     return false;
    // }
    char rpmsg_dev[256];
    char rpmsg_ctrl_dev[256];
    char rpmsg_char_name[64];
    char ept_dev_name[64];
    char ept_dev_path[256];
    int ret;
    if (name == "")
    {
        return false;
    }
    printf("[manhpd9] Channel name %s \n", name.c_str());
    strncpy(ept.name, name.c_str(), sizeof(ept.name) - 1);
    ept.name[sizeof(ept.name) - 1] = '\0';

    ept.src = 0;
    ept.dst = 0;

    // Lookup channel
    snprintf(rpmsg_dev, sizeof(rpmsg_dev), "virtio0.%s.-1.0", name.c_str());
    ret = RPMsgLinuxHelper::lookup_channel(rpmsg_dev, &ept);
    if (ret < 0) {
        printf("Failed to lookup channel %s\n", name.c_str());
        return false;
    }

    // Bind chrdev
    ret = RPMsgLinuxHelper::bind_rpmsg_chrdev(rpmsg_dev);
    if (ret < 0) {
        printf("Failed to bind chrdev for %s\n", name.c_str());
        return false;
    }

    // Get control device fd
    snprintf(rpmsg_ctrl_dev, sizeof(rpmsg_ctrl_dev), "virtio0.rpmsg_ctrl.0.0");
    charfd = RPMsgLinuxHelper::get_rpmsg_chrdev_fd(rpmsg_ctrl_dev,
                                                   rpmsg_char_name);
    if (charfd < 0) {
        charfd = RPMsgLinuxHelper::get_rpmsg_chrdev_fd(rpmsg_dev,
                                                       rpmsg_char_name);
        if (charfd < 0) {
            printf("Failed to get chrdev fd for %s\n", name.c_str());
            return false;
        }
    }

    // Create endpoint
    // TODO: [manhpd9] Create multiple endpoint in a channel
    ret = RPMsgLinuxHelper::app_rpmsg_create_ept(charfd, &ept);
    if (ret < 0) {
        printf("Failed to create endpoint for %s\n", name.c_str());
        // TODO: remove syscal
        // ::close(charfd);
        qt_safe_close(charfd);
        return false;
    }

    // Get endpoint device name
    if (!RPMsgLinuxHelper::get_rpmsg_ept_dev_name(rpmsg_char_name,
                                                  ept.name,
                                                  ept_dev_name)) {
        printf("Failed to get ept dev name for %s\n", name.c_str());
        // TODO: remove syscal
        // ::close(charfd);
        qt_safe_close(charfd);
        return false;
    }

    // Open endpoint device
    snprintf(ept_dev_path, sizeof(ept_dev_path), "/dev/%s", ept_dev_name);


    int flags = O_NOCTTY | O_NONBLOCK;
    switch (mode & QIODevice::ReadWrite) {
    case QIODevice::WriteOnly:
        flags |= O_WRONLY;
        break;
    case QIODevice::ReadWrite:
        flags |= O_RDWR;
        break;
    default:
        flags |= O_RDONLY;
        break;
    }

    // datafd = ::open(ept_dev_path, flags);
    descriptor = qt_safe_open(ept_dev_path, flags); // datafd
    if (descriptor < 0) {
        printf("Failed to open %s\n", ept_dev_path);
        setError(getSystemError());
        // TODO: remove syscal
        // ::close(charfd);
        qt_safe_close(charfd);
        return false;
    }

    printf("[OK] Channel '%s' initialized (datafd=%d)\n",
           name.c_str(), datafd);

    // descriptor = qt_safe_open(systemLocation.toLocal8Bit().constData(), flags);

    // if (descriptor == -1) {
    //     setError(getSystemError());
    //     return false;
    // }

    if (!initialize(mode)) {
        qt_safe_close(descriptor);
        return false;
    }

    // TODO: lock file
    // lockFileScopedPointer = std::move(newLockFileScopedPointer);

    return true;
}

void QRPMsgPrivate::close()
{
#ifdef TIOCNXCL
    ::ioctl(descriptor, TIOCNXCL);
#endif

    delete readNotifier;
    readNotifier = nullptr;

    delete writeNotifier;
    writeNotifier = nullptr;

    delete exceptionNotifier;
    exceptionNotifier = nullptr;

    qt_safe_close(descriptor);

    // lockFileScopedPointer.reset(nullptr);

    descriptor = -1;
    // pendingBytesWritten = 0;
    // writeSequenceStarted = false;
    gotException = false;

    if (charfd >= 0)
        qt_safe_close(charfd);

    charfd = -1;
}

QRPMsgErrorInfo QRPMsgPrivate::getSystemError(int systemErrorCode) const
{
    if (systemErrorCode == -1)
        systemErrorCode = errno;

    QRPMsgErrorInfo error;
    error.errorString = qt_error_string(systemErrorCode);

    switch (systemErrorCode) {
    case ENODEV:
        error.errorCode = QRPMsg::DeviceNotFoundError;
        break;
#ifdef ENOENT
    case ENOENT:
        error.errorCode = QRPMsg::DeviceNotFoundError;
        break;
#endif
    case EACCES:
        error.errorCode = QRPMsg::PermissionError;
        break;
    case EBUSY:
        error.errorCode = QRPMsg::PermissionError;
        break;
    case EAGAIN:
        error.errorCode = QRPMsg::ResourceError;
        break;
    case EIO:
        error.errorCode = QRPMsg::ResourceError;
        break;
    case EBADF:
        error.errorCode = QRPMsg::ResourceError;
        break;
#ifdef EINVAL
    case EINVAL:
        error.errorCode = QRPMsg::UnsupportedOperationError;
        break;
#endif
#ifdef ENOIOCTLCMD
    case ENOIOCTLCMD:
        error.errorCode = QRPMsg::UnsupportedOperationError;
        break;
#endif
#ifdef ENOTTY
    case ENOTTY:
        error.errorCode = QRPMsg::UnsupportedOperationError;
        break;
#endif
#ifdef EPERM
    case EPERM:
        error.errorCode = QRPMsg::PermissionError;
        break;
#endif
    default:
        error.errorCode = QRPMsg::UnknownError;
        break;
    }
    return error;
}

void QRPMsgPrivate::setError(const QRPMsgErrorInfo &errorInfo)
{
    Q_Q(QRPMsg);

    error = errorInfo.errorCode;
    q->setErrorString(errorInfo.errorString);
    emit q->errorOccurred(error);
}

qint64 QRPMsgPrivate::writeData(const char *data, qint64 maxSize)
{
    qint64 toAppend = maxSize;

    if (writeBufferMaxSize && (writeBuffer.size() + toAppend > writeBufferMaxSize))
        toAppend = writeBufferMaxSize - writeBuffer.size();

    writeBuffer.append(data, toAppend);
    if (!writeBuffer.isEmpty() && !isWriteNotificationEnabled())
        setWriteNotificationEnabled(true);
    return toAppend;
}

bool QRPMsgPrivate::initialize(QIODevice::OpenMode mode)
{
    // Ngăn tiến trình khác mở cùng một datafd cùng lúc
#ifdef TIOCEXCL
    if (::ioctl(descriptor, TIOCEXCL) == -1)
        setError(getSystemError());
#endif
    if (mode & QIODevice::ReadOnly)
        setReadNotificationEnabled(true);

    exceptionNotifier = new ExceptionNotifier(this, q_func());
    gotException = false;

    return true;
}
