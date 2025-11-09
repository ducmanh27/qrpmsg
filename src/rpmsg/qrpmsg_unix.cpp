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

#if defined QRPMSG_DEBUG
#include <QDebug>
#endif

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
#if defined QRPMSG_DEBUG
    qDebug() << "Read from endpoint " << readBytes << " bytes";
#endif
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
    if (name.empty()) {
        return false;
    }

    char rpmsg_dev[256];
    char rpmsg_ctrl_dev_name[NAME_MAX] = "virtio0.rpmsg_ctrl.0.0";
    char rpmsg_char_name[64];
    char ept_dev_name[64];
    char ept_dev_path[256];
    int ret;
    char fpath[2*NAME_MAX];
    if (name == "")
    {
        return false;
    }

    strncpy(eptinfo.name, name.c_str(), sizeof(eptinfo.name) - 1);

    eptinfo.name[sizeof(eptinfo.name) - 1] = '\0';

    eptinfo.src = RPMSG_ADDR_ANY;
    eptinfo.dst = 0;
#if defined QRPMSG_DEBUG
    qDebug() << "ept.name:" << eptinfo.name;
    qDebug() << "ept.src:" << eptinfo.src;
    qDebug() << "ept.dst:" << eptinfo.dst;
#endif
    // Lookup channel
    snprintf(rpmsg_dev, sizeof(rpmsg_dev), "virtio0.%s.-1.0", name.c_str());
    ret = RPMsgLinuxHelper::lookup_channel(rpmsg_dev, &eptinfo);
    if (ret < 0) {
#if defined QRPMSG_DEBUG
        qDebug("Failed to lookup rpmsg_dev %s\n",rpmsg_dev);
#endif
        return false;
    }

    sprintf(fpath, RPMSG_BUS_SYS "/devices/%s", rpmsg_dev);
    if (access(fpath, F_OK)) {
        fprintf(stderr, "access(%s): %s\n", fpath, strerror(errno));
        return false;
    }

    // Bind chrdev
    ret = RPMsgLinuxHelper::bind_rpmsg_chrdev(rpmsg_dev);
    if (ret < 0) {
#if defined QRPMSG_DEBUG
        qDebug("Failed to bind chrdev for %s\n", name.c_str());
#endif
        return false;
    }

    // Get control device fd
        /* kernel >= 6.0 has new path for rpmsg_ctrl device */
    charfd = RPMsgLinuxHelper::get_rpmsg_chrdev_fd(rpmsg_ctrl_dev_name,
                                                   rpmsg_char_name);
    if (charfd < 0) {
        charfd = RPMsgLinuxHelper::get_rpmsg_chrdev_fd(rpmsg_dev,
                                                       rpmsg_char_name);
        /* may be kernel is < 6.0 try previous path */
        if (charfd < 0) {
#if defined QRPMSG_DEBUG
            qDebug("Failed to get chrdev fd for %s\n", name.c_str());
#endif
            return false;
        }
    }

    // Create endpoint from rpmsg char driver
    // TODO: [manhpd9] Create multiple endpoint in a channel
    ret = RPMsgLinuxHelper::app_rpmsg_create_ept(charfd, &eptinfo);
    if (ret) {
#if defined QRPMSG_DEBUG
            qDebug("Failed to create endpoint for %s\n", name.c_str());
#endif
        qt_safe_close(charfd);
        return false;
    }

    // Get endpoint device name
#if defined QRPMSG_DEBUG
    qDebug() <<"eptinfo.name: " << eptinfo.name;
#endif

    if (!RPMsgLinuxHelper::get_rpmsg_ept_dev_name(rpmsg_char_name,
                                                  eptinfo.name,
                                                  ept_dev_name)) {

#if defined QRPMSG_DEBUG
        qDebug("Failed to get ept dev name for %s\n", name.c_str());
#endif
        qt_safe_close(charfd);
        return false;
    }

    // Open endpoint device
    snprintf(ept_dev_path, sizeof(ept_dev_path), "/dev/%s", ept_dev_name);
    int flags =  O_NONBLOCK;
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

    descriptor = qt_safe_open(ept_dev_path, flags); // datafd
    if (descriptor < 0) {
        setError(getSystemError());
#if defined QRPMSG_DEBUG
        qDebug("Failed to open %s", ept_dev_path);
        qDebug() << "Error string: " <<getSystemError().errorString;
#endif
        qt_safe_close(charfd);
        return false;
    }
#if defined QRPMSG_DEBUG
    qDebug("Channel '%s' initialized (descriptor = %d)\n",
           name.c_str(), descriptor);
#endif
    if (!initialize(mode)) {
        qt_safe_close(descriptor);
        qt_safe_close(charfd);
        return false;
    }
    // TODO: lock file
    // lockFileScopedPointer = std::move(newLockFileScopedPointer);


    /* NOTE: Driver rpmsg của Linux KHÔNG tự động gửi NS announcement sau khi probe
    Do đó user code cần phải gửi một bản tin lần đầu để cho phía lib OpenAMP R5 Side có thể nhận được
    và gọi callback rpmsg_virtio_ns_callback để set dst.
    Nếu không thì khi gọi rpmsg_send() nó sẽ check dst = 0XFFFF'FFFF và trả về ngay mã lỗi -2003. */
    qt_safe_write(descriptor, "A", 1);
    return true;
}

void QRPMsgPrivate::close()
{
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

    if (charfd >= 0){
        ioctl(charfd, RPMSG_DESTROY_EPT_IOCTL, &eptinfo);
        qt_safe_close(charfd);
    }
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

qint64 QRPMsgPrivate::writeData(const char *data, qint64 maxSize)
{
    /*
    NOTE: trong linux source:  /drivers/rpmsg/rpmsg_char.c hàm __poll_t rpmsg_eptdev_poll(struct file *filp, poll_table *wait)
    không hỗ trợ EPOLLOUT mà chỉ hỗ trợ EPOLLIN mặc định.
    Nếu muốn write async thì override lại  hàm poll của struct rpmsg_endpoint_ops hỗ trợ cho EPOLLOUT
        struct rpmsg_endpoint_ops {
            void (*destroy_ept)(struct rpmsg_endpoint *ept);

            int (*send)(struct rpmsg_endpoint *ept, void *data, int len);
            int (*sendto)(struct rpmsg_endpoint *ept, void *data, int len, u32 dst);

            int (*trysend)(struct rpmsg_endpoint *ept, void *data, int len);
            int (*trysendto)(struct rpmsg_endpoint *ept, void *data, int len, u32 dst);
            __poll_t (*poll)(struct rpmsg_endpoint *ept, struct file *filp,
                             poll_table *wait);
            int (*set_flow_control)(struct rpmsg_endpoint *ept, bool pause, u32 dst);
            ssize_t (*get_mtu)(struct rpmsg_endpoint *ept);
        };
    WriterNotifier không được QEventLoop gọi vì k có thông báo event ready to write do k có EPOLLOUT để monitor fd
    */


    // qint64 toAppend = maxSize;

    // if (writeBufferMaxSize && (writeBuffer.size() + toAppend > writeBufferMaxSize))
    //     toAppend = writeBufferMaxSize - writeBuffer.size();

    // writeBuffer.append(data, toAppend);
    // if (!writeBuffer.isEmpty() && !isWriteNotificationEnabled()){
    //     setWriteNotificationEnabled(true);
    // }

    // return toAppend;



    // Write blocking
    const qint64 chunkSize = writeBufferChunkSize;   // tối đa 512 bytes mỗi lần ghi
    qint64 totalWritten = 0;

    while (totalWritten < maxSize)
    {
        qint64 toWrite = std::min(chunkSize, maxSize - totalWritten);

        qint64 written = qt_safe_write(descriptor, data + totalWritten, toWrite);
        if (written < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                // Write bị ngắt tạm thời, retry
                continue;
            }
#if defined QRPMSG_DEBUG
            qDebug() << "Write error:" << strerror(errno);
#endif
            return -1;
        }

        if (written == 0) {
            // Không ghi được nữa, blocking nhưng buffer đầy
#if defined QRPMSG_DEBUG
            qDebug() << "Write done";
#endif
            break;
        }

        totalWritten += written;
    }

    return totalWritten;  // trả về số byte đã ghi
}

bool QRPMsgPrivate::initialize(QIODevice::OpenMode mode)
{
    if (mode & QIODevice::ReadOnly)
        setReadNotificationEnabled(true);

    exceptionNotifier = new ExceptionNotifier(this, q_func());
    gotException = false;

    return true;
}
