#!/usr/bin/env bash
set -uo pipefail

# Default parameter values: every rejected shape, exercised via a real
# `ebc` compile rather than a standalone parse-only tool, matching
# tests/e2e_pkg/manifest_version_errors.sh's own "check_rejected" pattern -
# many small, single-purpose error cases, no runnable program involved.

if [ "$#" -ne 1 ]; then
    echo "usage: default_params_errors.sh <path-to-ebc>" >&2
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

check_rejected "non-defaulted parameter follows a defaulted one" \
'FUNCTION Bad(a AS INTEGER = 1, b AS INTEGER) AS INTEGER
    Bad = a + b
END FUNCTION' \
    "must have a default value (it follows a parameter that has one)"

check_rejected "call omits a required (non-defaulted) argument" \
'FUNCTION NeedsTwo(a AS INTEGER, b AS INTEGER) AS INTEGER
    NeedsTwo = a + b
END FUNCTION
PRINT NeedsTwo(1)' \
    "missing required argument 'b'"

check_rejected "call passes more arguments than declared" \
'FUNCTION AddWithDefault(a AS INTEGER, b AS INTEGER = 10) AS INTEGER
    AddWithDefault = a + b
END FUNCTION
PRINT AddWithDefault(1, 2, 3)' \
    "expected 2 argument(s), got 3"

check_rejected "a BYREF parameter cannot have a default value" \
'FUNCTION Greet(greeting AS STRING = "Hello") AS STRING
    Greet = greeting
END FUNCTION' \
    "a BYREF parameter cannot have a default value"

check_rejected "a default value must be a literal, not an arbitrary expression" \
'FUNCTION Foo(a AS INTEGER, b AS INTEGER = a) AS INTEGER
    Foo = a + b
END FUNCTION' \
    "a parameter's default value must be a literal"

exit "$FAILED"
