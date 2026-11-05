#!/bin/sh
# Release smoke test (CI has no game files): the packaged binary starts, links
# SDL3, prints its usage, and reaches the headless fatal path on an empty game
# folder without opening a window or a message box.
# usage: tools/release/smoke.sh <path to the packaged prageport>
set -eu
bin=$1
empty=$(mktemp -d)
"$bin" --help >/dev/null
set +e
out=$("$bin" --check 1 --game-dir "$empty" 2>&1)
rc=$?
set -e
rmdir "$empty"
echo "$out"
test "$rc" -eq 1
echo "$out" | grep -q 'Primal Rage: fatal init error'
echo "smoke: ok ($bin)"
