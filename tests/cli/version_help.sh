#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: version_help.sh <path-to-ebc> <path-to-ebpm> <path-to-docgen>" >&2
    exit 2
fi

EBC="$1"
EBPM="$2"
DOCGEN="$3"

FAILED=0

check_version() {
    local tool_path="$1" tool_name="$2" flag="$3"
    local out rc=0
    out="$("$tool_path" "$flag" 2>&1)" || rc=$?
    if [ "$rc" -ne 0 ]; then
        echo "FAIL: $tool_name $flag exited $rc (expected 0): $out"
        FAILED=1
        return
    fi
    if ! printf '%s\n' "$out" | grep -qE "^${tool_name} [0-9]+\.[0-9]+\.[0-9]+"; then
        echo "FAIL: $tool_name $flag output did not match '^${tool_name} X.Y.Z': $out"
        FAILED=1
        return
    fi
    echo "PASS: $tool_name $flag -> $out"
}

check_help() {
    local tool_path="$1" tool_name="$2" flag="$3"
    local out rc=0
    out="$("$tool_path" "$flag" 2>&1)" || rc=$?
    if [ "$rc" -ne 0 ]; then
        echo "FAIL: $tool_name $flag exited $rc (expected 0): $out"
        FAILED=1
        return
    fi
    if ! printf '%s\n' "$out" | grep -qE "^usage: ${tool_name}"; then
        echo "FAIL: $tool_name $flag output did not start with 'usage: ${tool_name}': $out"
        FAILED=1
        return
    fi
    echo "PASS: $tool_name $flag"
}

## Deliberately not "$path:$name" combined-string + split, unlike an
## earlier version of this script - a Windows path (e.g.
## D:/a/ebasic/.../ebc.exe) contains a drive-letter colon of its own,
## which broke the split (extracted just "D" as the path, a real bug
## only surfaced by real Windows CI, not visible on Linux/macOS where
## paths never contain a colon).
run_checks() {
    local path="$1" name="$2"
    check_version "$path" "$name" "-v"
    check_version "$path" "$name" "--version"
    check_help "$path" "$name" "-h"
    check_help "$path" "$name" "--help"
}

run_checks "$EBC" "ebc"
run_checks "$EBPM" "ebpm"
run_checks "$DOCGEN" "docgen"

exit "$FAILED"
