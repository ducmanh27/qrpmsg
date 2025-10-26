QT = core
QT += rpmsg

CONFIG += console
CONFIG -= app_bundle

TARGET = creaderasync
TEMPLATE = app

HEADERS += \
    rpmsgreader.h

SOURCES += \
    main.cpp \
    rpmsgreader.cpp

target.path = $$[QT_INSTALL_EXAMPLES]/rpmsg/creaderasync
INSTALLS += target
