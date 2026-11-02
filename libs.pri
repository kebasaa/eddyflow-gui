# define libraries to build and link
# QUAZIP_LIB_DIR and QUAZIP_LIB_NAME can be passed on the qmake command line

# zlib, for the MAT-file reader. QuaZip already pulls it in transitively, but
# naming it here is what makes matfile.cpp's own use of inflate() legitimate
# rather than a lucky side effect of link order - and it is present in the
# MinGW sysroot, on macOS and on every Linux Qt builds against.
LIBS += -lz

CONFIG(debug, debug|release) {
    win32 {
        # quazip 1.x for Qt6 (debug)
        !isEmpty(QUAZIP_LIB_DIR): LIBS += -L$$QUAZIP_LIB_DIR
        !isEmpty(QUAZIP_LIB_NAME): LIBS += -l$$QUAZIP_LIB_NAME
        else: LIBS += -lquazip1-qt6

        QMAKE_PRE_LINK += $$_PRO_FILE_PWD_/scripts/build/win-pre-link-debug.cmd
    }
    macx {
        # quazip 1.x for Qt6 (debug)
        !isEmpty(QUAZIP_LIB_DIR): LIBS += -L$$QUAZIP_LIB_DIR
        !isEmpty(QUAZIP_LIB_NAME): LIBS += -l$$QUAZIP_LIB_NAME
        else: LIBS += -lquazip1-qt6

        # linking: templates, docs and QuaZIP into the bundle, then its rpath
        QMAKE_PRE_LINK += /bin/bash $$shell_quote($$_PRO_FILE_PWD_/scripts/build/mac-pre-link.sh) debug $$shell_quote($$OUT_PWD) $$shell_quote($$QUAZIP_LIB_DIR)
        QMAKE_POST_LINK += /bin/bash $$shell_quote($$_PRO_FILE_PWD_/scripts/build/mac-post-link.sh) debug $$shell_quote($$OUT_PWD)
    }
    linux {
        # quazip 1.x for Qt6 (debug)
        !isEmpty(QUAZIP_LIB_DIR): LIBS += -L$$QUAZIP_LIB_DIR
        !isEmpty(QUAZIP_LIB_NAME): LIBS += -l$$QUAZIP_LIB_NAME
        else: LIBS += -lquazip1-qt6

        # linking
        QMAKE_PRE_LINK += $$_PRO_FILE_PWD_/scripts/build/lin-pre-link.sh debug
    }
} else {
    win32 {
        # quazip 1.x for Qt6 (release)
        !isEmpty(QUAZIP_LIB_DIR): LIBS += -L$$QUAZIP_LIB_DIR
        !isEmpty(QUAZIP_LIB_NAME): LIBS += -l$$QUAZIP_LIB_NAME
        else: LIBS += -lquazip1-qt6

        QMAKE_PRE_LINK += $$_PRO_FILE_PWD_/scripts/build/win-pre-link-release.cmd
    }
    macx {
        # quazip 1.x for Qt6 (release)
        !isEmpty(QUAZIP_LIB_DIR): LIBS += -L$$QUAZIP_LIB_DIR
        !isEmpty(QUAZIP_LIB_NAME): LIBS += -l$$QUAZIP_LIB_NAME
        else: LIBS += -lquazip1-qt6

        # linking: templates, docs and QuaZIP into the bundle, then its rpath
        QMAKE_PRE_LINK += /bin/bash $$shell_quote($$_PRO_FILE_PWD_/scripts/build/mac-pre-link.sh) release $$shell_quote($$OUT_PWD) $$shell_quote($$QUAZIP_LIB_DIR)
        QMAKE_POST_LINK += /bin/bash $$shell_quote($$_PRO_FILE_PWD_/scripts/build/mac-post-link.sh) release $$shell_quote($$OUT_PWD)
    }
    linux {
        # quazip 1.x for Qt6 (release)
        !isEmpty(QUAZIP_LIB_DIR): LIBS += -L$$QUAZIP_LIB_DIR
        !isEmpty(QUAZIP_LIB_NAME): LIBS += -l$$QUAZIP_LIB_NAME
        else: LIBS += -lquazip1-qt6

        # linking
        QMAKE_PRE_LINK += $$_PRO_FILE_PWD_/scripts/build/lin-pre-link.sh release
    }
}
