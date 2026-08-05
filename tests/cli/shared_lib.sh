#!/usr/bin/env bash
# --shared-lib/-dll e2e test: builds tests/fixtures/shared_lib/mylib.bas
# into a real, dynamically loadable shared library, then dlopen()s it from
# a small C harness and calls its one real export - genuine proof of
# dynamic loadability (a real dlopen/dlsym/call round trip), not just "the
# file exists". Also exercises both Sema rejection paths (a STRING export;
# --lib + --shared-lib together).
#
# Skipped on Windows/MinGW: dlopen/dlfcn.h don't exist there - Windows'
# own shared-lib verification (import-library link + LoadLibrary/
# GetProcAddress) is a separate, MinGW-specific test.
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: shared_lib.sh <path-to-ebc>" >&2
    exit 2
fi

EBC="$1"
FIXTURE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../fixtures/shared_lib" && pwd)"

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        echo "SKIP: shared_lib.sh (no dlopen/dlfcn.h on Windows/MinGW)"
        exit 0
        ;;
esac

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

# --- 1. build the real shared library, verify a genuine dlopen/dlsym/call ---
if ! "$EBC" "$FIXTURE_DIR/mylib.bas" --shared-lib -o "$WORKDIR/mylib" >"$WORKDIR/compile.log" 2>&1; then
    echo "FAIL: mylib.bas did not compile with --shared-lib"
    cat "$WORKDIR/compile.log"
    exit 1
fi

SHARED_LIB="$WORKDIR/libmylib.so"
if [ "$(uname -s)" = "Darwin" ]; then
    SHARED_LIB="$WORKDIR/libmylib.dylib"
fi
if [ ! -f "$SHARED_LIB" ]; then
    echo "FAIL: expected shared library not produced: $SHARED_LIB"
    ls -la "$WORKDIR"
    exit 1
fi

CC="${CC:-cc}"
if ! "$CC" "$FIXTURE_DIR/harness.c" -o "$WORKDIR/harness" -ldl 2>"$WORKDIR/cc.log"; then
    echo "FAIL: harness.c did not compile"
    cat "$WORKDIR/cc.log"
    exit 1
fi

ACTUAL="$("$WORKDIR/harness" "$SHARED_LIB")"
EXPECTED="AddNumbers(3, 4) = 7"
if [ "$ACTUAL" != "$EXPECTED" ]; then
    echo "FAIL: harness output mismatch: got '$ACTUAL', expected '$EXPECTED'"
    exit 1
fi
echo "PASS: dlopen/dlsym/call round trip ($ACTUAL)"

# --- 2. a STRING-typed export must be rejected ---
cat >"$WORKDIR/bad_string.bas" <<'EOF'
Extern "C"
    Function BadExport(name As String) As String
        BadExport = name
    End Function
End Extern
EOF
if "$EBC" "$WORKDIR/bad_string.bas" --shared-lib -o "$WORKDIR/bad_string" >"$WORKDIR/bad_string.log" 2>&1; then
    echo "FAIL: a STRING-typed export was accepted (should have been rejected)"
    exit 1
fi
if ! grep -q "cannot be STRING in an EXTERN/DECLARE signature" "$WORKDIR/bad_string.log"; then
    echo "FAIL: STRING-export rejection message not found:"
    cat "$WORKDIR/bad_string.log"
    exit 1
fi
echo "PASS: STRING-typed export rejected"

# --- 3. --lib and --shared-lib are mutually exclusive ---
if "$EBC" "$FIXTURE_DIR/mylib.bas" --lib --shared-lib -o "$WORKDIR/both" >"$WORKDIR/both.log" 2>&1; then
    echo "FAIL: --lib + --shared-lib together was accepted (should have been rejected)"
    exit 1
fi
if ! grep -q "mutually exclusive" "$WORKDIR/both.log"; then
    echo "FAIL: --lib/--shared-lib mutual-exclusivity message not found:"
    cat "$WORKDIR/both.log"
    exit 1
fi
echo "PASS: --lib + --shared-lib rejected as mutually exclusive"
