#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "usage: run_case.sh <path-to-ebc> <case-dir> [<fixture-lib-dir>]" >&2
    exit 2
fi

EBC="$1"
CASE_DIR="$2"
FIXTURE_LIB_DIR="${3:-}"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

BIN="$WORKDIR/prog"
COMPILE_LOG="$WORKDIR/compile.log"

EBC_ARGS=("$CASE_DIR/input.bas" -o "$BIN")
if [ -n "$FIXTURE_LIB_DIR" ]; then
    EBC_ARGS+=(-L "$FIXTURE_LIB_DIR")
fi

if ! "$EBC" "${EBC_ARGS[@]}" >"$COMPILE_LOG" 2>&1; then
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
