#!/bin/bash
# Called by qmake before linking (see EddyFlow_mac.pro). Compiles the
# translation source into the .qm file with the lrelease of the Qt kit that
# runs the build. It does not run lupdate, which would rewrite the tracked .ts
# file on every build.
#
# usage: mac-update-translations.sh <qt_host_bins>

echo 'Update translation files...'

QT_HOST_BINS=$1
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TRA_DIR="$SCRIPT_DIR/../../tra"

LRELEASE="$QT_HOST_BINS/lrelease"
if [ ! -x "$LRELEASE" ]; then
    LRELEASE=$(command -v lrelease)
fi
if [ -z "$LRELEASE" ]; then
    echo "lrelease was not found in '$QT_HOST_BINS' or on PATH" >&2
    exit 1
fi

"$LRELEASE" "$TRA_DIR/EddyFlow_en.ts" -qm "$TRA_DIR/EddyFlow_en.qm" || exit 2

echo 'translations updated.'
