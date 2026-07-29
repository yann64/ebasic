#!/usr/bin/env bash
set -euo pipefail

# REG-4: a real end-to-end registry dependency - a fake "index" bare repo
# and a fake "library" bare repo (tagged v1.0.0/v2.0.0), proving the
# resolver's index lookup + SemVer matching + synthetic git resolution all
# work together for real. Two scenarios share the same seeded lib/index:
#   1. app/ (regmathlib = "^1.0") builds and runs, printing "1" - proving
#      the caret requirement correctly excludes the v2.0.0 major bump.
#   2. app2/ + wrapper/ (a diamond: app2 wants regmathlib ^1.0 directly and
#      ^2.0 transitively via wrapper) fails with the expected conflict
#      error - proving the non-backtracking resolver's conflict detection.
if [ "$#" -ne 3 ]; then
    echo "usage: run_registry_case.sh <path-to-ebpm> <path-to-ebc> <case-dir>" >&2
    exit 2
fi

EBPM="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
EBC="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
CASE_DIR="$(cd "$3" && pwd)"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

FAILED=0

# The library repo itself, tagged v1.0.0 and v2.0.0 (a major bump, so a
# caret "^1.0" requirement can only ever pick the former).
LIB_REMOTE="$WORKDIR/lib.git"
git init --bare -q "$LIB_REMOTE"
LIB_SEED="$WORKDIR/lib_seed"
git clone -q "$LIB_REMOTE" "$LIB_SEED"
(
    cd "$LIB_SEED"
    git checkout -q -B master
    cp -r "$CASE_DIR/lib_seed_v1/." .
    git add -A
    git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "v1.0.0"
    git tag v1.0.0
    rm -rf ./*
    cp -r "$CASE_DIR/lib_seed_v2/." .
    git add -A
    git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "v2.0.0"
    git tag v2.0.0
    git push -q origin master
    git push -q origin v1.0.0 v2.0.0
)

# The index repo, with one entry pointing at the library repo above.
INDEX_REMOTE="$WORKDIR/index.git"
git init --bare -q "$INDEX_REMOTE"
INDEX_SEED="$WORKDIR/index_seed"
git clone -q "$INDEX_REMOTE" "$INDEX_SEED"
sed "s#@LIB_GIT_URL@#$LIB_REMOTE#" "$CASE_DIR/index_seed/regmathlib.toml.in" >"$INDEX_SEED/regmathlib.toml"
(
    cd "$INDEX_SEED"
    git checkout -q -B master
    git add -A
    git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "seed"
    git push -q origin master
)

export EBC="$EBC"
export EBASIC_INDEX_URL="$INDEX_REMOTE"
# An isolated HOME so the index/git dependency caches never touch the real
# user's home directory or a previous test run's cache.
export HOME="$WORKDIR/fakehome"
mkdir -p "$HOME"

# Scenario 1: happy path.
APP="$WORKDIR/app"
mkdir -p "$APP/src"
cp "$CASE_DIR/app/ebasic.toml" "$APP/ebasic.toml"
cp -r "$CASE_DIR/app/src/." "$APP/src/"

ACTUAL="$WORKDIR/.actual.stdout"
RUN_LOG="$WORKDIR/.ebpm_run.log"
if ! (cd "$APP" && "$EBPM" run) >"$ACTUAL" 2>"$RUN_LOG"; then
    echo "FAIL: happy-path scenario did not build/run"
    cat "$RUN_LOG"
    FAILED=1
elif ! diff -u "$CASE_DIR/expected.stdout" "$ACTUAL"; then
    echo "FAIL: happy-path scenario stdout mismatch"
    FAILED=1
else
    echo "PASS: happy-path scenario (regmathlib ^1.0 resolves to 1.0.0, excludes 2.0.0)"
fi

# Scenario 2: conflicting requirements for the same registry package name.
APP2="$WORKDIR/app2"
WRAPPER="$WORKDIR/wrapper"
mkdir -p "$APP2/src" "$WRAPPER/src"
cp "$CASE_DIR/app2/ebasic.toml" "$APP2/ebasic.toml"
cp -r "$CASE_DIR/app2/src/." "$APP2/src/"
cp "$CASE_DIR/wrapper/ebasic.toml" "$WRAPPER/ebasic.toml"
cp -r "$CASE_DIR/wrapper/src/." "$WRAPPER/src/"

CONFLICT_LOG="$WORKDIR/.ebpm_conflict.log"
conflictRc=0
(cd "$APP2" && "$EBPM" build) >"$CONFLICT_LOG" 2>&1 || conflictRc=$?
if [ "$conflictRc" -eq 0 ]; then
    echo "FAIL: conflict scenario - expected ebpm build to fail, but it exited 0"
    FAILED=1
elif ! grep -qF "no version of 'regmathlib' satisfies both" "$CONFLICT_LOG"; then
    echo "FAIL: conflict scenario - expected the conflict error message, got:"
    cat "$CONFLICT_LOG"
    FAILED=1
else
    echo "PASS: conflict scenario (incompatible ^1.0/^2.0 requirements rejected clearly)"
fi

exit "$FAILED"
