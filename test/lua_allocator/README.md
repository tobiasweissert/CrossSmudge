# Lua VM heap policy

Run `python3 test/lua_allocator/run.py`.

The harness extracts the actual `LuaRunner::customLuaAlloc` body and the actual init-time pool-selection span from production `LuaRunner.cpp`; it does not maintain a second allocator implementation. It compiles those spans against capability-aware fake heaps and the vendored firmware Lua sources with their int32/float32 settings.

Four configurations cover physical PSRAM hardware with/without detected PSRAM, C3 and simulator. Checks cover tiny allocations (including Lua's nullptr/osize type tag), grow/shrink and payload preservation, injected realloc failure retaining the old block/accounting, the existing byte cap, free and zero-byte behavior, PSRAM exhaustion without internal fallback, C3's existing internal safety cushion, and a real Lua VM workload that hits OOM, recovers, then releases all allocations on close. Selected PSRAM mode must make zero libc realloc calls and consume zero modeled internal bytes; other modes must make zero capability realloc calls.

The fake pools prove routing, failure and accounting behavior. They do not measure ESP32 allocator fragmentation, TLS headroom, performance, or real device peak memory. Hardware validation must compare internal free/largest blocks before each TLS connection and after teardown, alongside PSRAM and Lua usage. Evidence is written to `/tmp/lua-allocator-results.json`.
