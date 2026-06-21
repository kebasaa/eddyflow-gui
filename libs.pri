# define libraries to build and link
# QUAZIP_LIB_DIR and QUAZIP_LIB_NAME can be passed on the qmake command line

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
        LIBS += -L$$OUT_PWD/../../libs/mac/quazip-1.x -lquazip1-qt6_debug

        # linking
        QMAKE_PRE_LINK += $$_PRO_FILE_PWD_/scripts/build/mac-pre-link.sh debug $$OUT_PWD
        QMAKE_POST_LINK += $$_PRO_FILE_PWD_/scripts/build/mac-post-link.sh debug
    }
    linux {
        # quazip 1.x for Qt6 (debug)
        LIBS += -L$$OUT_PWD/../../libs/linux/quazip-1.x -lquazip1-qt6_debug

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
        LIBS += -L$$OUT_PWD/../../libs/mac/quazip-1.x -lquazip1-qt6

        # linking
        QMAKE_PRE_LINK += $$_PRO_FILE_PWD_/scripts/build/mac-pre-link.sh release
        QMAKE_POST_LINK += $$_PRO_FILE_PWD_/scripts/build/mac-post-link.sh release
    }
    linux {
        # quazip 1.x for Qt6 (release)
        LIBS += -L$$OUT_PWD/../../libs/linux/quazip-1.x -lquazip1-qt6

        # linking
        QMAKE_PRE_LINK += $$_PRO_FILE_PWD_/scripts/build/lin-pre-link.sh release
    }
}
