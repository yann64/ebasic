#!/usr/bin/env bash
set -euo pipefail

# LSP-3: verifies textDocument/documentSymbol and textDocument/hover over
# the real stdio protocol against a small fixture with a top-level FUNCTION
# and a module-level DIM.

if [ "$#" -ne 1 ]; then
    echo "usage: symbols_test.sh <path-to-ebasic_lsp>" >&2
    exit 2
fi
LSP_BIN="$1"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

frame() {
    local body="$1"
    printf 'Content-Length: %d\r\n\r\n%s' "${#body}" "$body"
}

check() {
    local needle="$1"
    if ! grep -qF "$needle" "$WORKDIR/output.bin"; then
        echo "FAIL: expected to find $needle in the response stream" >&2
        cat "$WORKDIR/output.bin" >&2
        exit 1
    fi
}

URI="file://$WORKDIR/prog.bas"
TEXT='DIM total AS INTEGER\n\nFUNCTION Square(n AS INTEGER) AS INTEGER\n    RETURN n * n\nEND FUNCTION\n\ntotal = Square(5)\nPRINT total\n'

{
    frame '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
    frame '{"jsonrpc":"2.0","method":"initialized","params":{}}'
    frame "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"$URI\",\"languageId\":\"ebasic\",\"version\":1,\"text\":\"$TEXT\"}}}"
    frame "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/documentSymbol\",\"params\":{\"textDocument\":{\"uri\":\"$URI\"}}}"
    # position of "Square" inside "total = Square(5)" (line 6, 0-based)
    frame "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"$URI\"},\"position\":{\"line\":6,\"character\":9}}}"
    # position of "total" inside "DIM total AS INTEGER" (line 0, 0-based)
    frame "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"$URI\"},\"position\":{\"line\":0,\"character\":4}}}"
    # a position with no identifier at all - must reply with a null result, not an error
    frame "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"textDocument/hover\",\"params\":{\"textDocument\":{\"uri\":\"$URI\"},\"position\":{\"line\":1,\"character\":0}}}"
    frame '{"jsonrpc":"2.0","id":9,"method":"shutdown"}'
    frame '{"jsonrpc":"2.0","method":"exit"}'
} | "$LSP_BIN" > "$WORKDIR/output.bin"

check '"documentSymbolProvider":true'
check '"hoverProvider":true'
check '"name":"Square"'
check '"kind":12' # SymbolKind.Function
check '"value":"FUNCTION Square(BYVAL n AS INTEGER) AS INTEGER"'
check '"value":"DIM total AS INTEGER"'
check '"id":5,"jsonrpc":"2.0","result":null'

echo "PASS: lsp_symbols"
