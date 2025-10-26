#ifndef QSERIALPORTINFO_H
#define QSERIALPORTINFO_H
#include <QtCore/qlist.h>
#include <QtCore/qscopedpointer.h>

#include <QtRPMsg/qrpmsgglobal.h>
QT_BEGIN_NAMESPACE
class QRPMsg;
class QRPMsgInfoPrivate;
class Q_RPMSG_EXPORT QRPMsgInfo
{
    Q_DECLARE_PRIVATE(QRPMsgInfo)
public:
    QRPMsgInfo();
    explicit QRPMsgInfo(const QRPMsg &rpmsg);
    explicit QRPMsgInfo(const QString &name);
    QRPMsgInfo(const QRPMsgInfo &other);
    ~QRPMsgInfo();
private:
    QRPMsgInfo(const QRPMsgInfoPrivate &dd);
    // friend QList<QRPMsgInfo> availablePortsByUdev(bool &ok);
    // friend QList<QRPMsgInfo> availablePortsBySysfs(bool &ok);
    // friend QList<QRPMsgInfo> availablePortsByFiltersOfDevices(bool &ok);
    std::unique_ptr<QRPMsgInfoPrivate> d_ptr;
};

QT_END_NAMESPACE

#endif // QSERIALPORTINFO_H
