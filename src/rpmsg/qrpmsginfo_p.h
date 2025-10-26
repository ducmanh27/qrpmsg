#ifndef QRPMSGINFO_P_H
#define QRPMSGINFO_P_H
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

#include <QtCore/qstring.h>
#include <QtCore/private/qglobal_p.h>

QT_BEGIN_NAMESPACE
class Q_AUTOTEST_EXPORT QRPMsgInfoPrivate
{
public:
    static QString channelNameToSystemLocation(const QString &source);
    static QString channelNameFromSystemLocation(const QString &source);

    QString channelName;
    QString device;
    QString description;
    QString manufacturer;
    QString serialNumber;

    quint16 vendorIdentifier = 0;
    quint16 productIdentifier = 0;

    bool hasVendorIdentifier = false;
    bool hasProductIdentifier = false;
};
class QRPMsgInfoPrivateDeleter
{
public:
    static void cleanup(QRPMsgInfoPrivate *p) {
        delete p;
    }
};
QT_END_NAMESPACE
#endif // QRPMSGINFO_P_H
