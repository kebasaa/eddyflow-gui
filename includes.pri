# include paths — QUAZIP_INCLUDE_DIR can be passed on qmake command line
!isEmpty(QUAZIP_INCLUDE_DIR) {
    INCLUDEPATH += $$QUAZIP_INCLUDE_DIR
} else {
    INCLUDEPATH += $$_PRO_FILE_PWD_/libs/quazip-1.x/quazip
}
