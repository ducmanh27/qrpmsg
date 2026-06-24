#include "rpmsgreader.h"
#include <QCoreApplication>
#include <QDebug>
RPMsgReader::RPMsgReader(QRPMsg* rpMsg, QObject* parent)
    :
    QObject(parent),
    mRpMsg(rpMsg),
    mStandardOutput(stdout)
{
    connect(mRpMsg, &QRPMsg::readyRead, this, &RPMsgReader::handleReadyRead);
    connect(mRpMsg, &QRPMsg::errorOccurred, this, &RPMsgReader::handleError);
    connect(&mTimerEchoMessage, &QTimer::timeout, this, &RPMsgReader::handleEchoMessage);
    mTimerEchoMessage.start(1000);
}

void RPMsgReader::handleReadyRead()
{
    qDebug() << "Data receive: " << mRpMsg->readAll();
}

void RPMsgReader::handleEchoMessage()
{
    std::string msg = "hello world with seq:" + std::to_string(mCounter++);
    mRpMsg->write(msg.c_str());
}

void RPMsgReader::handleError(QRPMsg::RPMsgError rpMsgError)
{
    if (rpMsgError == QRPMsg::ReadError)
    {
        mStandardOutput << QObject::tr("An I/O error occurred while reading "
                                        "the data from channel %1, error: %2")
                         .arg(mRpMsg->channelName(), mRpMsg->errorString())
                         << "\n";
        QCoreApplication::exit(1);
    }
}
