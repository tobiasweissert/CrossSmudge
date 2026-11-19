# Font-finish qualification

Run from the repository root:

```
python3 test/crosspoint_migration/font-finish/run.py --evidence <outside-worktree-dir>
python3 test/crosspoint_migration/font-finish/run.py --sanitize --evidence <outside-worktree-dir>
```

The entrypoint runs five real-production executable probes (SD bitmap loading/cache/identity, persistent mixed bitmap/vector catalog, complete renderer/cache/decompressor with synthetic fallback and owner Inter compressed asset, profile storage, installer path admission) and two whole-TU syntax configurations for the SD system/manager/installer, vector off/on. Every child has a 110-second timeout, retained stdout/stderr, exit receipt and input SHA256 manifest. No network, install, SDK initialization or firmware build is performed.

Host adapters are deliberately limited:
- Catalog/profile storage uses the existing filesystem-backed HalStorage adapter. Vector metadata inspection deliberately fails through a small FtFont double, exercising production filename-style fallback, not actual FreeType inspection.
- Renderer uses the existing in-memory panel, storage, scheduler/mutex and heap adapters. MemoryManager sink registration is inert in this deterministic fixture. Actual GfxRenderer, Bitmap, FontCacheManager, FontDecompressor, EpdFont/Family, SdCardFont, BiDi and inflater translation units compile/link/run. The owner Inter asset is read from its real header; it is not a copied font or device pixel golden.
- The selected SDK's exact GrayscaleCapabilities.h is retained as a small readonly-object extract. FontAlloc is malloc-backed on the host; FontPsram uses the existing host adapter. This is not SDK-native vector font rasterization.
- SD system syntax checks use the real CrossPointSettings header and already-installed ArduinoJson headers; TextBlock/BLE/HAL dependencies are explicit doubles. They do **not** link or execute dictionary switching, streaming-vector source lifetime, profile-triggered reloading or CSS variant eviction. Those remain parent integration gates.
- `-include cstring` supplies the normal Arduino transitive standard header needed by the already-frozen FontDecompressor TU; that frozen source was not edited. Missing trailing callback initializers in legacy builtin font aggregates are intentionally zero-initialized; their single warning class is disabled. Other C++ warnings are errors.

Existing four font tests and 38 frozen font source files are untouched. The new entrypoint is routine for this allowlisted subtree; parent must register it in combined CI without rewriting the frozen original entrypoint. Logs from earlier failed migrations and fixture corrections are retained separately from final qualification.
