#!/usr/bin/env bash
set -uo pipefail

# ExitProcess terminates the program immediately with a given exit code -
# the standard e2e harness (run_case.sh) assumes a clean exit 0, so this
# one behavior (an intentionally nonzero, immediate exit) needs its own
# small script, matching default_params_errors.sh's own pattern.

if [ "$#" -ne 1 ]; then
    echo "usage: process_library_exitprocess.sh <path-to-ebc>" >&2
    exit 2
fi

EBC="$1"
FAILED=0
DIR="$(mktemp -d)"
trap 'rm -rf "$DIR"' EXIT

cat > "$DIR/input.bas" << 'EOF'
PRINT "before exit"
CALL ExitProcess(7)
PRINT "should never print"
EOF

if ! "$EBC" "$DIR/input.bas" -o "$DIR/prog" >"$DIR/compile.log" 2>&1; then
    echo "FAIL: ExitProcess program did not compile"
    cat "$DIR/compile.log"
    exit 1
fi

OUT="$("$DIR/prog")"
RC=$?

if [ "$RC" -ne 7 ]; then
    echo "FAIL: expected exit code 7, got $RC"
    FAILED=1
fi

if [ "$OUT" != "before exit" ]; then
    echo "FAIL: expected stdout 'before exit' (nothing after ExitProcess), got: $OUT"
    FAILED=1
fi

if [ "$FAILED" -eq 0 ]; then
    echo "PASS: ExitProcess terminates immediately with the given exit code"
fi

exit "$FAILED"
