#!/usr/bin/env bash
# M8e: rebuilds and runs the full e2e suite on a real Haiku machine over
# SSH, since GitHub's official self-hosted Actions runner has no Haiku
# build (only macOS/Linux/Windows) and Haiku has no dotnet/mono to run one
# even if it did - true native CI integration isn't feasible with standard
# tooling. This is the honest, current substitute: a rerunnable script a
# maintainer runs by hand whenever they want to re-check Haiku, not
# something triggered automatically on every push (see
# docs/architecture/roadmap.md's M8e notes for the full reasoning).
#
# Transfers the current commit (via `git archive`, not the working
# directory - so this always tests exactly what's committed, matching what
# CI would see) to a fresh directory on the remote box, configures, builds,
# and runs ctest there.
set -euo pipefail

if [ "$#" -gt 1 ]; then
    echo "usage: haiku_verify.sh [ssh-host]  (default: haiku - set up an" >&2
    echo "  SSH config Host alias for your own Haiku box if it isn't already)" >&2
    exit 2
fi

HOST="${1:-haiku}"
REMOTE_DIR="ebasic-verify-$(date +%s)"

echo "==> Checking SSH connectivity to '$HOST'..."
if ! ssh -o BatchMode=yes -o ConnectTimeout=10 "$HOST" 'uname -a'; then
    echo "error: could not reach '$HOST' - check your SSH config/keys" >&2
    exit 1
fi

echo "==> Transferring the current commit (HEAD) to ~/$REMOTE_DIR on $HOST..."
ssh "$HOST" "mkdir -p ~/$REMOTE_DIR"
git archive HEAD | ssh "$HOST" "tar -x -C ~/$REMOTE_DIR"

echo "==> Configuring (haiku preset)..."
ssh "$HOST" "cd ~/$REMOTE_DIR && cmake --preset haiku"

echo "==> Building..."
ssh "$HOST" "cd ~/$REMOTE_DIR && cmake --build --preset haiku"

echo "==> Running the e2e suite..."
if ssh "$HOST" "cd ~/$REMOTE_DIR/build/haiku && ctest --output-on-failure"; then
    echo "==> PASSED on $HOST (commit $(git rev-parse --short HEAD))"
    RESULT=0
else
    echo "==> FAILED on $HOST (commit $(git rev-parse --short HEAD))"
    RESULT=1
fi

echo "==> Cleaning up ~/$REMOTE_DIR on $HOST..."
ssh "$HOST" "rm -rf ~/$REMOTE_DIR"

exit $RESULT
