#!/usr/bin/env bash
set -euo pipefail

# REG-7: `ebpm list`/`ebpm search`/`ebpm update` end to end against a real
# fake "index" and "library" bare repo (tagged v1.0.0/v1.2.0), with the app
# declaring `mylib = "^1.0"` directly (not via `ebpm add` - that's REG-6's
# own test) - proves list's dependency-tree output, search's index-browse
# output, and update's re-resolve-ignoring-the-pin behavior (both a no-op
# "already up to date" case and a real version-bump case), while confirming
# list still honors the lockfile pin the way an ordinary build would.
if [ "$#" -ne 3 ]; then
    echo "usage: run_list_search_update_case.sh <path-to-ebpm> <path-to-ebc> <case-dir>" >&2
    exit 2
fi

EBPM="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
EBC="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
CASE_DIR="$(cd "$3" && pwd)"

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

# --- Seed the library repo (v1.0.0 and v1.2.0) and an index offering only
# v1.0.0 at first. ---
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
description = "REG-7 list/search/update test package"

[[versions]]
version = "1.0.0"
git = "$LIB_REMOTE"
tag = "v1.0.0"
EOF
(
    cd "$INDEX_SEED"
    git checkout -q -B master
    git add -A
    git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "seed"
    git push -q origin master
)

export EBC="$EBC"
export EBASIC_INDEX_URL="$INDEX_REMOTE"
export HOME="$WORKDIR/fakehome"
mkdir -p "$HOME"

APP="$WORKDIR/app"
mkdir -p "$APP/src"
cp "$CASE_DIR/app/ebasic.toml" "$APP/ebasic.toml"
cp -r "$CASE_DIR/app/src/." "$APP/src/"

# 1. `ebpm search mylib` finds it; an unrelated term finds nothing.
if (cd "$APP" && "$EBPM" search mylib) >"$WORKDIR/search1.log" 2>&1; then
    if grep -qF "mylib - REG-7 list/search/update test package" "$WORKDIR/search1.log"; then
        check "search mylib finds the package with its description" 0
    else
        echo "FAIL: search mylib - unexpected output:"; cat "$WORKDIR/search1.log"; FAILED=1
    fi
else
    echo "FAIL: search mylib did not succeed:"; cat "$WORKDIR/search1.log"; FAILED=1
fi
if (cd "$APP" && "$EBPM" search nosuchterm) >"$WORKDIR/search2.log" 2>&1; then
    if grep -qF "no packages found matching 'nosuchterm'" "$WORKDIR/search2.log"; then
        check "search nosuchterm finds nothing" 0
    else
        echo "FAIL: search nosuchterm - unexpected output:"; cat "$WORKDIR/search2.log"; FAILED=1
    fi
else
    echo "FAIL: search nosuchterm did not succeed:"; cat "$WORKDIR/search2.log"; FAILED=1
fi

# 2. `ebpm list` shows the resolved registry dependency at v1.0.0, even
# before any build (no lockfile needed - resolveDependencyGraph alone).
if (cd "$APP" && "$EBPM" list) >"$WORKDIR/list1.log" 2>&1; then
    if grep -qF "mylib v1.0.0 (registry, commit " "$WORKDIR/list1.log"; then
        check "list shows mylib v1.0.0 before any build" 0
    else
        echo "FAIL: list (before build) - unexpected output:"; cat "$WORKDIR/list1.log"; FAILED=1
    fi
else
    echo "FAIL: list (before build) did not succeed:"; cat "$WORKDIR/list1.log"; FAILED=1
fi

# 3. A real build pins mylib at v1.0.0 in ebasic.lock.
if ! (cd "$APP" && "$EBPM" build) >"$WORKDIR/build1.log" 2>&1; then
    echo "FAIL: initial build did not succeed:"; cat "$WORKDIR/build1.log"; FAILED=1
fi

# 4. The index now also offers v1.2.0 - `list` must still show v1.0.0
# (the pin), matching REG-5's own reproducibility guarantee.
cat >"$INDEX_SEED/mylib.toml" <<EOF
[package]
name = "mylib"
description = "REG-7 list/search/update test package"

[[versions]]
version = "1.0.0"
git = "$LIB_REMOTE"
tag = "v1.0.0"

[[versions]]
version = "1.2.0"
git = "$LIB_REMOTE"
tag = "v1.2.0"
EOF
(cd "$INDEX_SEED" && git add -A && git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "add 1.2.0" && git push -q origin master)

if (cd "$APP" && "$EBPM" list) >"$WORKDIR/list2.log" 2>&1; then
    if grep -qF "mylib v1.0.0 (registry, commit " "$WORKDIR/list2.log"; then
        check "list stays pinned to v1.0.0 after a newer version appears in the index" 0
    else
        echo "FAIL: list (after index bump, before update) - unexpected output:"
        cat "$WORKDIR/list2.log"; FAILED=1
    fi
else
    echo "FAIL: list (after index bump) did not succeed:"; cat "$WORKDIR/list2.log"; FAILED=1
fi

# 5. `ebpm update mylib` re-resolves ignoring the pin, picks v1.2.0, and
# reports the transition.
if (cd "$APP" && "$EBPM" update mylib) >"$WORKDIR/update1.log" 2>&1; then
    if grep -qF "Updating mylib v1.0.0 -> v1.2.0" "$WORKDIR/update1.log"; then
        check "update mylib reports the v1.0.0 -> v1.2.0 transition" 0
    else
        echo "FAIL: update mylib - unexpected output:"; cat "$WORKDIR/update1.log"; FAILED=1
    fi
else
    echo "FAIL: update mylib did not succeed:"; cat "$WORKDIR/update1.log"; FAILED=1
fi

# 6. `list` (and a real run) now reflect v1.2.0.
if (cd "$APP" && "$EBPM" list) >"$WORKDIR/list3.log" 2>&1; then
    if grep -qF "mylib v1.2.0 (registry, commit " "$WORKDIR/list3.log"; then
        check "list shows mylib v1.2.0 after update" 0
    else
        echo "FAIL: list (after update) - unexpected output:"; cat "$WORKDIR/list3.log"; FAILED=1
    fi
else
    echo "FAIL: list (after update) did not succeed:"; cat "$WORKDIR/list3.log"; FAILED=1
fi
if (cd "$APP" && "$EBPM" run) >"$WORKDIR/run1.stdout" 2>"$WORKDIR/run1.log"; then
    if [ "$(cat "$WORKDIR/run1.stdout")" = "12" ]; then
        check "the app builds and runs against the updated mylib 1.2.0 -> 12" 0
    else
        echo "FAIL: expected program output '12', got '$(cat "$WORKDIR/run1.stdout")'"; FAILED=1
    fi
else
    echo "FAIL: ebpm run did not succeed after update:"; cat "$WORKDIR/run1.log"; FAILED=1
fi

# 7. `ebpm update` (no name) on an already-current dependency says so.
if (cd "$APP" && "$EBPM" update) >"$WORKDIR/update2.log" 2>&1; then
    if grep -qF "mylib is already up to date (v1.2.0)" "$WORKDIR/update2.log"; then
        check "update with no name reports 'already up to date' once current" 0
    else
        echo "FAIL: update (no name, already current) - unexpected output:"
        cat "$WORKDIR/update2.log"; FAILED=1
    fi
else
    echo "FAIL: update (no name) did not succeed:"; cat "$WORKDIR/update2.log"; FAILED=1
fi

# 8. `ebpm update <name-not-a-registry-dep>` fails clearly.
rc=0
(cd "$APP" && "$EBPM" update nosuchdep) >"$WORKDIR/update3.log" 2>&1 || rc=$?
if [ "$rc" -ne 0 ] && grep -qF "is not a registry dependency" "$WORKDIR/update3.log"; then
    check "update on a non-registry name fails clearly" 0
else
    echo "FAIL: update nosuchdep should have failed with 'is not a registry dependency', got (rc=$rc):"
    cat "$WORKDIR/update3.log"; FAILED=1
fi

exit "$FAILED"
