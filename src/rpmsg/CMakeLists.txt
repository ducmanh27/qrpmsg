# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

#####################################################################
## RPMsg Module:
#####################################################################
qt_find_package(Libudev)

qt_internal_add_module(RPMsg
    SOURCES
    qrpmsg.cpp qrpmsg.h qrpmsg_p.h
    qrpmsgglobal.h
    qrpmsginfo.cpp qrpmsginfo.h qrpmsginfo_p.h
    NO_PCH_SOURCES
        removed_api.cpp
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}
    LIBRARIES
        Qt::CorePrivate
    PUBLIC_LIBRARIES
        Qt::Core
    PRIVATE_MODULE_INTERFACE
        Qt::CorePrivate
)


## Scopes:
#####################################################################

qt_internal_extend_target(RPMsg CONDITION QT_FEATURE_libudev AND UNIX
   DEFINES
       LINK_LIBUDEV
   LIBRARIES
       PkgConfig::Libudev
)


qt_internal_extend_target(RPMsg CONDITION UNIX
    SOURCES
        qrpmsg_unix.cpp qrpmsginfo_unix.cpp
        rpmsglinuxhelper.cpp rpmsglinuxhelper.h
)



# qt_internal_add_docs(RPMsg
#     doc/qrpmsg.qdocconf
# )

