# platform specific configurations

macx {
    CONFIG += app_bundle

    # Apple Silicon only. The minimum macOS version is Qt's default; pass
    # QMAKE_MACOSX_DEPLOYMENT_TARGET=<version> to qmake to change it.
    QMAKE_APPLE_DEVICE_ARCHS = arm64

    QMAKE_CFLAGS += -gdwarf-2
    QMAKE_CXXFLAGS += -gdwarf-2

    RESOURCES += $$_PRO_FILE_PWD_/EddyFlow_mac.qrc
}

win32 {
    RESOURCES += $$_PRO_FILE_PWD_/EddyFlow_win.qrc
}

linux {
    RESOURCES += $$_PRO_FILE_PWD_/EddyFlow_lin.qrc
}

# to copy files in the bundle
#macx {
#    jpegtran.path = Contents/MacOS
#    jpegtran.files = /Users/maloney/dev/bin/jpegtran
#    QMAKE_BUNDLE_DATA += jpegtran
#}

# or in the bin folder
#win32 {
#   EXTRA_BINFILES_WIN = ..\\bin\\jpegtran.exe
#   for( FILE, EXTRA_BINFILES_WIN ) {
#      QMAKE_PRE_LINK += $$quote(cmd /c copy /y $${FILE} $${DEBUG_OR_RELEASE}$$escape_expand(\\n\\t))
#    }
#}
