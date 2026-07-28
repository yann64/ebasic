#!/usr/bin/env bash
set -euo pipefail

# LSP-4: verifies textDocument/definition and textDocument/references over
# the real stdio protocol against a fixture with a FUNCTION called twice
# and a variable assigned once, read once.

if [ "$#" -ne 1 ]; then
    echo "usage: definition_references_test.sh <path-to-ebasic_lsp>" >&2
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
check_absent() {
    local needle="$1"
    if grep -qF "$needle" "$WORKDIR/output.bin"; then
        echo "FAIL: expected NOT to find $needle in the response stream" >&2
        cat "$WORKDIR/output.bin" >&2
        exit 1
    fi
}

URI="file://$WORKDIR/prog.bas"
# line 0: DIM total AS INTEGER          ("total" at columns 4-9)
# line 2: FUNCTION Square(n AS INTEGER) AS INTEGER
# line 6: total = Square(5)             ("total" at 0-5, "Square" at 8-14)
# line 7: PRINT Square(total)           ("Square" at 6-12, "total" at 13-18)
TEXT='DIM total AS INTEGER\n\nFUNCTION Square(n AS INTEGER) AS INTEGER\n    RETURN n * n\nEND FUNCTION\n\ntotal = Square(5)\nPRINT Square(total)\n'

{
    frame '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
    frame '{"jsonrpc":"2.0","method":"initialized","params":{}}'
    frame "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"$URI\",\"languageId\":\"ebasic\",\"version\":1,\"text\":\"$TEXT\"}}}"
    # go-to-definition on the "Square" call site in "total = Square(5)"
    frame "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/definition\",\"params\":{\"textDocument\":{\"uri\":\"$URI\"},\"position\":{\"line\":6,\"character\":9}}}"
    # all references to "Square", including its own declaration
    frame "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/references\",\"params\":{\"textDocument\":{\"uri\":\"$URI\"},\"position\":{\"line\":6,\"character\":9},\"context\":{\"includeDeclaration\":true}}}"
    # references to "total", excluding its own declaration
    frame "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"textDocument/references\",\"params\":{\"textDocument\":{\"uri\":\"$URI\"},\"position\":{\"line\":0,\"character\":4},\"context\":{\"includeDeclaration\":false}}}"
    frame '{"jsonrpc":"2.0","id":9,"method":"shutdown"}'
    frame '{"jsonrpc":"2.0","method":"exit"}'
} | "$LSP_BIN" > "$WORKDIR/output.bin"

check '"definitionProvider":true'
check '"referencesProvider":true'

# definition lands on FUNCTION Square's own declaration (line 2)
check '"id":2,"jsonrpc":"2.0","result":{"range":{"end":{"character":1,"line":2},"start":{"character":0,"line":2}}'

# references(Square, includeDeclaration=true): both call sites + the declaration
check '{"end":{"character":9,"line":6},"start":{"character":8,"line":6}}'  # "Square(5)"
check '{"end":{"character":7,"line":7},"start":{"character":6,"line":7}}'  # "Square(total)"
check '{"end":{"character":1,"line":2},"start":{"character":0,"line":2}}'  # the declaration itself

# references(total, includeDeclaration=false): the assignment target + the read, never the DIM
check '{"end":{"character":1,"line":6},"start":{"character":0,"line":6}}'   # "total = ..."
check '{"end":{"character":14,"line":7},"start":{"character":13,"line":7}}' # "Square(total)"
check_absent '{"end":{"character":9,"line":0},"start":{"character":4,"line":0}}' # DIM's own declLoc, excluded

echo "PASS: lsp_definition_references"
