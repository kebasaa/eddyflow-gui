#!/bin/sh
# QuaZip is linked from the system (or from the path given to qmake as
# QUAZIP_LIB_DIR), so nothing needs copying next to the binary but the
# ancillary file templates.

DEBUG_OR_RELEASE=$1
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

if [ "$DEBUG_OR_RELEASE" != "debug" ] && [ "$DEBUG_OR_RELEASE" != "release" ]; then
  echo "usage: $0 debug|release" >&2
  exit 1
fi

BUILD_DIR="./$DEBUG_OR_RELEASE"
echo "[BUILD_DIR: $BUILD_DIR]"

echo "Copy ancillary template files in the app binary folder..."
mkdir -p "$BUILD_DIR/file-templates" || exit 1
cp "$SCRIPT_DIR/../../file-templates/"*.txt "$BUILD_DIR/file-templates" || exit 1
