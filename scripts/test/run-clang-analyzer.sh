#!/usr/bin/env sh
# Run the clang static analyzer over a debug build, in a build tree of its own.
# scan-build has to wrap the configure step as well as the build, because the
# compiler it substitutes is recorded when CMake configures.
#
# usage: run-clang-analyzer.sh <source_dir> <scan_build_dir>
# The `clang-static-check` target in CMakeLists.txt calls it on macOS.

SOURCE_DIR=$1
SCAN_DIR=$2

if [ -z "$SOURCE_DIR" ] || [ -z "$SCAN_DIR" ]; then
    echo "usage: $0 <source_dir> <scan_build_dir>" >&2
    exit 1
fi

REPORT_DIR="$SCAN_DIR/report"
mkdir -p "$REPORT_DIR" || exit 2

# A fresh tree, so every file is compiled and analysed.
rm -rf "$SCAN_DIR/tree"

# other options: -analyze-headers --keep-empty -stats
scan-build cmake -S "$SOURCE_DIR" -B "$SCAN_DIR/tree" -G Ninja -DCMAKE_BUILD_TYPE=Debug || exit 3
scan-build -V -v -k --keep-empty -stats -o "$REPORT_DIR" cmake --build "$SCAN_DIR/tree" || exit 4
