# Lifecycle migration host qualification

Run from repository root:

```
python3 test/crosspoint_migration/lifecycle/run.py --evidence <durable-evidence-directory>
python3 test/crosspoint_migration/lifecycle/run.py --sanitize --evidence <different-evidence-directory>
```

Each compile/executable is bounded to 60 seconds and writes generated fixture source, raw logs and JSON exit receipts. No downloads, dependency initialization or target firmware build. SDK BleRemoteProfiles.h is read from the pinned local object store, not an initialized SDK checkout.

Five routine suites execute six native executables per mode:
- remote-render: complete production render loop (one RTOS-double iteration), RenderLock/wait and remote admission/action methods;
- popup: complete real OptionPopup header with rendering/input/font/geometry doubles;
- radio: complete main BLE-host, sleep-plugin and enterDeepSleep bodies, both default-denied plugin networking and an explicit qualified-transport fixture variant;
- edges: complete manager edge dispatcher and AirDrop radio admission, reading the actual settings-contract field names;
- runtime: complete BlePageTurner Runtime.cpp and upstream RadioPort fakes, with actual pinned SDK profile header.

The RTOS-double render loop exits using a test sentinel after one iteration. It is not a firmware exception path. The qualified plugin fixture is not real TLS acceptance: production sleep-plugin networking remains default-off while inherited unowned PluginHttp still uses setInsecure().

These tests prove selected production bodies and actual full popup/BLE runtime compilation against explicit seams, not whole manager/main TUs, SDK linkage, real fonts/pixels, FAT durability, actual cryptography, wireless coexistence or physical contact timing. See external integration/lifecycle/CONTRACT.md and HANDOFF.md for source hashes and remaining gates. Root test registration and peer source are outside this writer's ownership.
