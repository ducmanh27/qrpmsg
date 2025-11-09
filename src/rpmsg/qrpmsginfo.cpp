#include "qrpmsginfo.h"
#include "qrpmsginfo_p.h"
#include "qrpmsg.h"
#include "qrpmsg_p.h"
QT_BEGIN_NAMESPACE

QRPMsgInfo::QRPMsgInfo()
{

}

QRPMsgInfo::QRPMsgInfo(const QRPMsg &rpmsg)
{
    Q_UNUSED(rpmsg)
}

QRPMsgInfo::QRPMsgInfo(const QString &name)
{
    Q_UNUSED(name)
}

QRPMsgInfo::QRPMsgInfo(const QRPMsgInfo &other)
{
    Q_UNUSED(other)
}

QRPMsgInfo::~QRPMsgInfo()
{

}

QString QRPMsgInfo::channelName() const
{
    Q_D(const QRPMsgInfo);
    return !d ? QString() : d->channelName;
}

QString QRPMsgInfo::description() const
{
    Q_D(const QRPMsgInfo);
    return !d ? QString() : d->description;
}

QT_END_NAMESPACE


