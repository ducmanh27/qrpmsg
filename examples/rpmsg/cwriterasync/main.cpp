#include <QCoreApplication>
#include <QFile>
#include <QStringList>
#include <QTextStream>
#include <QDebug>

#include "rpmsgwriter.h"

int main(int argc, char *argv[])
{
    QCoreApplication coreApplication(argc, argv);
    const int argumentCount = QCoreApplication::arguments().size();
    const QStringList argumentList = QCoreApplication::arguments();

    QTextStream standardOutput(stdout);
    QString channelName {""};
    if (argumentCount == 1) {
        standardOutput << QObject::tr("Usage: %1 <channelname>")
        .arg(argumentList.first())
            << "\n";
        channelName = "rpmsg-openamp-demo-channel";
        qDebug() << "Using default channel name: " << channelName;

    }
    else
    {
       channelName = argumentList.at(1);
    }
    QRPMsg rpMsg;

    rpMsg.setChannelName(channelName);


    rpMsg.open(QIODevice::WriteOnly);

    QFile dataFile;
    if (!dataFile.open(stdin, QIODevice::ReadOnly)) {
        standardOutput << QObject::tr("Failed to open stdin for reading") << "\n";
        return 1;
    }

    const QByteArray writeData(dataFile.readAll());
    dataFile.close();

    if (writeData.isEmpty()) {
        standardOutput << QObject::tr("Either no data was currently available on "
                                      "the standard input for reading, "
                                      "or an error occurred for port %1, error: %2")
                              .arg(channelName).arg(rpMsg.errorString())
                       << "\n";
        return 1;
    }

    RPMsgWriter rpMsgWriter(&rpMsg);
    rpMsgWriter.write(writeData);

    return coreApplication.exec();

}
