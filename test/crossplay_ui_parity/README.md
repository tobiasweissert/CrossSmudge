# CrossPlay production pixel regressions

Run from the firmware repository:

```
python3 test/crossplay_ui_parity/run.py --reference /root/github/crossplay --output /path/to/evidence
```

The original revision is pinned to `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`. Relevant source files and SDK UI/icon headers must match immutable Git objects before compilation. No PlatformIO or network operation is used. Output contains host source/objects in `tooling/`, full-size PNGs and frame draw traces in `captures/`, individual outcomes and copied app/test hashes in `results.json`. Temporary app copies are removed; installed saves are never accessed.

`build_capture.py --output DIR [--root ROOT]` compiles the actual renderer pixel/rotation/shape/dither methods, actual Lua bindings, and production bitmap raster loop. Backing framebuffer and file I/O are host adapters. Fonts/sprites come from actual shipped bytes; unknown host text APIs fail. Device identity uses `LuaRunner::l_getDevice` under resolved Sticky owner-test defines and AppCapabilities mapping. Libraries use `LuaRuntime::openEssential`, and the VM is int32/float32. Every fixture checks that identity before any explicit preservation override.

`build_native.py` compiles original Battleship builders/core and extracted native Activity painters/report logic. `build_solitaire_reference.py` reuses that original raster/font scaffold and compiles original Solitaire builders/core/CardArt plus original truncation. Neither references port layout constants. Mapped model inputs are saved next to captures. A replayable stock action uses the actual original action to retain Undo history; arbitrary restore fixtures are labeled as such rather than claimed to be played histories.

The registry tests Battleship placement/partial saves/buttons/focus/menus/records/failed writes/repeated games/target contacts/imported view flags, Solitaire menu/cards/fans/records, Sudoku preview/reveal, and Wordle input. Both dimensions run with a 2MiB Lua cap and zero allocation retained after close. Frame traces reset on Clear, so discarded repaints do not become a test-only unbounded allocation. The font guard requires its specific failure.

Named Battleship portrait gameplay/result/aim frames and Solitaire landscape menu/board/crowded/selected/waste/victory frames require zero full-frame pixel differences. Other dimensions remain captured adaptations: native Battleship assumes portrait and native Solitaire forces landscape. This does not establish every title/state, physical acceptance, firmware compilation, or publication readiness. All-title reference capture and per-component gates are separate evidence.

To test an extracted local SD candidate, add `--apps-root /path/to/.crosssmudge/applications --only-app cp-battleship --only-app solitaire`. `--skip-build --tooling /path/to/qualified/tooling` reuses the explicitly selected binaries. The package script creates complete runtime-member archives without changing catalog/manifests; byte readback and actual extracted execution are distinct receipts.

The PNG comparator is separate from both renderers. Source strings, bounding boxes and fake fonts cannot approve a nonzero frame. Original site shots are historical LANCZOS exports, retained for context rather than used as one-bit same-state oracles.

The complete local SD integration gate is `qualify_catalog.py`:

```
python3 scripts/package_crossplay_ui.py --inventory EVIDENCE/inventory/ledger.json --output PACKAGES
python3 test/crossplay_ui_parity/qualify_catalog.py --evidence EVIDENCE --packages PACKAGES --reference /root/github/crossplay --output QUALIFICATION
```

It reconciles all24 package IDs against the machine inventory, validates every
archive/member hash against the exact source, extracts SD files without saves,
and runs all13 app-specific production callback/pixel groups serially. Each
component retains its precise original-state mappings and disclosed layout,
service and model limitations; a pass is not a claim that missing native
services exist. Input hashes must stay unchanged throughout qualification.
`--resume` accepts a previous group only for the same complete source/test hash;
`--only-group` always leaves the overall receipt explicitly incomplete.

The production adapter also extracts real orientation and feature-detection
bindings. Sticky touch metrics use the production `buttonHintsHeight=0` rule
from UITheme; other adapter metrics remain named host fixture inputs. This
host framebuffer/file adapter is not an ESP32, a physical panel, an SD transport
or a PIO build. Firmware source and pinned SDK are unchanged by this app-only
candidate; device acceptance remains the listed per-title hardware procedure.
