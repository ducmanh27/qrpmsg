INCLUDEPATH += $$PWD

unix:qtConfig(libudev) {
    DEFINES += LINK_LIBUDEV
    INCLUDEPATH += $$QMAKE_INCDIR_LIBUDEV
    LIBS_PRIVATE += $$QMAKE_LIBS_LIBUDEV
}

PUBLIC_HEADERS += \
    $$PWD/qrpmsgglobal.h \
    $$PWD/qrpmsg.h \
    $$PWD/qrpmsginfo.h

PRIVATE_HEADERS += \
    $$PWD/qrpmsg_p.h \
    $$PWD/qrpmsginfo_p.h \
    $$PWD/rpmsglinuxhelper.h

SOURCES += \
    $$PWD/qrpmsg.cpp \
    $$PWD/qrpmsginfo.cpp


unix {
    SOURCES += \
        $$PWD/qrpmsg_unix.cpp \
        $$PWD/rpmsglinuxhelper.cpp
}

HEADERS += $$PUBLIC_HEADERS $$PRIVATE_HEADERS
