#!/usr/bin/env bash
set -euo pipefail

# REG-5: proves the lockfile's reproducibility guarantee for a registry
# dependency - a repeat build stays pinned to its originally-resolved
# version even after the index/upstream gains a newer compatible release,
# a pinned rebuild needs no index consultation at all (the real proof: it
# still succeeds once EBASIC_INDEX_URL is pointed at nothing), and editing
# the manifest's own requirement to something the pin no longer satisfies
# picks up the newer version on the very next build.
if [ "$#" -ne 3 ]; then
    echo "usage: run_registry_lock_case.sh <path-to-ebpm> <path-to-ebc> <case-dir>" >&2
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

# --- Seed the library repo with only v1.0.0 for now ---
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
    git push -q origin master v1.0.0
)

# --- Seed the index repo with only the v1.0.0 entry for now ---
INDEX_REMOTE="$WORKDIR/index.git"
git init --bare -q "$INDEX_REMOTE"
INDEX_SEED="$WORKDIR/index_seed"
git clone -q "$INDEX_REMOTE" "$INDEX_SEED"
cat >"$INDEX_SEED/mylib.toml" <<EOF
[package]
name = "mylib"
description = "REG-5 lockfile-reuse test package"

[[versions]]
version = "1.0.0"
git = "$(native_path "$LIB_REMOTE")"
tag = "v1.0.0"
EOF
(
    cd "$INDEX_SEED"
    git checkout -q -B master
    git add -A
    git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "v1.0.0 only"
    git push -q origin master
)

export EBC="$EBC"
HOME="$WORKDIR/fakehome"
mkdir -p "$HOME"
export HOME="$(native_path "$HOME")"

APP="$WORKDIR/app"
mkdir -p "$APP/src"
cp "$CASE_DIR/app/ebasic.toml" "$APP/ebasic.toml"
cp -r "$CASE_DIR/app/src/." "$APP/src/"

# Only one [[package]] entry ever exists in this fixture (mylib is the
# app's sole dependency), so a plain whole-file grep for the `version`
# field is unambiguous - no need to scope to a particular [[package]]
# block.
lock_version() { grep '^version = ' "$APP/ebasic.lock" | sed 's/version = "\(.*\)"/\1/'; }

# --- Build 1: fresh resolution, should pick 1.0.0 (the only version) ---
export EBASIC_INDEX_URL="$(native_path "$INDEX_REMOTE")"
if ! (cd "$APP" && "$EBPM" build) >"$WORKDIR/build1.log" 2>&1; then
    echo "FAIL: build 1 (fresh resolution) did not succeed"
    cat "$WORKDIR/build1.log"
    FAILED=1
elif [ "$(lock_version)" != "1.0.0" ]; then
    echo "FAIL: build 1 - expected ebasic.lock to pin version 1.0.0, got '$(lock_version)'"
    FAILED=1
else
    echo "PASS: build 1 resolves fresh to 1.0.0"
fi

# --- The library and index both gain a newer, still-compatible v1.1.0 ---
(
    cd "$LIB_SEED"
    rm -rf ./*
    cp -r "$CASE_DIR/lib_seed_v1_1/." .
    git add -A
    git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "v1.1.0"
    git tag v1.1.0
    git push -q origin master v1.1.0
)
cat >"$INDEX_SEED/mylib.toml" <<EOF
[package]
name = "mylib"
description = "REG-5 lockfile-reuse test package"

[[versions]]
version = "1.0.0"
git = "$(native_path "$LIB_REMOTE")"
tag = "v1.0.0"

[[versions]]
version = "1.1.0"
git = "$(native_path "$LIB_REMOTE")"
tag = "v1.1.0"
EOF
(
    cd "$INDEX_SEED"
    git add -A
    git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "add v1.1.0"
    git push -q origin master
)

# --- Build 2: repeat build, same manifest requirement - must stay pinned
# at 1.0.0, not silently upgrade to the newer compatible 1.1.0. ---
if ! (cd "$APP" && "$EBPM" build) >"$WORKDIR/build2.log" 2>&1; then
    echo "FAIL: build 2 (repeat build) did not succeed"
    cat "$WORKDIR/build2.log"
    FAILED=1
elif [ "$(lock_version)" != "1.0.0" ]; then
    echo "FAIL: build 2 - a repeat build must stay pinned at 1.0.0, got '$(lock_version)'"
    FAILED=1
else
    echo "PASS: build 2 stays pinned at 1.0.0 despite a newer compatible 1.1.0 existing"
fi

# --- Build 3: same as build 2, but the index is now unreachable - proves
# a pinned rebuild needs no index consultation at all. ---
export EBASIC_INDEX_URL="$WORKDIR/does-not-exist.git"
if ! (cd "$APP" && "$EBPM" build) >"$WORKDIR/build3.log" 2>&1; then
    echo "FAIL: build 3 (pinned rebuild with an unreachable index) did not succeed"
    cat "$WORKDIR/build3.log"
    FAILED=1
elif [ "$(lock_version)" != "1.0.0" ]; then
    echo "FAIL: build 3 - expected to stay pinned at 1.0.0, got '$(lock_version)'"
    FAILED=1
else
    echo "PASS: build 3 succeeds fully offline (no index consultation needed once pinned)"
fi

# --- Build 4: restore the real index, edit the manifest's own requirement
# to ^1.1 (which the 1.0.0 pin no longer satisfies) - must re-resolve to
# 1.1.0, proving a manifest edit always takes effect. ---
export EBASIC_INDEX_URL="$(native_path "$INDEX_REMOTE")"
# A portable redirect-based edit, not `sed -i` - BSD sed (macOS's default)
# requires an explicit (even if empty) backup-suffix argument after `-i`,
# unlike GNU sed, so a bare `-i` is parsed as consuming the next argument
# (the script text itself) as that suffix and fails outright. Matches this
# project's own existing convention (see run_git_case.sh's templating).
sed 's/mylib = "\^1.0"/mylib = "^1.1"/' "$APP/ebasic.toml" >"$APP/ebasic.toml.new"
mv "$APP/ebasic.toml.new" "$APP/ebasic.toml"
if ! (cd "$APP" && "$EBPM" build) >"$WORKDIR/build4.log" 2>&1; then
    echo "FAIL: build 4 (requirement bumped to ^1.1) did not succeed"
    cat "$WORKDIR/build4.log"
    FAILED=1
elif [ "$(lock_version)" != "1.1.0" ]; then
    echo "FAIL: build 4 - expected ebasic.lock to now pin version 1.1.0, got '$(lock_version)'"
    FAILED=1
elif [ "$("$APP/target/lockapp")" != "11" ]; then
    echo "FAIL: build 4 - expected the program to print 11 (mylib 1.1.0's RegValue), got '$("$APP/target/lockapp")'"
    FAILED=1
else
    echo "PASS: build 4 re-resolves to 1.1.0 once the manifest requirement no longer fits the pin"
fi

exit "$FAILED"
