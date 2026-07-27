#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: run_case.sh <path-to-ebc> <case-dir>" >&2
    exit 2
fi

EBC="$1"
CASE_DIR="$2"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

BIN="$WORKDIR/prog"
COMPILE_LOG="$WORKDIR/compile.log"

if ! "$EBC" "$CASE_DIR/input.bas" -o "$BIN" >"$COMPILE_LOG" 2>&1; then
    echo "FAIL: $CASE_DIR did not compile"
    cat "$COMPILE_LOG"
    exit 1
fi

ACTUAL="$WORKDIR/actual.stdout"
"$BIN" >"$ACTUAL"

if ! diff -u "$CASE_DIR/expected.stdout" "$ACTUAL"; then
    echo "FAIL: stdout mismatch for $CASE_DIR"
    exit 1
fi

echo "PASS: $CASE_DIR"
