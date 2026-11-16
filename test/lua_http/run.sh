#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
TEST_OUT=${LUA_HTTP_TEST_OUT:-/tmp/lua-http-tests}
mkdir -p "$TEST_OUT"
export ASAN_OPTIONS=detect_leaks=0
FLAGS=(-std=c++20 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer -Ilib/LuaHttp -Ilib/Lua/src)
for unit in policy protocol budget; do
  c++ "${FLAGS[@]}" "test/lua_http/${unit}_test.cpp" lib/LuaHttp/HttpCore.cpp -o "$TEST_OUT/$unit"
  "$TEST_OUT/$unit"
done
c++ "${FLAGS[@]}" -DBOARD_HAS_PSRAM=1 test/lua_http/protocol_test.cpp lib/LuaHttp/HttpCore.cpp -o "$TEST_OUT/protocol-psram"
"$TEST_OUT/protocol-psram"
c++ "${FLAGS[@]}" -I.pio/libdeps/sticky/ArduinoJson/src test/lua_http/manifest_test.cpp lib/LuaHttp/HttpCore.cpp -o "$TEST_OUT/manifest"
"$TEST_OUT/manifest"
for source in lib/Lua/src/l*.c; do
  cc -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Ilib/Lua/src -c "$source" -o "$TEST_OUT/$(basename "${source%.c}").o"
done
c++ "${FLAGS[@]}" -pthread test/lua_http/async_test.cpp lib/LuaHttp/{LuaHttp,HttpCore}.cpp "$TEST_OUT"/*.o -ldl -lm -o "$TEST_OUT/async"
SMUDGE_X3_CONSTRAINTS=1 "$TEST_OUT/async"
printf 'PASS: policy, bounded HTTP parser, allocation budget, production Lua async registration/lifecycle (ASan+UBSan; LSan unavailable under ptrace)\n'
