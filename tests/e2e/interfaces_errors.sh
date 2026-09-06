#!/usr/bin/env bash
set -uo pipefail

# M11 (multiple-interface implementation): every rejected shape, exercised
# via a real `ebc` compile - same "check_rejected" pattern as
# typed_function_pointer_errors.sh/generics_errors.sh. See e2e_interfaces
# for the accepted shapes.

if [ "$#" -ne 1 ]; then
    echo "usage: interfaces_errors.sh <path-to-ebc>" >&2
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

check_rejected "at most one non-interface (fielded) base is allowed" \
'TYPE IA
    Declare Virtual Sub Foo()
END TYPE

TYPE Base1
    x AS INTEGER
END TYPE

TYPE Base2
    y AS INTEGER
END TYPE

TYPE Widget EXTENDS Base1, Base2, IA
    Declare Virtual Sub Foo()
END TYPE

Virtual Sub Widget.Foo()
End Sub' \
    "TYPE 'Widget' names more than one non-interface base in EXTENDS ('base1' and 'Base2') - at most one ordinary base is allowed"

check_rejected "interface-of-interface surfaces as a real undefined-method error" \
'TYPE IClickable
    Declare Virtual Sub OnClick()
END TYPE

TYPE IResizable EXTENDS IClickable
    Declare Virtual Sub OnResize(w AS INTEGER, h AS INTEGER)
END TYPE' \
    "TYPE 'IResizable' declares method 'onresize' but never defines it"

check_rejected "an interface TYPE cannot cross an EXTERN boundary (already-existing rule, unaffected)" \
'TYPE IClickable
    Declare Virtual Sub OnClick()
END TYPE

Extern "C" Lib "somelib"
    Declare Sub eb_fixture_bad(ByVal c AS IClickable)
End Extern' \
    "has a constructor, destructor, or virtual method and is not C-ABI-compatible"

check_rejected "a TYPE that only declares zero-field interface EXTENDS but never implements it stays abstract (real backend error, not a silent miscompile)" \
'TYPE IClickable
    Declare Virtual Sub OnClick()
END TYPE

TYPE Widget EXTENDS IClickable
END TYPE

DIM w AS Widget' \
    "error"

exit "$FAILED"
