#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
out="$(mktemp -d "${TMPDIR:-/tmp}/smudge-productivity.XXXXXX")"
trap 'rm -rf "$out"' EXIT
cc -O1 -Ilib/Lua/src test/productivity/runner.c lib/Lua/src/l*.c -lm -o "$out/lua"
for dimensions in '480 800' '800 480'; do
 read -r width height <<< "$dimensions"
 for app in flashcards habits pomodoro weather; do
  SMUDGE_X3_CONSTRAINTS=1 LUA_APP_LIMIT=76800 WIDTH="$width" HEIGHT="$height" APP="$app" "$out/lua" test/productivity/host.lua
 done
done

python3 test/browser/run.py
