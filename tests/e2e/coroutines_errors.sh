#!/usr/bin/env bash
set -uo pipefail

# M12 (coroutines): every rejected shape, exercised via a real `ebc`
# compile - same "check_rejected" pattern as generics_errors.sh/
# interfaces_errors.sh. See e2e_coroutines for the accepted shapes.

if [ "$#" -ne 1 ]; then
    echo "usage: coroutines_errors.sh <path-to-ebc>" >&2
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

check_rejected "an Async FUNCTION must return TASK/GENERATOR" \
'FUNCTION Foo() Async AS INTEGER
    RETURN 5
END FUNCTION' \
    "Async FUNCTION 'Foo' must return TASK(OF T) or GENERATOR(OF T)"

check_rejected "returning TASK/GENERATOR requires Async" \
'FUNCTION Foo() AS Task(OF INTEGER)
    RETURN 5
END FUNCTION' \
    "FUNCTION 'Foo' returns TASK/GENERATOR but is not marked Async"

check_rejected "AWAIT is rejected at the top level (compiles into main, which C++ forbids from being a coroutine)" \
'FUNCTION MakeTask() Async AS Task(OF INTEGER)
    RETURN 5
END FUNCTION

DIM x AS INTEGER
x = AWAIT MakeTask()' \
    "AWAIT can only be used inside an Async SUB/FUNCTION"

check_rejected "AWAIT is rejected inside an ordinary (non-Async) FUNCTION too" \
'FUNCTION MakeTask() Async AS Task(OF INTEGER)
    RETURN 5
END FUNCTION

FUNCTION Ordinary() AS INTEGER
    Ordinary = AWAIT MakeTask()
END FUNCTION' \
    "AWAIT can only be used inside an Async SUB/FUNCTION"

check_rejected "RETURN cannot have a value inside a GENERATOR" \
'FUNCTION Gen() Async AS Generator(OF INTEGER)
    RETURN 5
END FUNCTION' \
    "RETURN cannot have a value inside a GENERATOR - use YIELD, or a bare RETURN to end the generator early"

check_rejected "YIELD is rejected outside a GENERATOR-shaped Async FUNCTION" \
'FUNCTION MakeTask() Async AS Task(OF INTEGER)
    YIELD 5
    RETURN 0
END FUNCTION' \
    "YIELD is only valid inside an Async FUNCTION returning GENERATOR(OF T)"

check_rejected "AWAIT is rejected on a GENERATOR (pulled via MoveNext/Current instead)" \
'FUNCTION Gen() Async AS Generator(OF INTEGER)
    YIELD 1
END FUNCTION

FUNCTION User() Async AS Task(OF INTEGER)
    DIM g AS Generator(OF INTEGER)
    g = Gen()
    RETURN AWAIT g
END FUNCTION' \
    "AWAIT requires a TASK(OF T) expression (not GENERATOR, and not a valueless TASK)"

check_rejected "the FuncName = value convention is rejected inside an Async FUNCTION" \
'FUNCTION MakeIt() Async AS Task(OF INTEGER)
    MakeIt = 5
END FUNCTION' \
    "'MakeIt' is an Async FUNCTION - use RETURN, not the FuncName = value convention, to produce a value"

check_rejected "a TASK cannot cross an EXTERN boundary" \
'Extern "C" Lib "somelib"
    Declare Sub eb_fixture_bad(ByVal t AS Task(OF INTEGER))
End Extern' \
    "cannot be TASK/GENERATOR in an EXTERN/DECLARE signature"

exit "$FAILED"
