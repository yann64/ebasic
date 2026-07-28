#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 3 ] || [ "$#" -gt 5 ]; then
    echo "usage: run_case.sh <path-to-ebpm> <path-to-ebc> <case-dir> [<runnable-subdir>] [<ebpm-command>]" >&2
    exit 2
fi

EBPM="$1"
EBC="$2"
CASE_DIR="$3"
# Optional (M5c): for a multi-package fixture (a lib + an app depending on
# it, each its own subdirectory of CASE_DIR), the subdirectory to actually
# run ebpm from - defaults to CASE_DIR itself, matching a single-package
# fixture like simple_bin.
RUNNABLE_SUBDIR="${4:-.}"
# Optional (M5e): which ebpm subcommand to invoke - defaults to "run"
# (build-if-stale + execute the [bin] target); "test" exercises `ebpm test`
# instead (build+run every tests/*.bas, diffed the same way).
EBPM_COMMAND="${5:-run}"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

# Copy the fixture (everything except expected.stdout) into an isolated
# workdir, so ebpm's target/ build output never pollutes the checked-in
# fixture and repeated runs always start from a clean slate.
cp -r "$CASE_DIR"/. "$WORKDIR/"
rm -f "$WORKDIR/expected.stdout"

export EBC="$EBC"
ACTUAL="$WORKDIR/.actual.stdout"
RUN_LOG="$WORKDIR/.ebpm_run.log"

# ebpm's own build progress goes to stderr (matches Cargo's own convention -
# see build.cpp), so stdout here is exactly the built program's real output,
# nothing else, ready to diff directly against expected.stdout.
if ! (cd "$WORKDIR/$RUNNABLE_SUBDIR" && "$EBPM" "$EBPM_COMMAND") >"$ACTUAL" 2>"$RUN_LOG"; then
    echo "FAIL: $CASE_DIR did not build/run"
    cat "$RUN_LOG"
    exit 1
fi

if ! diff -u "$CASE_DIR/expected.stdout" "$ACTUAL"; then
    echo "FAIL: stdout mismatch for $CASE_DIR"
    exit 1
fi

echo "PASS: $CASE_DIR"
