#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "usage: run_case.sh <path-to-docgen> <case-dir>" >&2
    exit 2
fi

DOCGEN="$1"
CASE_DIR="$2"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

if ! "$DOCGEN" "$CASE_DIR/input.bas" -o "$WORKDIR/out" >"$WORKDIR/docgen.log" 2>&1; then
    echo "FAIL: $CASE_DIR did not run through docgen"
    cat "$WORKDIR/docgen.log"
    exit 1
fi

# Only the Markdown output is golden-diffed - the simpler, more stable of
# the two formats (see the M7 plan: HTML output is hand-verified instead,
# since text-level HTML is more prone to trivial, non-meaningful
# formatting differences that would make a golden diff brittle).
if ! diff -u "$CASE_DIR/expected.md" "$WORKDIR/out/index.md"; then
    echo "FAIL: index.md mismatch for $CASE_DIR"
    exit 1
fi

echo "PASS: $CASE_DIR"
