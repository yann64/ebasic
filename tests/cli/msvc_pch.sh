#!/usr/bin/env bash
# Real MSVC precompiled-header rule: deterministic proof (grep the exact
# backend-compiler invocation via --verbose, not a timing comparison) that
# `/Yu`/`/Fp` actually appear when ebc is told to use cl - see
# runtime/CMakeLists.txt's MSVC PCH block and main.cpp's
# runtimeIncludeArgs/msvcRuntimePchObjectPath. MSVC-only, no-op (PASS)
# everywhere else - mirrors cli/shared_lib.sh's own IS_WINDOWS-conditional
# pattern.
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: msvc_pch.sh <path-to-ebc>" >&2
    exit 2
fi

EBC="$1"
BUILD_DIR="$(cd "$(dirname "$EBC")/.." && pwd)"
PCH_FILE="$BUILD_DIR/runtime_pch/ebasic/runtime/runtime.pch"

if ! command -v cl >/dev/null 2>&1; then
    echo "PASS: msvc_pch (skipped - cl.exe not on PATH, nothing to verify)"
    exit 0
fi
if [ ! -f "$PCH_FILE" ]; then
    echo "PASS: msvc_pch (skipped - $PCH_FILE not built, nothing to verify)"
    exit 0
fi

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../e2e/hello_world" && pwd)"

if ! "$EBC" -cxx cl --verbose "$CASE_DIR/input.bas" \
        -o "$WORKDIR/hello.exe" >"$WORKDIR/out.log" 2>"$WORKDIR/verbose.log"; then
    echo "FAIL: msvc_pch - ebc -cxx cl failed to compile a trivial program:"
    cat "$WORKDIR/verbose.log"
    exit 1
fi

if ! grep -qF '/Yu' "$WORKDIR/verbose.log"; then
    echo "FAIL: msvc_pch - expected /Yu in the real cl.exe invocation, got:"
    cat "$WORKDIR/verbose.log"
    exit 1
fi
if ! grep -qF '/Fp' "$WORKDIR/verbose.log"; then
    echo "FAIL: msvc_pch - expected /Fp in the real cl.exe invocation, got:"
    cat "$WORKDIR/verbose.log"
    exit 1
fi

"$WORKDIR/hello.exe" >"$WORKDIR/actual.stdout"
if ! diff -u "$CASE_DIR/expected.stdout" "$WORKDIR/actual.stdout"; then
    echo "FAIL: msvc_pch - stdout mismatch"
    exit 1
fi

echo "PASS: msvc_pch"
