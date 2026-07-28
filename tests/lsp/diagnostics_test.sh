#!/usr/bin/env bash
set -euo pipefail

# LSP-2: verifies textDocument/publishDiagnostics end to end over the real
# stdio protocol - a Sema error surfacing with the right message/severity,
# a didChange that fixes it clearing the diagnostic, and a diagnostic
# inside an #include'd file being published against *that* file's own URI
# rather than the including document's.

if [ "$#" -ne 1 ]; then
    echo "usage: diagnostics_test.sh <path-to-ebasic_lsp>" >&2
    exit 2
fi
LSP_BIN="$1"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

frame() {
    # $1 = a single-line JSON body (no embedded newlines).
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

# --- Scenario 1: a real Sema error, then a didChange that fixes it --------
MAIN_URI="file://$WORKDIR/bad.bas"
{
    frame '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
    frame '{"jsonrpc":"2.0","method":"initialized","params":{}}'
    frame "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"$MAIN_URI\",\"languageId\":\"ebasic\",\"version\":1,\"text\":\"DIM x AS INTEGER\\nx = \\\"oops\\\"\\n\"}}}"
    frame "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":\"$MAIN_URI\",\"version\":2},\"contentChanges\":[{\"text\":\"DIM x AS INTEGER\\nx = 5\\nPRINT x\\n\"}]}}"
    frame '{"jsonrpc":"2.0","id":9,"method":"shutdown"}'
    frame '{"jsonrpc":"2.0","method":"exit"}'
} | "$LSP_BIN" > "$WORKDIR/out1.bin"

check "$WORKDIR/out1.bin" "$MAIN_URI"
check "$WORKDIR/out1.bin" 'cannot assign a string value'
check "$WORKDIR/out1.bin" '"severity":1'
check "$WORKDIR/out1.bin" '"diagnostics":[]' # the didChange's fix clearing it

# --- Scenario 2: a diagnostic inside an #include'd file -------------------
cat > "$WORKDIR/lib.bas" << 'EOF'
FUNCTION Bad() AS INTEGER
    RETURN "oops"
END FUNCTION
EOF
LIB_URI="file://$WORKDIR/lib.bas"
MAIN2_URI="file://$WORKDIR/main.bas"
{
    frame '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
    frame '{"jsonrpc":"2.0","method":"initialized","params":{}}'
    frame "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"$MAIN2_URI\",\"languageId\":\"ebasic\",\"version\":1,\"text\":\"#include \\\"lib.bas\\\"\\nPRINT Bad()\\n\"}}}"
    frame '{"jsonrpc":"2.0","id":9,"method":"shutdown"}'
    frame '{"jsonrpc":"2.0","method":"exit"}'
} | "$LSP_BIN" > "$WORKDIR/out2.bin"

check "$WORKDIR/out2.bin" "\"uri\":\"$LIB_URI\""
check "$WORKDIR/out2.bin" "does not match the FUNCTION's declared return type"
# nlohmann::json::dump() orders object keys alphabetically ("diagnostics" < "uri").
check "$WORKDIR/out2.bin" "\"diagnostics\":[],\"uri\":\"$MAIN2_URI\""

echo "PASS: lsp_diagnostics"
