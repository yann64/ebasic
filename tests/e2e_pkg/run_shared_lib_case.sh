#!/usr/bin/env bash
# ebpm-side e2e test for a [shared-lib] manifest target: `ebpm build` in a
# package declaring only [shared-lib] (no [bin] to `ebpm run`), then a real
# dlopen/dlsym/call round trip against the produced shared library, proving
# ebpm's own build step (not just ebc directly, already covered by
# tests/cli/shared_lib.sh) produces a genuinely loadable artifact.
set -euo pipefail

if [ "$#" -ne 4 ]; then
    echo "usage: run_shared_lib_case.sh <path-to-ebpm> <path-to-ebc> <case-dir> <harness.c>" >&2
    exit 2
fi

EBPM="$1"
EBC="$2"
CASE_DIR="$3"
HARNESS_C="$4"
CC="${CC:-gcc}"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

cp -r "$CASE_DIR"/. "$WORKDIR/"

export EBC="$EBC"
BUILD_LOG="$WORKDIR/.ebpm_build.log"
if ! (cd "$WORKDIR/mypkg" && "$EBPM" build) >"$BUILD_LOG" 2>&1; then
    echo "FAIL: $CASE_DIR did not build"
    cat "$BUILD_LOG"
    exit 1
fi

TARGET_DIR="$WORKDIR/mypkg/target"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) SHARED_LIB="$TARGET_DIR/mypkg.dll" ;;
    Darwin)                SHARED_LIB="$TARGET_DIR/libmypkg.dylib" ;;
    *)                     SHARED_LIB="$TARGET_DIR/libmypkg.so" ;;
esac
if [ ! -f "$SHARED_LIB" ]; then
    echo "FAIL: expected shared library not produced by 'ebpm build': $SHARED_LIB"
    ls -la "$TARGET_DIR"
    exit 1
fi

DL_LIBS=""
if [ "$(uname -s)" = "Linux" ]; then
    DL_LIBS="-ldl" # see tests/cli/shared_lib.sh's own comment - not needed/available elsewhere
fi
if ! "$CC" "$HARNESS_C" -o "$WORKDIR/harness" $DL_LIBS 2>"$WORKDIR/cc.log"; then
    echo "FAIL: harness did not compile"
    cat "$WORKDIR/cc.log"
    exit 1
fi

ACTUAL="$("$WORKDIR/harness" "$SHARED_LIB")"
EXPECTED="AddNumbers(3, 4) = 7"
if [ "$ACTUAL" != "$EXPECTED" ]; then
    echo "FAIL: harness output mismatch: got '$ACTUAL', expected '$EXPECTED'"
    exit 1
fi

echo "PASS: $CASE_DIR ($ACTUAL)"
