lessThan(QT_MAJOR_VERSION, 5) {
    message("Cannot build current QtRPMsg sources with Qt version $${QT_VERSION}.")
}

requires(!integrity)
requires(!vxworks)
requires(!winrt)
requires(!uikit)
requires(!emscripten)

load(configure)
qtCompileTest(ntddmodm)

load(qt_parts)
