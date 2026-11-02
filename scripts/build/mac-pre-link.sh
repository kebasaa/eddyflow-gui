#!/bin/bash
# Called by qmake before linking the app bundle (see libs.pri). Puts what the
# GUI loads from next to its own binary into the bundle, so a plain qmake or
# Qt Creator build runs from the build folder: Contents/MacOS/file-templates,
# Contents/MacOS/docs and, when a QuaZIP lib dir is given, its dylib in
# Contents/Frameworks. The engines and 7z still go in Contents/MacOS/bin;
# build_eddyflow_macos.sh in eddyflow-build-script does that and runs
# macdeployqt for a release package.
#
# usage: mac-pre-link.sh debug|release <out_pwd> [<quazip_lib_dir>]

echo "[Running $0]"

DEBUG_OR_RELEASE=$1
OUT_DIR=$2
QUAZIP_LIB_DIR=$3
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SOURCE_DIR="$SCRIPT_DIR/../.."

if { [ "$DEBUG_OR_RELEASE" != "debug" ] && [ "$DEBUG_OR_RELEASE" != "release" ]; } || [ -z "$OUT_DIR" ]; then
    echo "usage: $0 debug|release <out_pwd> [<quazip_lib_dir>]" >&2
    exit 1
fi

if [ "$DEBUG_OR_RELEASE" = "debug" ]; then
    BUNDLE="$OUT_DIR/debug/EddyFlow_debug.app"
else
    BUNDLE="$OUT_DIR/release/EddyFlow.app"
fi
MACOS_DIR="$BUNDLE/Contents/MacOS"
FRAMEWORKS_DIR="$BUNDLE/Contents/Frameworks"
echo "[BUNDLE: $BUNDLE]"

echo "Copy ancillary template files in the app bundle..."
mkdir -p "$MACOS_DIR/file-templates" || exit 2
cp -f "$SOURCE_DIR/file-templates/"*.txt "$MACOS_DIR/file-templates/" || exit 3

echo "Copy the local documentation in the app bundle..."
rm -rf "$MACOS_DIR/docs"
cp -R "$SOURCE_DIR/docs" "$MACOS_DIR/docs" || exit 4

if [ -n "$QUAZIP_LIB_DIR" ]; then
    echo "Copy QuaZIP from $QUAZIP_LIB_DIR in the app bundle..."
    mkdir -p "$FRAMEWORKS_DIR" || exit 5
    # -a keeps the versioned dylib and its symlinks together.
    cp -af "$QUAZIP_LIB_DIR/"libquazip*.dylib "$FRAMEWORKS_DIR/" || exit 6
fi
