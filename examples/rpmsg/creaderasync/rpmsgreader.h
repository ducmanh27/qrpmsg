#ifndef RPMSGREADER_H
#define RPMSGREADER_H

#include <QObject>
#include <QByteArray>
#include <QTextStream>
#include <QTimer>
#include <QRPMsg>

QT_BEGIN_NAMESPACE

QT_END_NAMESPACE

class RPMsgReader : public QObject
{
    Q_OBJECT
public:
    explicit RPMsgReader(QRPMsg *rpMsg,QObject *parent = nullptr);

private slots:
    void handleReadyRead();
    void handleTimeout();
    void handleError(QRPMsg::RPMsgError rpMsgError);

private:
    QRPMsg *m_rpMsg = nullptr;
    QByteArray m_readData;
    QTextStream m_standardOutput;
    QTimer m_timer;
};

#endif // RPMSGREADER_H
