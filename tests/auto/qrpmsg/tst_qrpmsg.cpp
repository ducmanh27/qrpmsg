// tst_qrpmsg.cpp
// Build: qmake && make
// Run:   ./tst_qrpmsg
// Yêu cầu: firmware echo đang chạy trên R5

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QElapsedTimer>
#include "QRPMsg"

Q_DECLARE_METATYPE(QRPMsg::RPMsgError)

static const QString CH0 = QStringLiteral("rpmsg-openamp-demo-channel");
static const QString CH1 = QStringLiteral("rpmsg-openamp-demo-channel1");

class TstQRPMsg : public QObject
{
    Q_OBJECT

private slots:

    void initTestCase()
    {
        qRegisterMetaType<QRPMsg::RPMsgError>("QRPMsg::RPMsgError");
    }

    // Sau mỗi test: cho event loop xử lý hết event còn tồn đọng
    // rồi mới để test tiếp theo chạy — tránh data/lock file leak giữa các test
    void cleanup()
    {
        QTest::qWait(200);
    }

    // ── 1. Open / Close ──────────────────────────────────────────────────────

    void test_open_emptyName()
    {
        QRPMsg dev;
        QVERIFY(!dev.open(QIODevice::ReadWrite));
        QCOMPARE(dev.error(), QRPMsg::ChannelNameEmptyError);
    }

    void test_open_invalidName()
    {
        QRPMsg dev(QStringLiteral("channel-not-exists"));
        QVERIFY(!dev.open(QIODevice::ReadWrite));
        QVERIFY(dev.error() != QRPMsg::NoError);
    }

    void test_open_success()
    {
        QRPMsg dev(CH0);
        QVERIFY2(dev.open(QIODevice::ReadWrite), qPrintable(dev.errorString()));
        QVERIFY(dev.isOpen());
        QCOMPARE(dev.error(), QRPMsg::NoError);
        dev.close();
        QVERIFY(!dev.isOpen());
    }

    void test_open_twice_returns_error()
    {
        QRPMsg dev(CH0);
        QVERIFY(dev.open(QIODevice::ReadWrite));
        QVERIFY(!dev.open(QIODevice::ReadWrite));
        QCOMPARE(dev.error(), QRPMsg::OpenError);
        dev.close();
    }

    void test_close_when_not_open()
    {
        QRPMsg dev(CH0);
        dev.close();
        QCOMPARE(dev.error(), QRPMsg::NotOpenError);
    }

    void test_reopen_after_close()
    {
        QRPMsg dev(CH0);
        QVERIFY(dev.open(QIODevice::ReadWrite));
        dev.close();
        QVERIFY2(dev.open(QIODevice::ReadWrite), qPrintable(dev.errorString()));
        dev.close();
    }

    // ── 2. Write / Echo ──────────────────────────────────────────────────────

    void test_write_and_echo()
    {
        QRPMsg dev(CH0);
        QVERIFY(dev.open(QIODevice::ReadWrite));

        QSignalSpy spy(&dev, &QRPMsg::readyRead);
        const QByteArray msg = "hello rpmsg";
        QCOMPARE(dev.write(msg), (qint64)msg.size());

        QVERIFY2(spy.wait(2000), "Timeout chờ echo");
        QCOMPARE(dev.readAll(), msg);

        dev.close();
    }

    void test_write_empty_data()
    {
        QRPMsg dev(CH0);
        QVERIFY(dev.open(QIODevice::ReadWrite));
        QCOMPARE(dev.write(QByteArray()), (qint64)0);
        dev.close();
    }

    void test_write_large_payload()
    {
        // > 496 bytes (MTU 512 - 16 header) → writeData tự chunk
        QRPMsg dev(CH0);
        QVERIFY(dev.open(QIODevice::ReadWrite));

        QSignalSpy spy(&dev, &QRPMsg::readyRead);
        const QByteArray msg(600, 'X');
        QCOMPARE(dev.write(msg), (qint64)msg.size());

        QElapsedTimer timer;
        timer.start();
        while (dev.bytesAvailable() < msg.size() && timer.elapsed() < 3000)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }

        qDebug() << "received:" << dev.bytesAvailable();
        QVERIFY2(dev.bytesAvailable() >= msg.size(), "Timeout wait for 600 bytes");
        QCOMPARE(dev.readAll(), msg);

        dev.close();
    }

    void test_write_sequential_messages()
    {
        QRPMsg dev(CH0);
        QVERIFY(dev.open(QIODevice::ReadWrite));

        for (int i = 0; i < 5; ++i)
        {
            QSignalSpy spy(&dev, &QRPMsg::readyRead);
            const QByteArray msg = QByteArrayLiteral("seq:") + QByteArray::number(i);
            QCOMPARE(dev.write(msg), (qint64)msg.size());
            QVERIFY2(spy.wait(2000),
                     qPrintable(QStringLiteral("Timeout ở message %1").arg(i)));
            QCOMPARE(dev.readAll(), msg);
        }

        dev.close();
    }

    // ── 3. Read ──────────────────────────────────────────────────────────────

    void test_readyRead_signal_emitted()
    {
        QRPMsg dev(CH0);
        QVERIFY(dev.open(QIODevice::ReadWrite));

        QSignalSpy spy(&dev, &QRPMsg::readyRead);
        dev.write("ping");
        QVERIFY(spy.wait(2000));
        QVERIFY(spy.count() >= 1);

        dev.close();
    }

    void test_bytesAvailable_after_echo()
    {
        QRPMsg dev(CH0);
        QVERIFY(dev.open(QIODevice::ReadWrite));

        QSignalSpy spy(&dev, &QRPMsg::readyRead);
        dev.write("data");
        QVERIFY(spy.wait(2000));
        QVERIFY(dev.bytesAvailable() > 0);

        dev.close();
    }

    void test_read_partial()
    {
        QRPMsg dev(CH0);
        QVERIFY(dev.open(QIODevice::ReadWrite));

        QSignalSpy spy(&dev, &QRPMsg::readyRead);
        dev.write("ABCDE");
        QVERIFY(spy.wait(2000));

        const QByteArray part1 = dev.read(2);
        QCOMPARE(part1.size(), 2);
        const QByteArray part2 = dev.readAll();
        QCOMPARE(part1 + part2, QByteArray("ABCDE"));

        dev.close();
    }

    // ── 4. Multi-channel ─────────────────────────────────────────────────────

    void test_two_channels_independent()
    {
        QRPMsg dev0(CH0), dev1(CH1);
        QVERIFY(dev0.open(QIODevice::ReadWrite));
        QVERIFY(dev1.open(QIODevice::ReadWrite));

        QSignalSpy spy0(&dev0, &QRPMsg::readyRead);
        QSignalSpy spy1(&dev1, &QRPMsg::readyRead);

        dev0.write("from-ch0");
        dev1.write("from-ch1");

        QVERIFY(spy0.wait(2000));
        QVERIFY(spy1.wait(2000));

        QCOMPARE(dev0.readAll(), QByteArray("from-ch0"));
        QCOMPARE(dev1.readAll(), QByteArray("from-ch1"));

        dev0.close();
        dev1.close();
    }

    void test_five_channels_concurrent()
    {
        const QStringList names =
        {
            QStringLiteral("rpmsg-openamp-demo-channel"),
            QStringLiteral("rpmsg-openamp-demo-channel1"),
            QStringLiteral("rpmsg-openamp-demo-channel2"),
            QStringLiteral("rpmsg-openamp-demo-channel3"),
            QStringLiteral("rpmsg-openamp-demo-channel4"),
        };

        QVector<QRPMsg*> devs;
        QVector<QSignalSpy*> spies;

        for (const QString & name : names)
        {
            auto* d = new QRPMsg(name, this);
            if (!d->open(QIODevice::ReadWrite))
            {
                qDeleteAll(spies);
                qDeleteAll(devs);
                QSKIP(qPrintable(QStringLiteral("Không mở được %1: %2")
                                 .arg(name, d->errorString())));
            }
            spies.append(new QSignalSpy(d, &QRPMsg::readyRead));
            devs.append(d);
        }

        for (int i = 0; i < devs.size(); ++i)
        {
            devs[i]->write(QByteArrayLiteral("ch") + QByteArray::number(i));
        }

        for (int i = 0; i < devs.size(); ++i)
        {
            QVERIFY2(spies[i]->wait(3000),
                     qPrintable(QStringLiteral("Timeout channel %1").arg(i)));
            QCOMPARE(devs[i]->readAll(),
                     QByteArrayLiteral("ch") + QByteArray::number(i));
        }

        qDeleteAll(spies);
        qDeleteAll(devs);
    }

    // ── 5. Error / Signal ────────────────────────────────────────────────────

    void test_errorOccurred_signal_on_bad_open()
    {
        QRPMsg dev(QStringLiteral("channel-not-exists"));
        QSignalSpy spy(&dev, &QRPMsg::errorOccurred);
        dev.open(QIODevice::ReadWrite);
        if (spy.isEmpty())
        {
            spy.wait(500);
        }
        QVERIFY(!spy.isEmpty());
    }

    void test_clearError()
    {
        QRPMsg dev(QStringLiteral("channel-not-exists"));
        dev.open(QIODevice::ReadWrite);
        QVERIFY(dev.error() != QRPMsg::NoError);
        dev.clearError();
        QCOMPARE(dev.error(), QRPMsg::NoError);
    }

    // ── 6. Throughput / stress ───────────────────────────────────────────────

    void test_throughput_100_messages()
    {
        QRPMsg dev(CH0);
        QVERIFY2(dev.open(QIODevice::ReadWrite), qPrintable(dev.errorString()));

        constexpr int N = 100;
        int received = 0;
        QElapsedTimer timer;

        connect(&dev, &QRPMsg::readyRead, this, [&]()
        {
            while (dev.bytesAvailable() > 0)
            {
                dev.readAll();
                ++received;
            }
        });

        timer.start();
        for (int i = 0; i < N; ++i)
        {
            dev.write(QByteArrayLiteral("msg") + QByteArray::number(i));
        }

        QElapsedTimer wait;
        wait.start();
        while (received < N && wait.elapsed() < 10000)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }

        qDebug("Receive %d/%d in %lld ms", received, N, timer.elapsed());
        QCOMPARE(received, N);

        dev.close();
    }
};

QTEST_MAIN(TstQRPMsg)
#include "tst_qrpmsg.moc"
