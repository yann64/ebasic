#!/usr/bin/env bash
set -euo pipefail

# REG-3: exercises index.{hpp,cpp}'s fetch/cache/parse logic against a real
# local bare "index" repo this script creates - the same technique
# run_git_case.sh already uses for a fake git-dependency remote, applied to
# the index itself. Seeds one well-formed package (two versions) and one
# deliberately malformed one (missing its own `git` URL), matching what
# index_test.cpp itself expects.
if [ "$#" -ne 1 ]; then
    echo "usage: run_index_case.sh <path-to-index_test>" >&2
    exit 2
fi

INDEX_TEST="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"

WORKDIR="$(mktemp -d)"
trap 'rm -rf "$WORKDIR"' EXIT

REMOTE="$WORKDIR/index.git"
git init --bare -q "$REMOTE"

SEED="$WORKDIR/seed"
git clone -q "$REMOTE" "$SEED"
(
    cd "$SEED"
    git checkout -q -B master

    cat >goodpkg.toml <<'EOF'
[package]
name = "goodpkg"
description = "a good package"

[[versions]]
version = "1.0.0"
git = "https://example.invalid/goodpkg"
tag = "v1.0.0"

[[versions]]
version = "1.1.0"
git = "https://example.invalid/goodpkg"
tag = "v1.1.0"
EOF

    # Deliberately missing `git` on its one version entry - index.cpp
    # should reject it (lookupPackage fails outright; listAllPackages
    # skips it silently rather than failing the whole listing).
    cat >badpkg.toml <<'EOF'
[package]
name = "badpkg"
description = "a malformed package"

[[versions]]
version = "1.0.0"
EOF

    git add -A
    git -c user.email=ebpm-test@example.com -c user.name=ebpm-test commit -q -m "seed"
    git push -q origin master
)

# An isolated HOME so the index cache (~/.ebpm/cache/index/) and any
# ~/.ebpm/config.toml never touch the real user's home directory or a
# previous test run's cache.
export HOME="$WORKDIR/fakehome"
mkdir -p "$HOME"
export EBASIC_INDEX_URL="$REMOTE"

if ! "$INDEX_TEST"; then
    echo "FAIL: index_test reported failing checks"
    exit 1
fi

echo "PASS: index fetch/cache/parse"
