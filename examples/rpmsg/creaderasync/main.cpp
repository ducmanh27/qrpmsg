#include <QCoreApplication>
#include <QRPMsg>
#include <QStringList>
#include <QTextStream>
#include "rpmsgreader.h"
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication coreApplication(argc, argv);
    const int argumentCount = QCoreApplication::arguments().size();
    const QStringList argumentList = QCoreApplication::arguments();

    QTextStream standardOutput(stdout);
    QString channelName {""};
    if (argumentCount == 1) {
        standardOutput << QObject::tr("Usage: %1 <rpmsgchannelname>")
                          .arg(argumentList.first())
                       << "\n";
        // FIXME: tách channel name riêng, virtIO và src/des riêng
        channelName = "rpmsg-openamp-demo-channel"; // virtualIO - channelName - src.des
        qDebug() << "Using default channel name: " << channelName;
    }
    else
    {
        channelName = argumentList.at(1);
    }

    QRPMsg rpMsg;

    rpMsg.setChannelName(channelName);
    // TODO: implement
    // rpMsg.setVirtIO(0);
    // rpMsg.setDst(0);
    // rpMsg.setSrc(1024);

    if (!rpMsg.open(QIODevice::ReadOnly)) {
        standardOutput << QObject::tr("Failed to open channel %1, error: %2")
                          .arg(channelName)
                          .arg(rpMsg.errorString())
                       << "\n";
        return 1;
    }

    RPMsgReader rpMsgReader(&rpMsg);

    return coreApplication.exec();
}
