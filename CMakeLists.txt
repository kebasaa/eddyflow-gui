# EddyFlow GUI build
#
# One project for Windows (Qt's MinGW kit), Linux and macOS. CMakePresets.json
# holds the usual configurations; the README shows how to point them at a Qt
# kit and at QuaZIP and zlib when they are not on the default search path.
#
# The binary lands in <build>/release/EddyFlow or <build>/debug/EddyFlow_debug
# (an .app bundle on macOS). The release scripts in eddyflow-build-script look
# for it there, so keep those two names and folders.

cmake_minimum_required(VERSION 3.21)

# Apple Silicon only. Must be set before project(), which probes the compiler;
# pass -DCMAKE_OSX_ARCHITECTURES=... to change it. Ignored elsewhere.
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures to build for")

# The version also lives in src/defs.h (APP_VERSION, APP_VERSION_STR). Bump both.
project(EddyFlow VERSION 8.1.10 LANGUAGES CXX)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Debug or Release" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.7 REQUIRED COMPONENTS
    Core Gui Widgets Network Concurrent Svg LinguistTools)
qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

# zlib, for the MAT-file reader. QuaZip already pulls it in transitively, but
# naming it here is what makes matfile.cpp's own use of inflate() legitimate
# rather than a lucky side effect of link order.
find_package(ZLIB REQUIRED)

# QuaZip 1.x for Qt 6, found through CMAKE_PREFIX_PATH.
#
# Its CMake package insists on Qt's Core5Compat module, even when QuaZip was
# built without it - which is how eddyflow-build-script builds it for Windows,
# against a Qt kit that may not have the module. So the package is only tried
# when Core5Compat is there; otherwise QuaZip is looked up as a plain header
# and library. QUAZIP_INCLUDE_DIR (the folder holding quazip.h) and
# QUAZIP_LIBRARY can be set by hand when the lookup misses.
find_package(Qt6 QUIET COMPONENTS Core5Compat)
if(TARGET Qt6::Core5Compat)
    find_package(QuaZip-Qt6 CONFIG QUIET)
endif()
if(NOT TARGET QuaZip::QuaZip)
    set(quazip_suffixes quazip QuaZip-Qt6/quazip)
    foreach(prefix IN LISTS CMAKE_PREFIX_PATH CMAKE_SYSTEM_PREFIX_PATH)
        file(GLOB versioned LIST_DIRECTORIES true RELATIVE "${prefix}/include"
            "${prefix}/include/QuaZip-Qt6-*")
        foreach(dir IN LISTS versioned)
            list(APPEND quazip_suffixes "${dir}/quazip")
        endforeach()
    endforeach()
    find_path(QUAZIP_INCLUDE_DIR quazip.h PATH_SUFFIXES ${quazip_suffixes})
    find_library(QUAZIP_LIBRARY NAMES quazip1-qt6)
    if(NOT QUAZIP_INCLUDE_DIR OR NOT QUAZIP_LIBRARY)
        message(FATAL_ERROR
            "QuaZip for Qt 6 was not found. Add its install prefix to CMAKE_PREFIX_PATH, "
            "or set QUAZIP_INCLUDE_DIR and QUAZIP_LIBRARY.")
    endif()
    if(WIN32)
        find_file(QUAZIP_DLL NAMES libquazip1-qt6.dll quazip1-qt6.dll PATH_SUFFIXES bin)
    endif()
    if(WIN32 AND QUAZIP_DLL)
        add_library(QuaZip::QuaZip SHARED IMPORTED)
        set_target_properties(QuaZip::QuaZip PROPERTIES
            IMPORTED_LOCATION "${QUAZIP_DLL}"
            IMPORTED_IMPLIB "${QUAZIP_LIBRARY}")
    else()
        add_library(QuaZip::QuaZip UNKNOWN IMPORTED)
        set_target_properties(QuaZip::QuaZip PROPERTIES
            IMPORTED_LOCATION "${QUAZIP_LIBRARY}")
    endif()
    get_filename_component(quazip_parent "${QUAZIP_INCLUDE_DIR}" DIRECTORY)
    set(quazip_links Qt6::Core ZLIB::ZLIB)
    if(TARGET Qt6::Core5Compat)
        list(APPEND quazip_links Qt6::Core5Compat)
    endif()
    set_target_properties(QuaZip::QuaZip PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${QUAZIP_INCLUDE_DIR};${quazip_parent}"
        INTERFACE_LINK_LIBRARIES "${quazip_links}")
    message(STATUS "QuaZip: ${QUAZIP_LIBRARY} (headers in ${QUAZIP_INCLUDE_DIR})")
endif()

set(EDDYFLOW_HEADERS
    src/absolute_limit_units.h
    src/globalsettings.h
    src/widget_utils.h
    src/aboutdialog.h
    src/anem_delegate.h
    src/anem_desc.h
    src/anem_model.h
    src/anem_tableview.h
    src/anem_view.h
    src/binarysettingsdialog.h
    src/biomitem.h
    src/biommetadatareader.h
    src/bminidefs.h
    src/cecpairmodel.h
    src/cecsettingsdialog.h
    src/clicklabel.h
    src/detlimsettingsdialog.h
    src/langfordsettingsdialog.h
    src/configstate.h
    src/customcombomodel.h
    src/customheader.h
    src/customsplashscreen.h
    src/defs.h
    src/dlinidefs.h
    src/dlinidialog.h
    src/dlinstrtab.h
    src/dlproject.h
    src/dlprojectstate.h
    src/matfile.h
    src/eddyuhimport.h
    src/eddyproexport.h
    src/eddyprogasslotdialog.h
    src/dlrawfiledesctab.h
    src/dlsitetab.h
    src/despikearena.h
    src/dndpanel.h
    src/docchooser.h
    src/eastereggkeyfilter.h
    src/eastereggpage.h
    src/eastereggwidgets.h
    src/ecinidefs.h
    src/ecproject.h
    src/ecprojectstate.h
    src/measurement_record.h
    src/faderwidget.h
    src/fileutils.h
    src/flux_units.h
    src/fluxrunner.h
    src/ini_file.h
    src/gas_metadata.h
    src/infomessage.h
    src/irga_delegate.h
    src/irga_desc.h
    src/irga_model.h
    src/irga_tableview.h
    src/irga_view.h
    src/mainwindow.h
    src/matrixrain.h
    src/rawplayback.h
    src/mymenu.h
    src/mystyle.h
    src/mytabwidget.h
    src/nonzerodoublespinbox.h
    src/outputspace.h
    src/previousrun.h
    src/planarfitsettingsdialog.h
    src/pwbtimelagsettingsdialog.h
    src/engineprogress.h
    src/lagstats.h
    src/laghistogramwidget.h
    src/logpercentslider.h
    src/timelagpreassessdialog.h
    src/process.h
    src/proxystyle.h
    src/QProgressIndicator.h
    src/rawfilenamedialog.h
    src/rawfilesettingsdialog.h
    src/runeta.h
    src/runpage.h
    src/splitter.h
    src/splitterhandle.h
    src/stringutils.h
    src/table_delegate_utils.h
    src/timelagsettingsdialog.h
    src/tlagdonordialog.h
    src/tooltipfilter.h
    src/variable_delegate.h
    src/variable_desc.h
    src/variable_model.h
    src/variable_tableview.h
    src/variable_view.h
    src/wheeleventfilter.h
    src/smartfluxbar.h
    src/mainwidget.h
    src/welcomepage.h
    src/projectpage.h
    src/basicsettingspage.h
    src/advancedsettingspage.h
    src/advstatisticaloptions.h
    src/advprocessingoptions.h
    src/updatedialog.h
    src/updateinstaller.h
    src/advspectraloptions.h
    src/advoutputoptions.h
    src/advsettingscontainer.h
    src/createpackagedialog.h
    src/angle_tableview.h
    src/angle_tablemodel.h
    src/angles_view.h
    src/angle_item.h
    src/ancillaryfiletest.h
    src/customcombobox.h
    src/advmenudelegate.h
    src/container_helpers.h
    src/network_helpers.h
    src/qt_helpers.h
    src/customclearlineedit.h
    src/customresetlineedit.h
    src/customlineedit.h
    src/filebrowsewidget.h
    src/lineeditandbrowsewidget.h
    src/dirbrowsewidget.h
    src/fileformatwidget.h
    src/customdroplineedit.h
    src/customcheckbox.h
    src/clickablelabel.h
    src/hovercheckbox.h
    src/richtextcheckbox.h
    src/detectdaterangedialog.h
    src/downloadmanager.h
    src/remotebrowsedialog.h
    src/remotesource.h
    src/openfilefilter.h
    src/windfilter_view.h
    src/windfilter_tableview.h
    src/windfilter_tablemodel.h
    src/sector_item.h
    src/logging.h
)

set(EDDYFLOW_SOURCES
    src/absolute_limit_units.cpp
    src/measurement_record.cpp
    src/globalsettings.cpp
    src/widget_utils.cpp
    src/aboutdialog.cpp
    src/anem_delegate.cpp
    src/anem_desc.cpp
    src/anem_model.cpp
    src/anem_tableview.cpp
    src/anem_view.cpp
    src/binarysettingsdialog.cpp
    src/biommetadatareader.cpp
    src/cecpairmodel.cpp
    src/cecsettingsdialog.cpp
    src/clicklabel.cpp
    src/detlimsettingsdialog.cpp
    src/langfordsettingsdialog.cpp
    src/customcombomodel.cpp
    src/customheader.cpp
    src/customsplashscreen.cpp
    src/dlinidialog.cpp
    src/dlinstrtab.cpp
    src/dlproject.cpp
    src/matfile.cpp
    src/eddyuhimport.cpp
    src/eddyproexport.cpp
    src/eddyprogasslotdialog.cpp
    src/dlrawfiledesctab.cpp
    src/dlsitetab.cpp
    src/despikearena.cpp
    src/dndpanel.cpp
    src/docchooser.cpp
    src/eastereggkeyfilter.cpp
    src/eastereggpage.cpp
    src/eastereggwidgets.cpp
    src/ecproject.cpp
    src/faderwidget.cpp
    src/fileutils.cpp
    src/flux_units.cpp
    src/fluxrunner.cpp
    src/ini_file.cpp
    src/gas_metadata.cpp
    src/infomessage.cpp
    src/irga_delegate.cpp
    src/irga_desc.cpp
    src/irga_model.cpp
    src/irga_tableview.cpp
    src/irga_view.cpp
    src/main.cpp
    src/mainwindow.cpp
    src/matrixrain.cpp
    src/rawplayback.cpp
    src/nonzerodoublespinbox.cpp
    src/outputspace.cpp
    src/previousrun.cpp
    src/planarfitsettingsdialog.cpp
    src/pwbtimelagsettingsdialog.cpp
    src/lagstats.cpp
    src/laghistogramwidget.cpp
    src/logpercentslider.cpp
    src/timelagpreassessdialog.cpp
    src/basicsettingspage.cpp
    src/process.cpp
    src/proxystyle.cpp
    src/QProgressIndicator.cpp
    src/rawfilenamedialog.cpp
    src/rawfilesettingsdialog.cpp
    src/runeta.cpp
    src/runpage.cpp
    src/splitter.cpp
    src/splitterhandle.cpp
    src/stringutils.cpp
    src/timelagsettingsdialog.cpp
    src/tlagdonordialog.cpp
    src/tooltipfilter.cpp
    src/variable_delegate.cpp
    src/variable_desc.cpp
    src/variable_model.cpp
    src/variable_tableview.cpp
    src/variable_view.cpp
    src/wheeleventfilter.cpp
    src/smartfluxbar.cpp
    src/mainwidget.cpp
    src/welcomepage.cpp
    src/projectpage.cpp
    src/advancedsettingspage.cpp
    src/advstatisticaloptions.cpp
    src/advprocessingoptions.cpp
    src/updatedialog.cpp
    src/updateinstaller.cpp
    src/advspectraloptions.cpp
    src/advoutputoptions.cpp
    src/advsettingscontainer.cpp
    src/createpackagedialog.cpp
    src/angle_tableview.cpp
    src/angles_view.cpp
    src/angle_tablemodel.cpp
    src/ancillaryfiletest.cpp
    src/defs.cpp
    src/customcombobox.cpp
    src/advmenudelegate.cpp
    src/network_helpers.cpp
    src/qt_helpers.cpp
    src/customclearlineedit.cpp
    src/customresetlineedit.cpp
    src/customlineedit.cpp
    src/filebrowsewidget.cpp
    src/lineeditandbrowsewidget.cpp
    src/dirbrowsewidget.cpp
    src/mymenu.cpp
    src/fileformatwidget.cpp
    src/customdroplineedit.cpp
    src/customcheckbox.cpp
    src/clickablelabel.cpp
    src/hovercheckbox.cpp
    src/richtextcheckbox.cpp
    src/detectdaterangedialog.cpp
    src/downloadmanager.cpp
    src/remotebrowsedialog.cpp
    src/remotesource.cpp
    src/openfilefilter.cpp
    src/mystyle.cpp
    src/windfilter_view.cpp
    src/windfilter_tableview.cpp
    src/windfilter_tablemodel.cpp
    src/logging.cpp
)

qt_add_executable(EddyFlow ${EDDYFLOW_HEADERS} ${EDDYFLOW_SOURCES})

target_link_libraries(EddyFlow PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Concurrent Qt6::Svg
    QuaZip::QuaZip
    ZLIB::ZLIB)

# Debug and release side by side, each in its own folder, the release scripts
# expect release/EddyFlow and debug/EddyFlow_debug.
set_target_properties(EddyFlow PROPERTIES
    OUTPUT_NAME_DEBUG EddyFlow_debug
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<LOWER_CASE:$<CONFIG>>")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(EDDYFLOW_EXECUTABLE_NAME EddyFlow_debug)
else()
    set(EDDYFLOW_EXECUTABLE_NAME EddyFlow)
endif()

target_compile_definitions(EddyFlow PRIVATE
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_FROM_BYTEARRAY
    QT_NO_CAST_TO_ASCII
    QT_NO_URL_CAST_FROM_STRING
    QT_USE_QSTRINGBUILDER
    QT_DEPRECATED_WARNINGS
    $<$<CONFIG:Debug>:QT_DEBUG>
    # A release build silences qDebug and qWarning, and Qt expands them to
    # "while (false) ...", so nothing inside such a statement runs.
    $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG QT_NO_DEBUG_OUTPUT QT_NO_WARNING_OUTPUT>)

# Warnings. Qt's and QuaZip's headers come in through imported targets, which
# CMake already marks as system includes, so these stay about our own code.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(EddyFlow PRIVATE -Wall -Wextra)
endif()
if(MINGW)
    target_compile_options(EddyFlow PRIVATE
        $<$<CONFIG:Debug>:-Wdeprecated -Wcast-qual -Wwrite-strings -fdiagnostics-show-option
                          -O0 -fno-inline -Wunused-result>)
    # Strip the release binary, as qmake's MinGW kit did.
    target_link_options(EddyFlow PRIVATE $<$<CONFIG:Release>:-s>)
endif()
if(APPLE)
    target_compile_options(EddyFlow PRIVATE
        -gdwarf-2
        $<$<CONFIG:Debug>:-Weverything
            -Wno-c++98-compat -Wno-c++98-compat-pedantic
            -Wno-padded
            -Wno-covered-switch-default
            -Wno-used-but-marked-unused
            -Wno-global-constructors
            -Wno-exit-time-destructors
            -Wno-documentation -Wno-documentation-unknown-command -Wno-documentation-deprecated-sync>)
endif()

# Resources. The platform .qrc carries the style sheets, icons and fonts.
if(WIN32)
    target_sources(EddyFlow PRIVATE EddyFlow_win.qrc)
elseif(APPLE)
    target_sources(EddyFlow PRIVATE EddyFlow_mac.qrc)
else()
    target_sources(EddyFlow PRIVATE EddyFlow_lin.qrc)
endif()
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    # rcc at zlib's best compression, every file regardless of size.
    set_target_properties(EddyFlow PROPERTIES AUTORCC_OPTIONS "-threshold;0;-compress;9")
endif()

# Translations. lrelease compiles the tracked .ts on every build, and the .qm
# is embedded where main.cpp loads it from, :/tra/en.
qt_add_lrelease(
    TS_FILES tra/EddyFlow_en.ts
    QM_FILES_OUTPUT_VARIABLE EDDYFLOW_QM_FILES)
set_source_files_properties(${EDDYFLOW_QM_FILES} PROPERTIES QT_RESOURCE_ALIAS en)
qt_add_resources(EddyFlow "translations"
    PREFIX "/tra"
    FILES ${EDDYFLOW_QM_FILES})

# What the GUI loads from beside its own binary, so a build runs from its
# build folder: the file templates, and on Windows and macOS the QuaZip
# runtime. Packaging the engines, 7z and the Qt runtime is left to the scripts
# in eddyflow-build-script.
get_target_property(EDDYFLOW_QUAZIP_TYPE QuaZip::QuaZip TYPE)
file(GLOB EDDYFLOW_FILE_TEMPLATES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/file-templates/*.txt")

if(WIN32)
    # The debug build keeps its console, for qDebug.
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(EddyFlow PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()

    # Icon and version resource; qmake generated the same from RC_ICONS and VERSION.
    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/win_files/app.ico" EDDYFLOW_ICON_PATH)
    configure_file(win_files/EddyFlow.rc.in "${CMAKE_CURRENT_BINARY_DIR}/EddyFlow_resource.rc" @ONLY)
    target_sources(EddyFlow PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/EddyFlow_resource.rc")

    add_custom_command(TARGET EddyFlow POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:EddyFlow>/file-templates"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                ${EDDYFLOW_FILE_TEMPLATES}
                "$<TARGET_FILE_DIR:EddyFlow>/file-templates"
        VERBATIM)
    if(EDDYFLOW_QUAZIP_TYPE STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET EddyFlow POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "$<TARGET_FILE:QuaZip::QuaZip>" "$<TARGET_FILE_DIR:EddyFlow>"
            VERBATIM)
    endif()

elseif(APPLE)
    set(EDDYFLOW_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/mac_files/app.icns")
    set_source_files_properties("${EDDYFLOW_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(EddyFlow PRIVATE "${EDDYFLOW_ICNS}")

    # The Info.plist templates use qmake's placeholders; CMake fills the same
    # names when it configures the plist.
    set(FULL_VERSION "${PROJECT_VERSION}")
    set(ICON "app.icns")
    set(EXECUTABLE "${EDDYFLOW_EXECUTABLE_NAME}")
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        set(MACOSX_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    elseif(QT_SUPPORTED_MIN_MACOS_VERSION)
        set(MACOSX_DEPLOYMENT_TARGET "${QT_SUPPORTED_MIN_MACOS_VERSION}")
    else()
        set(MACOSX_DEPLOYMENT_TARGET "12.0")
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(EDDYFLOW_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/mac_files/plist/debug/Info.plist")
    else()
        set(EDDYFLOW_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/mac_files/plist/release/Info.plist")
    endif()

    set_target_properties(EddyFlow PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${EDDYFLOW_PLIST}"
        # QuaZip's install name is @rpath/..., and it is copied to Frameworks below.
        BUILD_RPATH "@executable_path/../Frameworks"
        INSTALL_RPATH "@executable_path/../Frameworks")

    set(EDDYFLOW_MACOS_DIR "$<TARGET_BUNDLE_CONTENT_DIR:EddyFlow>/MacOS")
    add_custom_command(TARGET EddyFlow POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${EDDYFLOW_MACOS_DIR}/file-templates"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                ${EDDYFLOW_FILE_TEMPLATES}
                "${EDDYFLOW_MACOS_DIR}/file-templates"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${EDDYFLOW_MACOS_DIR}/docs"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/docs" "${EDDYFLOW_MACOS_DIR}/docs"
        VERBATIM)
    if(EDDYFLOW_QUAZIP_TYPE STREQUAL "SHARED_LIBRARY")
        # -a keeps the versioned dylib and its symlinks together.
        add_custom_command(TARGET EddyFlow POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:EddyFlow>/Frameworks"
            COMMAND /bin/sh -c "cp -af \"$1\"/libquazip*.dylib \"$2\"/" sh
                    "$<TARGET_FILE_DIR:QuaZip::QuaZip>"
                    "$<TARGET_BUNDLE_CONTENT_DIR:EddyFlow>/Frameworks"
            VERBATIM)
    endif()
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        # Remove debug symbols from the release binary.
        add_custom_command(TARGET EddyFlow POST_BUILD
            COMMAND strip -S "$<TARGET_FILE:EddyFlow>"
            VERBATIM)
    endif()

    # The clang static analyzer, in a build tree of its own.
    add_custom_target(clang-static-check
        COMMAND /bin/bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test/run-clang-analyzer.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/clang-static-check"
        USES_TERMINAL
        VERBATIM)

else()
    add_custom_command(TARGET EddyFlow POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:EddyFlow>/file-templates"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                ${EDDYFLOW_FILE_TEMPLATES}
                "$<TARGET_FILE_DIR:EddyFlow>/file-templates"
        VERBATIM)
endif()
