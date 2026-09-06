#!/usr/bin/env bash
set -uo pipefail

# Typed function-pointer EXTERN/DECLARE parameters: every rejected shape,
# exercised via a real `ebc` compile - same "check_rejected" pattern as
# default_params_errors.sh. Proves the type checking is real structural
# checking (arity, parameter types, return type, calling convention), not
# just "it compiles" - see e2e_typed_function_pointers for the accepted
# shapes.

if [ "$#" -ne 1 ]; then
    echo "usage: typed_function_pointer_errors.sh <path-to-ebc>" >&2
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

check_rejected "@ProcName rejected: wrong parameter type" \
'FUNCTION Compare(BYVAL a AS INTEGER, BYVAL b AS INTEGER) AS INTEGER
    RETURN 0
END FUNCTION

DIM cb AS FUNCTION (BYVAL AS DOUBLE, BYVAL AS DOUBLE) AS INTEGER
cb = @Compare' \
    "does not match variable 'cb'"

check_rejected "@ProcName rejected: wrong arity" \
'FUNCTION Compare(BYVAL a AS INTEGER, BYVAL b AS INTEGER) AS INTEGER
    RETURN 0
END FUNCTION

DIM cb AS FUNCTION (BYVAL AS INTEGER) AS INTEGER
cb = @Compare' \
    "does not match variable 'cb'"

check_rejected "@ProcName rejected: SUB assigned where a FUNCTION pointer is expected" \
'SUB DoNothing(BYVAL a AS INTEGER)
END SUB

DIM cb AS FUNCTION (BYVAL AS INTEGER) AS INTEGER
cb = @DoNothing' \
    "does not match variable 'cb'"

check_rejected "STRING inside a function-pointer parameter is rejected at the EXTERN boundary" \
'Declare Sub eb_fixture_bad Cdecl (ByVal cb AS SUB (BYVAL AS STRING))' \
    "cannot be STRING in an EXTERN/DECLARE signature"

check_rejected "STRING as a function-pointer return type is rejected at the EXTERN boundary" \
'Declare Sub eb_fixture_bad Cdecl (ByVal cb AS FUNCTION () AS STRING)' \
    "cannot be STRING in an EXTERN/DECLARE signature"

check_rejected "calling a SUB-shaped function pointer in an expression is rejected" \
'SUB DoNothing(BYVAL a AS INTEGER)
END SUB

DIM cb AS SUB (BYVAL AS INTEGER)
cb = @DoNothing
PRINT cb(5)' \
    "is a SUB-shaped function pointer and cannot be used in an expression"

check_rejected "calling a non-function-pointer variable as a statement is still rejected the old way" \
'DIM x AS INTEGER
x = 5
CALL x(1, 2)' \
    "is not a declared SUB or FUNCTION"

check_rejected "calling a non-function-pointer TYPE field is rejected with a clear diagnostic" \
'TYPE Foo
    x AS INTEGER
END TYPE

DIM f AS Foo
PRINT f.x(1, 2)' \
    "is a field, not a method or a function pointer, and cannot be called"

check_rejected "calling a non-function-pointer PROPERTY is rejected with a clear diagnostic" \
'TYPE Thermometer
    cTemp AS SINGLE

    Declare Constructor()
    Declare Property Celsius() AS SINGLE
    Declare Property Celsius(BYVAL value AS SINGLE)
END TYPE

Constructor Thermometer()
    cTemp = 0
End Constructor

Property Thermometer.Celsius() AS SINGLE
    Celsius = cTemp
End Property

Property Thermometer.Celsius(BYVAL value AS SINGLE)
    cTemp = value
End Property

DIM t AS Thermometer
PRINT t.Celsius(1, 2)' \
    "is a PROPERTY, not a method or a function pointer, and cannot be called"

check_rejected "Stdcall is rejected on a TYPE method's out-of-line definition" \
'TYPE Point
    x AS INTEGER
    Declare Sub SetX(BYVAL v AS INTEGER)
END TYPE

SUB Point.SetX Stdcall (BYVAL v AS INTEGER)
    This.x = v
END SUB' \
    "is a TYPE method - STDCALL is only supported on a top-level SUB/FUNCTION"

exit "$FAILED"
