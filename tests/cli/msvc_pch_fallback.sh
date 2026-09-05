#!/usr/bin/env bash
# Real MSVC precompiled-header rule's resilience guarantee: unlike GCC's
# own silent, graceful fallback when a .gch doesn't match, a stale/
# mismatched MSVC .pch is a hard compile error (C1010/C1083/C2859) -
# runCompilerStepWithPchFallback (main.cpp) retries once without the PCH
# flags on exactly that failure. Proves it end to end: deliberately
# corrupt the real, already-built .pch, confirm ebc still compiles and
# runs a trivial program correctly, then restore the file (a shared
# build-tree artifact other tests rely on). MSVC-only, no-op (PASS)
# everywhere else - mirrors cli/shared_lib.sh's own IS_WINDOWS-conditional
# pattern.
set -uo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: msvc_pch_fallback.sh <path-to-ebc>" >&2
    exit 2
fi

EBC="$1"
BUILD_DIR="$(cd "$(dirname "$EBC")/.." && pwd)"
PCH_FILE="$BUILD_DIR/runtime_pch/ebasic/runtime/runtime.pch"

if ! command -v cl >/dev/null 2>&1; then
    echo "PASS: msvc_pch_fallback (skipped - cl.exe not on PATH, nothing to verify)"
    exit 0
fi
if [ ! -f "$PCH_FILE" ]; then
    echo "PASS: msvc_pch_fallback (skipped - $PCH_FILE not built, nothing to verify)"
    exit 0
fi

BACKUP="$(mktemp)"
cp "$PCH_FILE" "$BACKUP"
WORKDIR="$(mktemp -d)"
cleanup() { cp "$BACKUP" "$PCH_FILE"; rm -f "$BACKUP"; rm -rf "$WORKDIR"; }
trap cleanup EXIT

# A corrupted .pch (real content replaced with garbage bytes, same file
# still present) is exactly the "stale/mismatched" case cl.exe reports as
# C1010/C1083/C2859 - not a missing file (which would just skip the /Yu
# flags at the ebc level entirely, never reaching cl.exe's own mismatch
# detection at all).
printf 'not a real pch file' >"$PCH_FILE"

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../e2e/hello_world" && pwd)"

FAILED=0
if ! "$EBC" -cxx cl "$CASE_DIR/input.bas" -o "$WORKDIR/hello.exe" >"$WORKDIR/compile.log" 2>&1; then
    echo "FAIL: msvc_pch_fallback - ebc did not recover from a corrupted .pch:"
    cat "$WORKDIR/compile.log"
    FAILED=1
else
    "$WORKDIR/hello.exe" >"$WORKDIR/actual.stdout"
    if ! diff -u "$CASE_DIR/expected.stdout" "$WORKDIR/actual.stdout"; then
        echo "FAIL: msvc_pch_fallback - stdout mismatch after PCH-fallback recovery"
        FAILED=1
    fi
fi

if [ "$FAILED" -eq 0 ]; then
    echo "PASS: msvc_pch_fallback"
fi
exit "$FAILED"
