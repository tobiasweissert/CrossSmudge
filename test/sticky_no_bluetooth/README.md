# Sticky without Bluetooth remote

Run the focused routine suite (no downloads):

```
cmake -S test/sticky_no_bluetooth -B "$TMPDIR/sticky-no-bluetooth-tests" \
  -DSTICKY_NO_BLUETOOTH_SDK=/path/to/pinned/freeink-sdk
ctest --test-dir "$TMPDIR/sticky-no-bluetooth-tests" --output-on-failure -j1
```

Also registered in `test/CMakeLists.txt`. Both normal and ASan/UBSan modes run:

- effective Sticky/debug/simulator configuration and retained dependency controls;
- actual `Runtime.cpp` + `NoRadio.cpp` with saved enabled preference and mappings;
- exact menu insertion and action-dispatch slices with capability zero/one;
- enabled-port runtime control using the existing fake radio (no real Bluetooth);
- original font/settings/network/AirDrop bytes against the restored all-font baseline,
  plus all 76 canonical font metadata arrays;
- existing AirDrop consent and WiFi/AWDL lifecycle fixtures.

The disabled runtime must not reclaim caches, yield WiFi/reader ownership, deliver
remote input, display link failures, or mutate saved settings. Unsupported settings
actions are inert, not merely hidden. The header-only SDK remote profiles remain
available for persisted mapping types, without a BleKeyboardHost library dependency.
No settings/NVS erase is introduced.

These host fixtures do not establish target linkage, OTA slot fit, RF interoperability,
physical input/rendering, on-device NVS preservation, or recovery. Standard raw
`pio run -e sticky` build/map/config qualification is still required before release.
