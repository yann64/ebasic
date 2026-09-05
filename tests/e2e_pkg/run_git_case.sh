#!/usr/bin/env bash
set -euo pipefail

# M5d: exercises the real `git clone`/`fetch`/`checkout` code path against a
# local bare repo the test itself creates (no real-network dependency) -
# unlike run_case.sh's static fixture copy, a git-dependency fixture needs a
# real remote URL substituted in at test-run time, since the bare repo's
# path only exists once this script creates it.
if [ "$#" -ne 3 ]; then
    echo "usage: run_git_case.sh <path-to-ebpm> <path-to-ebc> <case-dir>" >&2
    exit 2
fi

# Resolved to absolute paths up front - the rest of this script `cd`s
# around (into the seed repo, then the app package), so a relative path
# given on the command line would otherwise stop resolving correctly.
EBPM="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
EBC="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
CASE_DIR="$(cd "$3" && pwd)"

# ebpm is a native (non-MSYS) executable, unlike git and the rest of this
# script - it gets none of the automatic POSIX-to-Windows path translation
# bash applies when *it* execs a native child (confirmed live: a bare
# "/tmp/..." path handed to ebpm's own child `git clone` came back "does
# not exist", since plain CreateProcess never translates argv strings).
# Any $WORKDIR-derived path that ends up inside a file ebpm itself reads
# (the manifest's git URL, HOME) needs to already be Windows-native before
# it gets there. cygpath only exists on MSYS/Cygwin; elsewhere (Linux/
# macOS/Haiku) paths are already native and this is a no-op.
native_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$1"
    else
        printf '%s' "$1"
    fi
}

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

# A bare "remote" repo, seeded from lib_seed/ via a throwaway clone -
# explicitly named "master" regardless of the machine's configured default
# branch name, matching the fixture's own ebasic.toml.in.
REMOTE="$WORKDIR/remote.git"
git init --bare -q "$REMOTE"

SEED="$WORKDIR/seed"
git clone -q "$REMOTE" "$SEED"
cp -r "$CASE_DIR/lib_seed/." "$SEED/"
(
    cd "$SEED"
    git checkout -q -B master
    git add -A
    git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "seed"
    git push -q origin master
)

# The app package, with the real bare-repo path substituted for @GIT_URL@.
APP="$WORKDIR/app"
mkdir -p "$APP/src"
sed "s#@GIT_URL@#$(native_path "$REMOTE")#" "$CASE_DIR/app/ebasic.toml.in" >"$APP/ebasic.toml"
cp -r "$CASE_DIR/app/src/." "$APP/src/"

# An isolated HOME so the git dependency cache (~/.ebpm/cache/git/) never
# touches the real user's home directory or a previous test run's cache.
HOME="$WORKDIR/fakehome"
mkdir -p "$HOME"
export HOME="$(native_path "$HOME")"
export EBC="$EBC"

ACTUAL="$WORKDIR/.actual.stdout"
RUN_LOG="$WORKDIR/.ebpm_run.log"

if ! (cd "$APP" && "$EBPM" run) >"$ACTUAL" 2>"$RUN_LOG"; then
    echo "FAIL: $CASE_DIR did not build/run"
    cat "$RUN_LOG"
    exit 1
fi

if ! diff -u "$CASE_DIR/expected.stdout" "$ACTUAL"; then
    echo "FAIL: stdout mismatch for $CASE_DIR"
    exit 1
fi

echo "PASS: $CASE_DIR"
