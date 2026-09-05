#!/usr/bin/env bash
set -euo pipefail

# REG-6: `ebpm add`/`ebpm remove` end to end against a real fake "index"
# and "library" bare repo (tagged v1.0.0/v1.2.0) - proves the whole
# add-a-dependency-by-name pipeline: index lookup, highest-version and
# --version-constrained picks, manifest text editing (both directions),
# duplicate/not-found error handling, and that the resulting manifest
# entry actually builds and runs.
if [ "$#" -ne 3 ]; then
    echo "usage: run_add_remove_case.sh <path-to-ebpm> <path-to-ebc> <case-dir>" >&2
    exit 2
fi

EBPM="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
EBC="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
CASE_DIR="$(cd "$3" && pwd)"

# ebpm is a native (non-MSYS) executable, unlike git and the rest of this
# script - it gets none of the automatic POSIX-to-Windows path translation
# bash applies when *it* execs a native child (confirmed live: a bare
# "/tmp/..." path handed to ebpm's own child `git clone` came back "does
# not exist", since plain CreateProcess never translates argv strings).
# Any $WORKDIR-derived path that ends up inside a file ebpm itself reads
# (a manifest's git URL, EBASIC_INDEX_URL, HOME) needs to already be
# Windows-native before it gets there. cygpath only exists on MSYS/Cygwin;
# elsewhere (Linux/macOS/Haiku) paths are already native and this is a
# no-op.
native_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        printf '%s' "$1"
    fi
}

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

FAILED=0
check() {
    local label="$1" cond="$2"
    if [ "$cond" -eq 0 ]; then
        echo "PASS: $label"
    else
        echo "FAIL: $label"
        FAILED=1
    fi
}

# --- Seed the library repo (v1.0.0 and v1.2.0) and the index ---
LIB_REMOTE="$WORKDIR/lib.git"
git init --bare -q "$LIB_REMOTE"
LIB_SEED="$WORKDIR/lib_seed"
git clone -q "$LIB_REMOTE" "$LIB_SEED"
(
    cd "$LIB_SEED"
    git checkout -q -B master
    cp -r "$CASE_DIR/lib_seed_v1_0/." .
    git add -A
    git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "v1.0.0"
    git tag v1.0.0
    rm -rf ./*
    cp -r "$CASE_DIR/lib_seed_v1_2/." .
    git add -A
    git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "v1.2.0"
    git tag v1.2.0
    git push -q origin master v1.0.0 v1.2.0
)

INDEX_REMOTE="$WORKDIR/index.git"
git init --bare -q "$INDEX_REMOTE"
INDEX_SEED="$WORKDIR/index_seed"
git clone -q "$INDEX_REMOTE" "$INDEX_SEED"
cat >"$INDEX_SEED/mylib.toml" <<EOF
[package]
name = "mylib"
description = "REG-6 add/remove test package"

[[versions]]
version = "1.0.0"
git = "$(native_path "$LIB_REMOTE")"
tag = "v1.0.0"

[[versions]]
version = "1.2.0"
git = "$(native_path "$LIB_REMOTE")"
tag = "v1.2.0"
EOF
(
    cd "$INDEX_SEED"
    git checkout -q -B master
    git add -A
    git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "seed"
    git push -q origin master
)

export EBC="$EBC"
export EBASIC_INDEX_URL="$(native_path "$INDEX_REMOTE")"
HOME="$WORKDIR/fakehome"
mkdir -p "$HOME"
export HOME="$(native_path "$HOME")"

APP="$WORKDIR/app"
mkdir -p "$APP/src"
cp "$CASE_DIR/app/ebasic.toml" "$APP/ebasic.toml"
cp -r "$CASE_DIR/app/src/." "$APP/src/"

# 1. `ebpm add mylib` with no --version picks the highest (1.2.0).
if (cd "$APP" && "$EBPM" add mylib) >"$WORKDIR/add1.log" 2>&1; then
    if grep -qF 'mylib = "^1.2.0"' "$APP/ebasic.toml"; then
        check "add mylib (no --version) picks highest 1.2.0" 0
    else
        echo "FAIL: add mylib - ebasic.toml doesn't contain the expected entry:"
        cat "$APP/ebasic.toml"
        FAILED=1
    fi
else
    echo "FAIL: add mylib did not succeed:"
    cat "$WORKDIR/add1.log"
    FAILED=1
fi

# 2. Adding it again must fail (already a dependency).
rc=0
(cd "$APP" && "$EBPM" add mylib) >"$WORKDIR/add2.log" 2>&1 || rc=$?
if [ "$rc" -ne 0 ] && grep -qF "already a dependency" "$WORKDIR/add2.log"; then
    check "re-adding mylib fails with 'already a dependency'" 0
else
    echo "FAIL: re-adding mylib should have failed with 'already a dependency', got (rc=$rc):"
    cat "$WORKDIR/add2.log"
    FAILED=1
fi

# 3. The generated entry actually builds and runs (mylib 1.2.0 -> 12).
if (cd "$APP" && "$EBPM" run) >"$WORKDIR/run1.stdout" 2>"$WORKDIR/run1.log"; then
    if [ "$(cat "$WORKDIR/run1.stdout")" = "12" ]; then
        check "the added dependency builds and runs (mylib 1.2.0 -> 12)" 0
    else
        echo "FAIL: expected program output '12', got '$(cat "$WORKDIR/run1.stdout")'"
        FAILED=1
    fi
else
    echo "FAIL: ebpm run did not succeed after add:"
    cat "$WORKDIR/run1.log"
    FAILED=1
fi

# 4. `ebpm remove mylib` deletes the entry.
if (cd "$APP" && "$EBPM" remove mylib) >"$WORKDIR/remove1.log" 2>&1; then
    if grep -qF "mylib" "$APP/ebasic.toml"; then
        echo "FAIL: remove mylib - ebasic.toml still mentions mylib:"
        cat "$APP/ebasic.toml"
        FAILED=1
    else
        check "remove mylib deletes the entry" 0
    fi
else
    echo "FAIL: remove mylib did not succeed:"
    cat "$WORKDIR/remove1.log"
    FAILED=1
fi

# 5. Removing it again must fail (not a dependency).
rc=0
(cd "$APP" && "$EBPM" remove mylib) >"$WORKDIR/remove2.log" 2>&1 || rc=$?
if [ "$rc" -ne 0 ] && grep -qF "is not a dependency" "$WORKDIR/remove2.log"; then
    check "re-removing mylib fails with 'is not a dependency'" 0
else
    echo "FAIL: re-removing mylib should have failed with 'is not a dependency', got (rc=$rc):"
    cat "$WORKDIR/remove2.log"
    FAILED=1
fi

# 6. `--version` picks a specific, non-latest version. Note `^1.0` would
# NOT prove this - caret means >=1.0.0,<2.0.0, so 1.2.0 legitimately
# satisfies it too and is correctly the highest pick; `~1.0.0` (>=1.0.0,
# <1.1.0) is what actually excludes 1.2.0.
if (cd "$APP" && "$EBPM" add mylib --version "~1.0.0") >"$WORKDIR/add3.log" 2>&1; then
    if grep -qF 'mylib = "^1.0.0"' "$APP/ebasic.toml"; then
        check "add mylib --version '~1.0.0' picks 1.0.0, not the newer 1.2.0" 0
    else
        echo "FAIL: add mylib --version '~1.0.0' - ebasic.toml doesn't contain the expected entry:"
        cat "$APP/ebasic.toml"
        FAILED=1
    fi
else
    echo "FAIL: add mylib --version '~1.0.0' did not succeed:"
    cat "$WORKDIR/add3.log"
    FAILED=1
fi
(cd "$APP" && "$EBPM" remove mylib) >/dev/null 2>&1

# 7. Adding an unknown package name fails clearly.
rc=0
(cd "$APP" && "$EBPM" add nosuchpackage) >"$WORKDIR/add4.log" 2>&1 || rc=$?
if [ "$rc" -ne 0 ] && grep -qF "no package named" "$WORKDIR/add4.log"; then
    check "adding an unknown package name fails with 'no package named'" 0
else
    echo "FAIL: adding an unknown package should have failed with 'no package named', got (rc=$rc):"
    cat "$WORKDIR/add4.log"
    FAILED=1
fi

exit "$FAILED"
