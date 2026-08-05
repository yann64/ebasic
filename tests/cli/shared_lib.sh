#!/usr/bin/env bash
# --shared-lib/-dll e2e test: builds tests/fixtures/shared_lib/mylib.bas
# into a real, dynamically loadable shared library, then loads it from a
# small C harness and calls its one real export - genuine proof of dynamic
# loadability (a real dlopen/dlsym, or LoadLibrary/GetProcAddress on
# Windows, round trip), not just "the file exists". On Windows/MinGW, also
# does a build-time link test directly against the generated import
# library (lib<name>.dll.a). Also exercises both Sema rejection paths (a
# STRING export; --lib + --shared-lib together).
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: shared_lib.sh <path-to-ebc>" >&2
    exit 2
fi

EBC="$1"
FIXTURE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../fixtures/shared_lib" && pwd)"
CC="${CC:-gcc}"

IS_WINDOWS=0
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) IS_WINDOWS=1 ;;
esac

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

# --- 1. build the real shared library, verify a genuine dynamic-load/call ---
if ! "$EBC" "$FIXTURE_DIR/mylib.bas" --shared-lib -o "$WORKDIR/mylib" >"$WORKDIR/compile.log" 2>&1; then
    echo "FAIL: mylib.bas did not compile with --shared-lib"
    cat "$WORKDIR/compile.log"
    exit 1
fi

if [ "$IS_WINDOWS" -eq 1 ]; then
    SHARED_LIB="$WORKDIR/mylib.dll"
    IMPORT_LIB="$WORKDIR/libmylib.dll.a"
    if [ ! -f "$IMPORT_LIB" ]; then
        echo "FAIL: expected import library not produced: $IMPORT_LIB"
        ls -la "$WORKDIR"
        exit 1
    fi
elif [ "$(uname -s)" = "Darwin" ]; then
    SHARED_LIB="$WORKDIR/libmylib.dylib"
else
    SHARED_LIB="$WORKDIR/libmylib.so"
fi
if [ ! -f "$SHARED_LIB" ]; then
    echo "FAIL: expected shared library not produced: $SHARED_LIB"
    ls -la "$WORKDIR"
    exit 1
fi

# dlopen/dlsym linkage varies by platform: glibc Linux needs an explicit
# -ldl (a real, separate library there); Haiku has no libdl at all -
# dlopen is built directly into libroot, and linking -ldl fails outright
# (confirmed live on real Haiku hardware: "cannot find -ldl"); Windows uses
# LoadLibrary/GetProcAddress instead (see harness.c), needing neither.
DL_LIBS=""
if [ "$(uname -s)" = "Linux" ]; then
    DL_LIBS="-ldl"
fi
if ! "$CC" "$FIXTURE_DIR/harness.c" -o "$WORKDIR/harness" $DL_LIBS 2>"$WORKDIR/cc.log"; then
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
echo "PASS: dynamic-load/call round trip ($ACTUAL)"

# --- 1b. Windows/MinGW only: a build-time link test against the generated
# import library - proves lib<name>.dll.a is genuinely usable at link
# time by another program, not just that the DLL exists on disk.
if [ "$IS_WINDOWS" -eq 1 ]; then
    if ! "$CC" "$FIXTURE_DIR/linktest.c" "$IMPORT_LIB" -o "$WORKDIR/linktest" 2>"$WORKDIR/linktest_cc.log"; then
        echo "FAIL: linktest.c did not compile/link against $IMPORT_LIB"
        cat "$WORKDIR/linktest_cc.log"
        exit 1
    fi
    # mylib.dll already sits alongside linktest.exe in $WORKDIR - Windows'
    # standard DLL search order checks the executable's own directory
    # first, so no PATH/copy juggling is needed.
    LINK_ACTUAL="$("$WORKDIR/linktest")"
    if [ "$LINK_ACTUAL" != "$EXPECTED" ]; then
        echo "FAIL: linktest output mismatch: got '$LINK_ACTUAL', expected '$EXPECTED'"
        exit 1
    fi
    echo "PASS: build-time link against the import library ($LINK_ACTUAL)"
fi

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
