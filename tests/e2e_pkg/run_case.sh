#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: run_case.sh <path-to-ebpm> <path-to-ebc> <case-dir>" >&2
    exit 2
fi

EBPM="$1"
EBC="$2"
CASE_DIR="$3"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

# Copy the fixture package (everything except expected.stdout) into an
# isolated workdir, so ebpm's target/ build output never pollutes the
# checked-in fixture and repeated runs always start from a clean slate.
cp -r "$CASE_DIR"/. "$WORKDIR/"
rm -f "$WORKDIR/expected.stdout"

export EBC="$EBC"
ACTUAL="$WORKDIR/.actual.stdout"
RUN_LOG="$WORKDIR/.ebpm_run.log"

# ebpm's own build progress goes to stderr (matches Cargo's own convention -
# see build.cpp), so stdout here is exactly the built program's real output,
# nothing else, ready to diff directly against expected.stdout.
if ! (cd "$WORKDIR" && "$EBPM" run) >"$ACTUAL" 2>"$RUN_LOG"; then
    echo "FAIL: $CASE_DIR did not build/run"
    cat "$RUN_LOG"
    exit 1
fi

if ! diff -u "$CASE_DIR/expected.stdout" "$ACTUAL"; then
    echo "FAIL: stdout mismatch for $CASE_DIR"
    exit 1
fi

echo "PASS: $CASE_DIR"
