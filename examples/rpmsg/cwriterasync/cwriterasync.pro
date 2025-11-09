QT = core
QT += rpmsg

CONFIG += console
CONFIG -= app_bundle

TARGET = cwriterasync
TEMPLATE = app

HEADERS += \
    rpmsgwriter.h

SOURCES += \
    main.cpp \
    rpmsgwriter.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/rpmsg/cwriterasync
INSTALLS += target
