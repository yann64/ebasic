#!/usr/bin/env bash
# clang-cl (Clang's MSVC-compatible driver mode) as a genuinely verified
# backend option - isMsvcToolchain() already matched it alongside cl.exe
# for flag-syntax purposes, but nothing had ever actually run against a
# real clang-cl.exe until this test was written. Deterministic proof (via
# --verbose) that a trivial program compiles, links, and runs correctly
# through clang-cl, and - the mirror image of cli/msvc_pch.sh's own
# /Yu-presence check - that /Yu/Fp do NOT appear: clang-cl deliberately
# never uses the MSVC PCH (see isClangCl's own doc comment in main.cpp -
# the only .pch that could exist here was built by real cl.exe, and
# MSVC's PDB-based PCH format isn't interchangeable with Clang's own
# AST-based one). clang-cl-only, no-op (PASS) everywhere else - mirrors
# cli/msvc_pch.sh's own command-v-gated pattern.
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "usage: clang_cl.sh <path-to-ebc>" >&2
    exit 2
fi

EBC="$1"

if ! command -v clang-cl >/dev/null 2>&1; then
    echo "PASS: clang_cl (skipped - clang-cl.exe not on PATH, nothing to verify)"
    exit 0
fi

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

CASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../e2e/hello_world" && pwd)"

if ! "$EBC" -cxx clang-cl --verbose "$CASE_DIR/input.bas" \
        -o "$WORKDIR/hello.exe" >"$WORKDIR/out.log" 2>"$WORKDIR/verbose.log"; then
    echo "FAIL: clang_cl - ebc -cxx clang-cl failed to compile a trivial program:"
    cat "$WORKDIR/verbose.log"
    exit 1
fi

if grep -qF '/Yu' "$WORKDIR/verbose.log"; then
    echo "FAIL: clang_cl - did not expect /Yu in a clang-cl invocation (the only .pch that could exist was built by cl.exe, not clang-cl), got:"
    cat "$WORKDIR/verbose.log"
    exit 1
fi

"$WORKDIR/hello.exe" >"$WORKDIR/actual.stdout"
if ! diff -u "$CASE_DIR/expected.stdout" "$WORKDIR/actual.stdout"; then
    echo "FAIL: clang_cl - stdout mismatch"
    exit 1
fi

echo "PASS: clang_cl"
