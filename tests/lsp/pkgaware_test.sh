#!/usr/bin/env bash
set -euo pipefail

# LSP-5: verifies ebpm-awareness end to end, reusing the real
# tests/e2e_pkg/lib_and_app fixture (a [lib] package + a [bin] package
# depending on it via #include "mathlib.iface.bas") - both the graceful
# "dependency not built yet" diagnostic path, and the real, correct
# diagnostics/hover/go-to-definition behavior once the dependency actually
# is built via a real `ebpm build`.

if [ "$#" -ne 3 ]; then
    echo "usage: pkgaware_test.sh <path-to-ebasic_lsp> <path-to-ebpm> <path-to-ebc>" >&2
    exit 2
fi
LSP_BIN="$1"
EBPM="$2"
EBC="$3"

FIXTURE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../e2e_pkg/lib_and_app" && pwd)"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT
cp -r "$FIXTURE_DIR"/. "$WORKDIR/"
rm -f "$WORKDIR/expected.stdout"

frame() {
    local body="$1"
    printf 'Content-Length: %d\r\n\r\n%s' "${#body}" "$body"
}
check() {
    local file="$1" needle="$2"
    if ! grep -qF "$needle" "$file"; then
        echo "FAIL: expected to find $needle in $file" >&2
        cat "$file" >&2
        exit 1
    fi
}

# Converts a real, on-disk path to the "file://" URI a real client would
# send - resolves symlinks (`cd "$dir" && pwd -P`, mirroring fs::canonical)
# and, on MSYS2/Windows, converts to a real native path via `cygpath -w`
# first: a plain posix-style path like "/tmp/xyz" isn't something a native
# (non-msys) Win32 program can resolve via real file APIs at all - package
# detection (findPackageRoot's own fs::exists check) would otherwise fail
# silently, exactly as real Windows CI caught (see
# tests/lsp/diagnostics_test.sh's own roadmap notes for the first time this
# exact issue was found).
to_file_uri() {
    local dir base resolved
    dir="$(dirname "$1")"
    base="$(basename "$1")"
    resolved="$(cd "$dir" && pwd -P)/$base"
    if command -v cygpath >/dev/null 2>&1; then
        resolved="$(cygpath -w "$resolved")"
        resolved="${resolved//\\//}"
        printf 'file:///%s' "$resolved"
    else
        printf 'file://%s' "$resolved"
    fi
}

MAIN_URI="$(to_file_uri "$WORKDIR/myapp/src/main.bas")"
# line 0: #include "mathlib.iface.bas"
# line 2: PRINT Twice(21)          ("Twice" at columns 6-11)
MAIN_TEXT='#include \"mathlib.iface.bas\"\n\nPRINT Twice(21)\nPRINT AddOne(41)\n'

# --- Scenario A: mathlib not built yet - a graceful, actionable hint ------
{
    frame '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
    frame '{"jsonrpc":"2.0","method":"initialized","params":{}}'
    frame "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"$MAIN_URI\",\"languageId\":\"ebasic\",\"version\":1,\"text\":\"$MAIN_TEXT\"}}}"
    frame '{"jsonrpc":"2.0","id":9,"method":"shutdown"}'
    frame '{"jsonrpc":"2.0","method":"exit"}'
} | "$LSP_BIN" > "$WORKDIR/out_unbuilt.bin"

check "$WORKDIR/out_unbuilt.bin" "cannot open included file 'mathlib.iface.bas'"
check "$WORKDIR/out_unbuilt.bin" "dependency 'mathlib' hasn't been built yet; run \`ebpm build\`"

# --- Build the real dependency, for real, via a real ebpm invocation ------
if ! (cd "$WORKDIR/mathlib" && EBC="$EBC" "$EBPM" build) >"$WORKDIR/ebpm_build.log" 2>&1; then
    echo "FAIL: ebpm build (mathlib) failed" >&2
    cat "$WORKDIR/ebpm_build.log" >&2
    exit 1
fi

# --- Scenario B: mathlib built - real diagnostics/hover/go-to-definition --
{
    frame '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
    frame '{"jsonrpc":"2.0","method":"initialized","params":{}}'
    frame "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"$MAIN_URI\",\"languageId\":\"ebasic\",\"version\":1,\"text\":\"$MAIN_TEXT\"}}}"
    frame "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"$MAIN_URI\"},\"position\":{\"line\":2,\"character\":8}}}"
    frame "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/definition\",\"params\":{\"textDocument\":{\"uri\":\"$MAIN_URI\"},\"position\":{\"line\":2,\"character\":8}}}"
    frame '{"jsonrpc":"2.0","id":9,"method":"shutdown"}'
    frame '{"jsonrpc":"2.0","method":"exit"}'
} | "$LSP_BIN" > "$WORKDIR/out_built.bin"

check "$WORKDIR/out_built.bin" '"diagnostics":[]' # the #include now resolves cleanly
# hover on the "Twice" call site shows mathlib's own real, resolved signature
check "$WORKDIR/out_built.bin" '"value":"FUNCTION Twice(BYVAL n AS INTEGER) AS INTEGER"'
# go-to-definition lands in mathlib's own generated interface file, not main.bas
check "$WORKDIR/out_built.bin" 'mathlib/target/mathlib.iface.bas'
check "$WORKDIR/out_built.bin" '"id":3,"jsonrpc":"2.0","result":{"range":{"end":{"character":5,"line":3},"start":{"character":4,"line":3}}'

echo "PASS: lsp_pkgaware"
