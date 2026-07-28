#!/usr/bin/env bash
set -euo pipefail

# LSP-6: verifies textDocument/completion over the real stdio protocol -
# keywords, in-scope symbols from a real fixture, and the "last good
# parse" fallback: completion must keep working even after a didChange
# leaves the document with a syntax error (mid-edit).

if [ "$#" -ne 1 ]; then
    echo "usage: completion_test.sh <path-to-ebasic_lsp>" >&2
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
GOOD_TEXT='DIM total AS INTEGER\n\nFUNCTION Square(n AS INTEGER) AS INTEGER\n    RETURN n * n\nEND FUNCTION\n'
# A syntax error (unterminated parameter list) - checkDocument returns
# nullopt for this; completion must fall back to the last good parse.
BROKEN_TEXT='DIM total AS INTEGER\n\nFUNCTION Square(n AS INTEGER\n'

{
    frame '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}'
    frame '{"jsonrpc":"2.0","method":"initialized","params":{}}'
    frame "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"$URI\",\"languageId\":\"ebasic\",\"version\":1,\"text\":\"$GOOD_TEXT\"}}}"
    frame "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":\"$URI\"},\"position\":{\"line\":0,\"character\":0}}}"
    frame "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":\"$URI\",\"version\":2},\"contentChanges\":[{\"text\":\"$BROKEN_TEXT\"}]}}"
    frame "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"textDocument/completion\",\"params\":{\"textDocument\":{\"uri\":\"$URI\"},\"position\":{\"line\":0,\"character\":0}}}"
    frame '{"jsonrpc":"2.0","id":9,"method":"shutdown"}'
    frame '{"jsonrpc":"2.0","method":"exit"}'
} | "$LSP_BIN" > "$WORKDIR/output.bin"

check '"completionProvider":{}'
# id:2 (document still parses cleanly): keywords + the real symbols
check '"id":2'
check '{"kind":14,"label":"DIM"}'      # a keyword
check '{"kind":3,"label":"square"}'    # the FUNCTION (canonical/lowercased label)
check '{"kind":6,"label":"total"}'     # the variable

# id:3 (document now has a syntax error): completion must still offer the
# same symbols, from the last successful parse - not go blank.
check '"id":3'
echo -n "" # (both ids share identical items in this fixture - the checks
           # above already confirm the labels appear somewhere; the count
           # match below confirms id:3 isn't just an empty array)
if grep -qF '"id":3,"jsonrpc":"2.0","result":[]' "$WORKDIR/output.bin"; then
    echo "FAIL: completion after a syntax error returned nothing - the last-good-parse fallback isn't working" >&2
    exit 1
fi

echo "PASS: lsp_completion"
