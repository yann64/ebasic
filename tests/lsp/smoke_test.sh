#!/usr/bin/env bash
set -euo pipefail

# Protocol-level smoke test for ebasic_lsp (LSP-1: transport + document
# sync). Speaks real Content-Length-framed JSON-RPC over the server's
# stdio - the same transport any real LSP client (Neovim, VS Code, ...)
# uses - by piping a fixed sequence of framed requests/notifications into
# one invocation and grepping the accumulated, compact-JSON response
# stream (nlohmann::json::dump()'s default output has no spaces around
# `:`/`,`) for the substrings each step must produce. This sandbox has no
# editor available to drive interactively, so this is the closest
# available substitute for real end-to-end verification, and doubles as a
# permanent regression test once an editor is used by hand.

if [ "$#" -ne 1 ]; then
    echo "usage: smoke_test.sh <path-to-ebasic_lsp>" >&2
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

{
    frame '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}}'
    frame '{"jsonrpc":"2.0","method":"initialized","params":{}}'
    frame '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///tmp/hello.bas","languageId":"ebasic","version":1,"text":"PRINT \"hi\"\n"}}}'
    frame '{"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///tmp/hello.bas","version":2},"contentChanges":[{"text":"PRINT \"hi again\"\n"}]}}'
    frame '{"jsonrpc":"2.0","method":"textDocument/didClose","params":{"textDocument":{"uri":"file:///tmp/hello.bas"}}}'
    # A made-up, never-real method name - all six real LSP slices (LSP-1
    # through LSP-6) are implemented now, so this deliberately isn't a real
    # LSP method at all, just to exercise MethodNotFound.
    frame '{"jsonrpc":"2.0","id":2,"method":"textDocument/notARealMethod","params":{}}'
    # A malformed request for an *implemented* method (missing the required
    # textDocument field) must get InvalidParams, not crash the server - a
    # real bug once hover started doing params.at("textDocument") with no
    # exception boundary around dispatch().
    frame '{"jsonrpc":"2.0","id":4,"method":"textDocument/hover","params":{}}'
    frame '{"jsonrpc":"2.0","id":3,"method":"shutdown"}'
    frame '{"jsonrpc":"2.0","method":"exit"}'
} > "$WORKDIR/input.bin"

set +e
"$LSP_BIN" < "$WORKDIR/input.bin" > "$WORKDIR/output.bin"
rc=$?
set -e
if [ "$rc" -ne 0 ]; then
    echo "FAIL: expected exit code 0 after a clean shutdown/exit, got $rc" >&2
    cat "$WORKDIR/output.bin" >&2
    exit 1
fi

check() {
    if ! grep -qF "$1" "$WORKDIR/output.bin"; then
        echo "FAIL: expected to find $1 in the response stream" >&2
        cat "$WORKDIR/output.bin" >&2
        exit 1
    fi
}
check '"textDocumentSync":1'
check '"name":"ebasic-lsp"'
check '"code":-32601'   # an unrecognized method name
check '"error":{"code":-32602'  # malformed hover params -> InvalidParams, not a crash
check '"id":4'                  # ... and it's the reply to that same request
check '"id":3'          # the shutdown response

# A client that disconnects without shutdown/exit (a crash, or an editor
# killed outright) must still exit 1, per the LSP spec's own exit-code
# contract.
set +e
"$LSP_BIN" < /dev/null > /dev/null
rc2=$?
set -e
if [ "$rc2" -ne 1 ]; then
    echo "FAIL: expected exit code 1 on an unclean disconnect, got $rc2" >&2
    exit 1
fi

echo "PASS: lsp_smoke"
