#include "rpmsgreader.h"
#include <QCoreApplication>

RPMsgReader::RPMsgReader(QRPMsg *rpMsg, QObject *parent)
    :
    QObject(parent),
    m_rpMsg(rpMsg),
    m_standardOutput(stdout)
{
    connect(m_rpMsg, &QRPMsg::readyRead, this, &RPMsgReader::handleReadyRead);
    connect(m_rpMsg, &QRPMsg::errorOccurred, this, &RPMsgReader::handleError);
    connect(&m_timer, &QTimer::timeout, this, &RPMsgReader::handleTimeout);

    m_timer.start(5000);
}

void RPMsgReader::handleReadyRead()
{
    m_readData.append(m_rpMsg->readAll());
    if (!m_timer.isActive())
        m_timer.start(5000);
}

void RPMsgReader::handleTimeout()
{
    if (m_readData.isEmpty()) {
        m_standardOutput << QObject::tr("No data was currently available "
                                        "for reading from channel %1")
                                .arg(m_rpMsg->channelName())
                         << "\n";
    } else {
        m_standardOutput << QObject::tr("Data successfully received from channel %1")
        .arg(m_rpMsg->channelName())
            << "\n";
        m_standardOutput << m_readData << "\n";
    }

    QCoreApplication::quit();
}

void RPMsgReader::handleError(QRPMsg::RPMsgError rpMsgError)
{
    if (rpMsgError == QRPMsg::ReadError) {
        m_standardOutput << QObject::tr("An I/O error occurred while reading "
                                        "the data from channel %1, error: %2")
                                .arg(m_rpMsg->channelName(), m_rpMsg->errorString())
                         << "\n";
        QCoreApplication::exit(1);
    }
}
