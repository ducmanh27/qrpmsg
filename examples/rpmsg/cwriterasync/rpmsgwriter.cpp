#include "rpmsgwriter.h"
#include <QCoreApplication>

RPMsgWriter::RPMsgWriter(QRPMsg *rpMsg, QObject *parent)
    :
    QObject(parent),
    m_rpMsg(rpMsg),
    m_standardOutput(stdout)
{
    m_timer.setSingleShot(true);
    connect(m_rpMsg, &QRPMsg::bytesWritten,
            this, &RPMsgWriter::handleBytesWritten);
    connect(m_rpMsg, &QRPMsg::errorOccurred,
            this, &RPMsgWriter::handleError);
    connect(&m_timer, &QTimer::timeout, this, &RPMsgWriter::handleTimeout);
}

void RPMsgWriter::write(const QByteArray &writeData)
{
    m_writeData = writeData;

    const qint64 bytesWritten = m_rpMsg->write(writeData);

    if (bytesWritten == -1) {
        m_standardOutput << QObject::tr("Failed to write the data to channel %1, error: %2")
        .arg(m_rpMsg->channelName())
                .arg(m_rpMsg->errorString())
            << "\n";
        QCoreApplication::exit(1);
    } else if (bytesWritten != m_writeData.size()) {
        m_standardOutput << QObject::tr("Failed to write all the data to channel %1, error: %2")
        .arg(m_rpMsg->channelName())
                .arg(m_rpMsg->errorString())
            << "\n";
        QCoreApplication::exit(1);
    }

    m_timer.start(5000);
}

void RPMsgWriter::handleBytesWritten(qint64 bytes)
{
    m_bytesWritten += bytes;
    if (m_bytesWritten == m_writeData.size()) {
        m_bytesWritten = 0;
        m_standardOutput << QObject::tr("Data successfully sent to channel %1")
                                .arg(m_rpMsg->channelName()) << "\n";
        QCoreApplication::quit();
    }
}

void RPMsgWriter::handleTimeout()
{
    m_standardOutput << QObject::tr("Operation timed out for port %1, error: %2")
    .arg(m_rpMsg->channelName())
            .arg(m_rpMsg->errorString())
        << "\n";
    QCoreApplication::exit(1);
}

void RPMsgWriter::handleError(QRPMsg::RPMsgError rpMsgError)
{
    if (rpMsgError == QRPMsg::WriteError) {
        m_standardOutput << QObject::tr("An I/O error occurred while writing"
                                        " the data to port %1, error: %2")
                                .arg(m_rpMsg->channelName())
                                .arg(m_rpMsg->errorString())
                         << "\n";
        QCoreApplication::exit(1);
    }
}
