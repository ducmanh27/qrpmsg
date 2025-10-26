#ifndef RPMSGWRITER_H
#define RPMSGWRITER_H

#include <QObject>
#include <QByteArray>
#include <QRPMsg>
#include <QTextStream>
#include <QTimer>

QT_BEGIN_NAMESPACE

QT_END_NAMESPACE

class RPMsgWriter : public QObject
{
    Q_OBJECT
public:
    explicit RPMsgWriter(QRPMsg *rpMsg,QObject *parent = nullptr);
    void write(const QByteArray &writeData);

private slots:
    void handleBytesWritten(qint64 bytes);
    void handleTimeout();
    void handleError(QRPMsg::RPMsgError rpMsgError);

private:
    QRPMsg *m_rpMsg = nullptr;
    QByteArray m_writeData;
    QTextStream m_standardOutput;
    qint64 m_bytesWritten = 0;
    QTimer m_timer;
};

#endif // RPMSGWRITER_H
