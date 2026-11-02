#!/bin/bash
# Called by qmake after linking the app bundle (see libs.pri). Lets the binary
# find the libraries mac-pre-link.sh copied into Contents/Frameworks: QuaZIP's
# install name is @rpath/..., so that folder must be an rpath of the binary.
#
# usage: mac-post-link.sh debug|release <out_pwd>

echo "[Running $0]"

DEBUG_OR_RELEASE=$1
OUT_DIR=$2

if { [ "$DEBUG_OR_RELEASE" != "debug" ] && [ "$DEBUG_OR_RELEASE" != "release" ]; } || [ -z "$OUT_DIR" ]; then
    echo "usage: $0 debug|release <out_pwd>" >&2
    exit 1
fi

if [ "$DEBUG_OR_RELEASE" = "debug" ]; then
    APP_BINARY="$OUT_DIR/debug/EddyFlow_debug.app/Contents/MacOS/EddyFlow_debug"
else
    APP_BINARY="$OUT_DIR/release/EddyFlow.app/Contents/MacOS/EddyFlow"
fi
RPATH="@executable_path/../Frameworks"
echo "[APP_BINARY: $APP_BINARY]"

# Qt's mkspec usually adds this rpath already, and adding it twice fails.
if otool -l "$APP_BINARY" | grep -A2 LC_RPATH | grep -q "path $RPATH "; then
    echo "$RPATH is already an rpath of the binary."
else
    echo "Add $RPATH as an rpath of the binary..."
    install_name_tool -add_rpath "$RPATH" "$APP_BINARY" || exit 2
fi
