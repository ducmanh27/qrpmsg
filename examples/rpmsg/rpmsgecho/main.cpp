#include <QCoreApplication>
#include <QRPMsg>
#include "rpmsgreader.h"
#include <QDebug>
#include <QVector>
#define ECHO_NUM_EPTS 5
int main(int argc, char* argv[])
{
    QCoreApplication coreApplication(argc, argv);


    QRPMsg rpMsgs[ECHO_NUM_EPTS];
    std::vector<std::unique_ptr<RPMsgReader>> rpMsgReaders;
    for (int i = 0; i < ECHO_NUM_EPTS; i++)
    {
        QString channelName {"rpmsg-openamp-demo-channel"};
        if (i != 0)
        {
            channelName += QString::number(i);
        }
        rpMsgs[i].setChannelName(channelName);

        if (!rpMsgs[i].open(QIODevice::ReadWrite))
        {
            return 1;
        }
        rpMsgReaders.push_back(
            std::make_unique<RPMsgReader>(&rpMsgs[i])
        );
    }

    return coreApplication.exec();
}
