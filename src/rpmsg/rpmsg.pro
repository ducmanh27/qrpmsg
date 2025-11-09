TARGET = QtRPMsg
QT = core-private

# QMAKE_DOCS = $$PWD/doc/qtrpmsg.qdocconf

config_ntddmodm: DEFINES += QT_NO_REDEFINE_GUID_DEVINTERFACE_MODEM

include($$PWD/rpmsg-lib.pri)

load(qt_module)

PRECOMPILED_HEADER =
