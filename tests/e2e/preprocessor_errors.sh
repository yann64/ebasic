#!/usr/bin/env bash
set -uo pipefail

# #error/#assert/malformed-directive rejections, exercised via a real `ebc`
# compile - matching tests/e2e/default_params_errors.sh's own
# "check_rejected" pattern.

if [ "$#" -ne 1 ]; then
    echo "usage: preprocessor_errors.sh <path-to-ebc>" >&2
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

check_rejected "#error stops compilation with the given message" \
'#error something went wrong
PRINT "unreachable"' \
    "something went wrong"

check_rejected "#assert reports the failing expression's own text" \
'#define X 5
#assert X = 6
PRINT "unreachable"' \
    "assertion failed: X = 6"

check_rejected "#elseif without a matching #if is rejected" \
'#elseif 1
PRINT "unreachable"' \
    "#elseif without matching #if"

check_rejected "#endmacro without a matching #macro is rejected" \
'#endmacro
PRINT "unreachable"' \
    "#endmacro without matching #macro"

check_rejected "an unclosed #macro is rejected" \
'#macro Foo()
PRINT "unreachable"' \
    "missing #endmacro"

check_rejected "too many arguments to a non-variadic macro is rejected" \
'#define Add(a, b) ((a) + (b))
PRINT Add(1, 2, 3)' \
    "too many arguments to macro 'Add'"

exit "$FAILED"
