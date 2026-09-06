#!/usr/bin/env bash
# A robust MSVC precompiled-header rule for --lib mode: --lib mode's one
# archived object is consumed by a *separate*, later ebc invocation - MSVC
# requires the PCH-creation companion object (runtime_pch.obj) be present
# in *that* later link whenever the archived object was compiled with
# /Yu. main.cpp's msvcRuntimePchObjectIfAvailable makes every exe/
# --shared-lib link defensively include that object whenever one exists,
# independent of whether *that* invocation's own compile used PCH - see
# its own doc comment. This test proves both halves live against real
# cl.exe: (1) a --lib archive built with PCH, consumed by a PCH-built exe,
# links and runs correctly (not just "it compiles"); (2) the same archive
# still links correctly even when the *consuming* exe's own compile falls
# back to no-PCH (a corrupted .pch), proving the defensive inclusion is
# genuinely independent of the current compile's own PCH usage. MSVC-only,
# no-op (PASS) everywhere else - mirrors cli/msvc_pch.sh's own pattern.
set -uo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: msvc_pch_lib_mode.sh <path-to-ebc>" >&2
    exit 2
fi

EBC="$1"
BUILD_DIR="$(cd "$(dirname "$EBC")/.." && pwd)"
PCH_FILE="$BUILD_DIR/runtime_pch/ebasic/runtime/runtime.pch"

if ! command -v cl >/dev/null 2>&1; then
    echo "PASS: msvc_pch_lib_mode (skipped - cl.exe not on PATH, nothing to verify)"
    exit 0
fi
if [ ! -f "$PCH_FILE" ]; then
    echo "PASS: msvc_pch_lib_mode (skipped - $PCH_FILE not built, nothing to verify)"
    exit 0
fi

WORKDIR="$(mktemp -d)"
BACKUP="$(mktemp)"
cp "$PCH_FILE" "$BACKUP"
cleanup() { cp "$BACKUP" "$PCH_FILE"; rm -f "$BACKUP"; rm -rf "$WORKDIR"; }
trap cleanup EXIT

FAILED=0

cat >"$WORKDIR/lib.bas" <<'EOF'
FUNCTION Twice(n AS INTEGER) AS INTEGER
    Twice = n * 2
END FUNCTION

FUNCTION AddOne(n AS INTEGER) AS INTEGER
    AddOne = n + 1
END FUNCTION
EOF

cat >"$WORKDIR/app.bas" <<'EOF'
#include "mathlib.iface.bas"

PRINT Twice(21)
PRINT AddOne(41)
EOF
printf '42\r\n42\r\n' >"$WORKDIR/expected.stdout"

# --- 1. build the --lib archive with PCH, confirm /Yu was used ---
if ! "$EBC" "$WORKDIR/lib.bas" --lib -o "$WORKDIR/mathlib" -cxx cl --verbose \
        >"$WORKDIR/lib.log" 2>"$WORKDIR/lib_verbose.log"; then
    echo "FAIL: msvc_pch_lib_mode - --lib build failed:"
    cat "$WORKDIR/lib_verbose.log"
    exit 1
fi
if ! grep -qF '/Yu' "$WORKDIR/lib_verbose.log"; then
    echo "FAIL: msvc_pch_lib_mode - expected /Yu in the --lib compile, got:"
    cat "$WORKDIR/lib_verbose.log"
    FAILED=1
fi

# --- 2. build a consuming exe (also PCH-built) linking against it ---
if ! "$EBC" "$WORKDIR/app.bas" -o "$WORKDIR/app1.exe" -cxx cl --verbose \
        -I "$WORKDIR" -L "$WORKDIR" >"$WORKDIR/app1.log" 2>"$WORKDIR/app1_verbose.log"; then
    echo "FAIL: msvc_pch_lib_mode - consuming exe failed to compile/link against a PCH-built --lib archive:"
    cat "$WORKDIR/app1_verbose.log"
    FAILED=1
else
    "$WORKDIR/app1.exe" >"$WORKDIR/app1.stdout"
    if ! diff -u "$WORKDIR/expected.stdout" "$WORKDIR/app1.stdout"; then
        echo "FAIL: msvc_pch_lib_mode - stdout mismatch for the PCH-built consuming exe"
        FAILED=1
    fi
fi

# --- 3. corrupt the real .pch, rebuild the same consuming exe: the
#        archive's own /Yu-compiled object still needs the companion
#        object, and this exe's own compile now falls back to no-PCH -
#        the defensive inclusion must not depend on either. ---
printf 'not a real pch file' >"$PCH_FILE"
if ! "$EBC" "$WORKDIR/app.bas" -o "$WORKDIR/app2.exe" -cxx cl \
        -I "$WORKDIR" -L "$WORKDIR" >"$WORKDIR/app2.log" 2>&1; then
    echo "FAIL: msvc_pch_lib_mode - consuming exe did not recover from a corrupted .pch while linking a PCH-built --lib archive:"
    cat "$WORKDIR/app2.log"
    FAILED=1
else
    "$WORKDIR/app2.exe" >"$WORKDIR/app2.stdout"
    if ! diff -u "$WORKDIR/expected.stdout" "$WORKDIR/app2.stdout"; then
        echo "FAIL: msvc_pch_lib_mode - stdout mismatch after PCH-fallback recovery"
        FAILED=1
    fi
fi

if [ "$FAILED" -eq 0 ]; then
    echo "PASS: msvc_pch_lib_mode"
fi
exit "$FAILED"
