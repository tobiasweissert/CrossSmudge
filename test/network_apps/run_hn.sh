#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export TMPDIR="${TMPDIR:-/tmp}"
out="${NETWORK_TEST_OUT:-$TMPDIR/cp-hn-tests}"
mkdir -p "$out"
cc -O1 -Ilib/Lua/src test/network_apps/runner.c lib/Lua/src/l*.c -lm -ldl -o "$out/lua"
"$out/lua" test/network_apps/test.lua
for source in lib/Lua/src/l*.c; do
  cc -O1 -Ilib/Lua/src -c "$source" -o "$out/$(basename "${source%.c}").o"
done
c++ -O1 -std=c++20 -Wall -Wextra -Ilib/Lua/src -Ilib/LuaHttp test/network_apps/bridge.cpp lib/LuaHttp/{LuaHttp,HttpCore}.cpp "$out"/*.o -lm -ldl -o "$out/bridge"
"$out/bridge" cp-hackernews
CP_STRESS=1 "$out/bridge" cp-hackernews
