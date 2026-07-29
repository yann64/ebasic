#!/usr/bin/env bash
set -uo pipefail

# REG-2: a manifest's [dependencies] entry may now name a version
# requirement (a bare string shorthand, or { version = "..." }) as a third
# alternative to path/git - this checks every rejected combination
# (loadManifest's own new validation, exercised via a real `ebpm build`
# rather than any standalone parse-only tool, since none exists). Every
# variant here is a synthetic, throwaway manifest built inline rather than
# a checked-in fixture directory - there are many small, single-purpose
# error cases and no runnable program is ever involved.
if [ "$#" -ne 1 ]; then
    echo "usage: manifest_version_errors.sh <path-to-ebpm>" >&2
    exit 2
fi

EBPM="$1"
FAILED=0

# $1: a short label; $2: the [dependencies] table body; $3: an expected
# substring in ebpm build's stderr.
check_rejected() {
    local label="$1" depsBody="$2" expectedSubstring="$3"
    local dir out rc=0
    dir="$(mktemp -d)"
    cat >"$dir/ebasic.toml" <<EOF
[package]
name = "vertest"
version = "0.1.0"

[bin]
name = "vertest"
path = "src/main.bas"

[dependencies]
$depsBody
EOF
    mkdir -p "$dir/src"
    printf 'PRINT 1\n' >"$dir/src/main.bas"
    out="$(cd "$dir" && "$EBPM" build 2>&1)" || rc=$?
    rm -rf "$dir"
    if [ "$rc" -eq 0 ]; then
        echo "FAIL: $label - expected ebpm build to reject this manifest, but it exited 0: $out"
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

check_rejected "git + version both given" \
    'dep = { git = "https://example.invalid/dep.git", version = "^1.0" }' \
    "must name exactly one of \`path\`, \`git\`, or \`version\`"

check_rejected "path + version both given" \
    'dep = { path = "../dep", version = "^1.0" }' \
    "must name exactly one of \`path\`, \`git\`, or \`version\`"

check_rejected "neither path, git, nor version given" \
    'dep = { branch = "main" }' \
    "must name exactly one of \`path\`, \`git\`, or \`version\`"

check_rejected "version combined with branch" \
    'dep = { version = "^1.0", branch = "main" }' \
    "may not combine \`version\` with"

check_rejected "version combined with tag" \
    'dep = { version = "^1.0", tag = "v1.0.0" }' \
    "may not combine \`version\` with"

check_rejected "version combined with rev" \
    'dep = { version = "^1.0", rev = "abc123" }' \
    "may not combine \`version\` with"

check_rejected "malformed version requirement (bare-string shorthand)" \
    'dep = "not-a-version"' \
    "is not a valid version requirement"

check_rejected "malformed version requirement (table form)" \
    'dep = { version = "1.2.3.4" }' \
    "is not a valid version requirement"

check_rejected "an exact requirement must be a full triple" \
    'dep = { version = "=1.2" }' \
    "must give a full MAJOR.MINOR.PATCH version"

exit "$FAILED"
