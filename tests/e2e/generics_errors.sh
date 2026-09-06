#!/usr/bin/env bash
set -uo pipefail

# M10 (generics): every rejected shape, exercised via a real `ebc` compile -
# same "check_rejected" pattern as typed_function_pointer_errors.sh. See
# e2e_generics for the accepted shapes.

if [ "$#" -ne 1 ]; then
    echo "usage: generics_errors.sh <path-to-ebc>" >&2
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

check_rejected "a type parameter that no parameter uses directly cannot be inferred" \
'FUNCTION MakeZero(OF T) () AS T
    MakeZero = 0
END FUNCTION

PRINT MakeZero()' \
    "cannot infer type parameter 'T' for 'MakeZero' - no parameter uses it directly"

check_rejected "too few arguments to reach the type-parameter-typed one" \
'FUNCTION Second(OF T) (a AS INTEGER, b AS T) AS T
    Second = b
END FUNCTION

PRINT Second(1)' \
    "'Second' expects at least 2 argument(s) to infer its type parameter from"

check_rejected "a generic (OF ...) clause is rejected on a TYPE method's out-of-line definition" \
'TYPE Box
    x AS INTEGER
END TYPE

FUNCTION Box.Wrap(OF T) (v AS T) AS T
    Wrap = v
END FUNCTION' \
    "is a TYPE method - a generic (OF ...) type parameter is only supported on a top-level SUB/FUNCTION"

exit "$FAILED"
