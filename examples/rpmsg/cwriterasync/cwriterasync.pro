QT = core
QT += rpmsg
CONFIG += debug
CONFIG += console
CONFIG -= app_bundle

TARGET = cwriterasync
TEMPLATE = app

HEADERS += \
    rpmsgwriter.h

SOURCES += \
    main.cpp \
    rpmsgwriter.cpp
LIBS += -L$$PWD/../../../build/Desktop_Qt_5_15_2_GCC_64bit-Debug/lib -lQt5RPMsg
target.path = $$[QT_INSTALL_EXAMPLES]/rpmsg/cwriterasync
INSTALLS += target
