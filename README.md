# QRPMsg

Qt wrapper cho Linux RPMsg character driver, cho phép giao tiếp với firmware chạy trên remote processor (ví dụ: Cortex-R5 trên Xilinx KR260) thông qua Qt event loop với signals/slots quen thuộc — tương tự cách dùng `QSerialPort`.

---

## Yêu cầu

| Thành phần | Phiên bản |
|---|---|
| Qt | 5.15+ |
| Linux kernel | 5.x+ (rpmsg_char driver) |
| Firmware | OpenAMP / FreeRTOS trên remote processor |
| Board đã test | Xilinx KR260 (PetaLinux 2023.2) |

---

## Kiến trúc tổng quan

```
Qt Application
      │  signals/slots (readyRead, bytesWritten, errorOccurred)
      ▼
   QRPMsg  ──────────────────  QRPMsgPrivate
 (QIODevice)                  (descriptor, charfd, buffer)
      │                              │
      │                    ReadNotifier / ExceptionNotifier
      │                    (QSocketNotifier → Qt event loop)
      ▼
Linux rpmsg_char driver
  /dev/rpmsgX   +   /dev/rpmsg_ctrlX
      │
   virtio / vring (shared memory)
      │
Remote Processor Firmware
  OpenAMP + FreeRTOS
```

Khi `open()` được gọi, thư viện thực hiện 5 bước:

1. **lookup_channel** — tìm device trong `/sys/bus/rpmsg/devices`
2. **bind_rpmsg_chrdev** — gắn kernel driver `rpmsg_chrdev` vào device
3. **get_rpmsg_chrdev_fd** — mở `/dev/rpmsg_ctrlX`
4. **app_rpmsg_create_ept** — tạo endpoint qua `ioctl`
5. **open endpoint device** — mở `/dev/rpmsgX` để đọc/ghi

Sau khi mở thành công, thư viện gửi một bản tin NS để phía firmware cập nhật địa chỉ `dst` (do Linux rpmsg driver không tự gửi Name Service announcement sau khi probe).

---

## Cài đặt

```bash
git clone <repo>
cd qrpmsg
mkdir build && cd build
qmake ../qrpmsg.pro
make -j$(nproc)
```

---

## Sử dụng nhanh

### 1. Giao tiếp một chiều (echo)

```cpp
#include <QCoreApplication>
#include <QRPMsg>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QRPMsg dev("rpmsg-openamp-demo-channel");

    if (!dev.open(QIODevice::ReadWrite)) {
        qCritical() << "Không mở được channel:" << dev.errorString();
        return 1;
    }

    // Nhận data từ firmware
    QObject::connect(&dev, &QRPMsg::readyRead, [&]() {
        qDebug() << "Nhận:" << dev.readAll();
    });

    // Gửi data tới firmware
    dev.write("hello firmware");

    return app.exec();
}
```

### 2. Nhiều channel đồng thời

```cpp
QRPMsg ch0("rpmsg-openamp-demo-channel");
QRPMsg ch1("rpmsg-openamp-demo-channel1");

ch0.open(QIODevice::ReadWrite);
ch1.open(QIODevice::ReadWrite);

QObject::connect(&ch0, &QRPMsg::readyRead, [&]() {
    qDebug() << "CH0:" << ch0.readAll();
});
QObject::connect(&ch1, &QRPMsg::readyRead, [&]() {
    qDebug() << "CH1:" << ch1.readAll();
});

ch0.write("msg to channel 0");
ch1.write("msg to channel 1");
```

### 3. Xử lý lỗi

```cpp
QObject::connect(&dev, &QRPMsg::errorOccurred,
                 [&](QRPMsg::RPMsgError err) {
    if (err == QRPMsg::ResourceError) {
        qWarning() << "Firmware bị ngắt kết nối!";
        dev.close();
    }
});
```

---

## API Reference

### Constructors

```cpp
QRPMsg(QObject *parent = nullptr);
QRPMsg(const QString &channelName, QObject *parent = nullptr);
```

### Cấu hình

| Phương thức | Mô tả |
|---|---|
| `setChannelName(QString)` | Đặt tên channel trước khi `open()` |
| `channelName()` | Trả về tên channel hiện tại |
| `setReadBufferSize(qint64)` | Giới hạn kích thước read buffer (0 = unlimited) |

### Vòng đời

| Phương thức | Mô tả |
|---|---|
| `open(OpenMode)` | Mở channel. Hỗ trợ `ReadOnly`, `WriteOnly`, `ReadWrite` |
| `close()` | Đóng channel, giải phóng endpoint và lock file |
| `isOpen()` | Kiểm tra trạng thái |

### Đọc / Ghi

Kế thừa đầy đủ từ `QIODevice`:

```cpp
dev.write(data);          // ghi data
dev.readAll();            // đọc toàn bộ buffer
dev.read(n);              // đọc n bytes
dev.bytesAvailable();     // số bytes đang có trong buffer
```

### Signals

| Signal | Khi nào emit |
|---|---|
| `readyRead()` | Có data mới trong buffer |
| `errorOccurred(RPMsgError)` | Có lỗi xảy ra |
| `bytesWritten(qint64)` | Ghi xong (async path, hiện chưa active) |

### Error codes

| Mã lỗi | Ý nghĩa |
|---|---|
| `NoError` | Không có lỗi |
| `ChannelNameEmptyError` | Chưa đặt tên channel |
| `ChannelNameNotExistsError` | Channel không tồn tại trong sysfs |
| `PermissionError` | Không có quyền truy cập hoặc lock file |
| `OpenError` | Device đã được mở |
| `NotOpenError` | Device chưa được mở |
| `ReadError` | Lỗi khi đọc |
| `WriteError` | Lỗi khi ghi |
| `ResourceError` | Firmware bị ngắt kết nối |
| `TimeoutError` | Timeout |
| `UnknownError` | Lỗi không xác định |

---

## Lưu ý quan trọng

### NS Announcement
Sau khi `open()`, thư viện tự động gửi một bản tin NS để firmware phía R5 cập nhật địa chỉ `dst`. Echo của bản tin này được thư viện tự động bỏ qua (không xuất hiện ở `readyRead`). Ứng dụng không cần xử lý.

### Write chunking
RPMsg MTU mặc định là 512 bytes (496 bytes payload sau khi trừ 16 bytes header). Nếu gửi data lớn hơn, `write()` tự động chia thành nhiều chunk. Firmware sẽ nhận và echo từng chunk riêng biệt — **không tự động ghép lại**.

### Write async
Linux `rpmsg_char` driver hiện không expose `EPOLLOUT`, do đó write hoạt động ở chế độ **blocking** với chunking. `WriteNotifier` hiện không được sử dụng.

### isSequential
`QRPMsg::isSequential()` trả về `true`. RPMsg là FIFO message-based, không hỗ trợ `seek()` hay random access.

---

## Chạy test

Yêu cầu: firmware echo đang chạy trên R5 với ít nhất 1 channel (`ECHO_NUM_EPTS >= 1`). Test 5 channel concurrent yêu cầu `ECHO_NUM_EPTS = 5`.

```bash
cd tests/auto/qrpmsg
qmake && make
./tst_qrpmsg                        # chạy tất cả
./tst_qrpmsg test_write_and_echo    # chạy 1 test
./tst_qrpmsg -v2                    # verbose
```

Kết quả mong đợi trên KR260:

```
Totals: 18 passed, 0 failed, 0 skipped
```

---

## Khởi động firmware trên KR260

```bash
# Load và start firmware trên RPU
echo image_echo_test > /sys/class/remoteproc/remoteproc0/firmware
echo start > /sys/class/remoteproc/remoteproc0/state

# Kiểm tra channel đã xuất hiện trong sysfs
ls /sys/bus/rpmsg/devices/

# Dừng firmware
echo stop > /sys/class/remoteproc/remoteproc0/state
```

---

## License

`LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only`

Copyright (C) 2025 Phan Duc Manh &lt;manhpd9@viettel.com.vn&gt;
