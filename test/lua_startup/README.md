Production startup regression (vendored int32/float32 Lua, no `luaL_openlibs`):

```
CP_FIXTURES=/path/to/captured/provider/fixtures python3 test/lua_startup/run.py
```

Fixtures must contain `feed.body`, `item.body`, and their original `provenance.json` SHA-256 records. The runner verifies these hashes. Captures remain external, never packaged into apps or firmware. The native executor replays their bytes; it does not claim live firmware DNS/TLS success. UI/storage doubles run the unchanged app init/main/JSON/cache code. Production library helpers and manifest parser are shared with firmware; the historical baseline is extracted from the delivered device-fix revision.

Checks: real feed → item → rendered title → exact cache → offline relaunch; no-config policy, opt-out/reenable and NVS failures; transport/time errors; cancelled response ignored; bounded full-feed OOM and zero retained allocation; native UTF-8 semantics; 32-bit Lua numbers; unchanged offline retained library heap; protected essential/UTF-8 initialization OOM. At 75 KiB the captured 500-entry feed exhausts the existing app heap; this test expects recoverable `LUA_ERRMEM` and cleanup, not successful C3 full-feed decoding. The Sticky 2 MiB case must succeed.
