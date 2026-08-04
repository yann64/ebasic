#!/usr/bin/env bash
set -uo pipefail

# UBound/LBound: every rejected shape, exercised via a real `ebc` compile
# rather than a standalone parse-only tool, matching
# tests/e2e_pkg/manifest_version_errors.sh's own "check_rejected" pattern.
# Unlike every other stdlib procedure, UBound/LBound are a compiler
# special form (Sema intercepts the call before normal array-index/
# procedure resolution) - their sole argument must be a bare array name,
# rejected at compile time otherwise, which is what this script covers.

if [ "$#" -ne 1 ]; then
    echo "usage: ubound_lbound_errors.sh <path-to-ebc>" >&2
    exit 2
fi

EBC="$1"
FAILED=0

# $1: a short label; $2: the .bas source; $3: an expected substring in
# ebc's stderr.
check_rejected() {
    local label="$1" source="$2" expectedSubstring="$3"
    local dir out rc=0
    dir="$(mktemp -d)"
    printf '%s\n' "$source" >"$dir/input.bas"
    out="$(cd "$dir" && "$EBC" input.bas -o out 2>&1)" || rc=$?
    rm -rf "$dir"
    if [ "$rc" -eq 0 ]; then
        echo "FAIL: $label - expected ebc to reject this program, but it exited 0: $out"
        FAILED=1
        return
    fi
    if ! printf '%s\n' "$out" | grep -qF "$expectedSubstring"; then
        echo "FAIL: $label - expected stderr to contain '$expectedSubstring', got: $out"
        FAILED=1
        return
    fi
    echo "PASS: $label"
}

check_rejected "UBound of an arbitrary expression" \
'PRINT UBound(3 + 4)' \
    "'UBound' requires an array name, not an expression"

check_rejected "UBound of a non-array variable" \
'DIM x AS INTEGER
PRINT UBound(x)' \
    "'UBound' requires an array name, not an expression"

check_rejected "UBound of an undeclared name" \
'PRINT UBound(nosuch)' \
    "'UBound' requires an array name, not an expression"

check_rejected "UBound with more than one argument" \
'DIM arr(5) AS INTEGER
PRINT UBound(arr, 1)' \
    "'UBound' takes exactly one argument (an array name)"

check_rejected "UBound with zero arguments" \
'PRINT UBound()' \
    "'UBound' takes exactly one argument (an array name)"

check_rejected "LBound of a non-array variable" \
'DIM x AS INTEGER
PRINT LBound(x)' \
    "'LBound' requires an array name, not an expression"

exit "$FAILED"
