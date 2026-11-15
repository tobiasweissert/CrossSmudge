#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
export TMPDIR=/root/.hermes/cache/scratch
OUT=${LUA_HTTP_TEST_OUT:-/root/.hermes/cache/scratch/lua-http-followup-tests}
# Reuses shipped Lua objects from test/lua_http/run.sh, no firmware/SDK build.
c++ -std=c++17 -include initializer_list -I/root/github/crossplay/src/apps_local/connections test/connections/oracle.cpp /root/github/crossplay/src/apps_local/connections/ConnectionsCore.cpp -o "$OUT/connections-oracle"
"$OUT/connections-oracle" > /root/github/.host-data/crosssmudge/http-followup/oracle.txt
cc -O1 -g -fsanitize=address,undefined -Ilib/Lua/src test/connections/runner.c "$OUT"/*.o -ldl -lm -o "$OUT/connections-lua"
ASAN_OPTIONS=detect_leaks=0 SMUDGE_X3_CONSTRAINTS=1 LUA_APP_LIMIT=2097152 "$OUT/connections-lua" test/connections/test.lua
ASAN_OPTIONS=detect_leaks=0 SMUDGE_X3_CONSTRAINTS=1 LUA_APP_LIMIT=2097152 "$OUT/connections-lua" test/connections/app_test.lua
ASAN_OPTIONS=detect_leaks=0 SMUDGE_X3_CONSTRAINTS=1 LUA_APP_LIMIT=76800 "$OUT/connections-lua" test/connections/gate_test.lua
