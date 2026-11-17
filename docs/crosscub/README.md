# Crosscub preservation and qualification boundary

Foundation: `2a477dea03a6db5ed4b5ccd33772c04cf19f8767` (the existing reader and public Browser), not a new upstream reset. Crosscub is the Sticky-only product identity; inherited licenses, API names and persisted data remain compatible.

## Source catalog is not the live store

The foundation has **37 source applications**. The separate owner-controlled live store has **41**; `flashcards`, `habits`, `pomodoro` and `weather` are live-only. The standard firmware uses `https://raw.githubusercontent.com/tobiasweissert/CrossSmudge/integration/sticky-test-all/apps/` for the catalog, icons and package files. The local 37-entry `apps/catalog.json` describes this candidate's source packages; **do not overwrite the live 41-entry catalog with it**. No live catalog or release is published by host tests.

All source IDs and save formats stay unchanged. Stopwatch alone changes from 1.0.0 to 1.0.1 in manifest and local catalog for its reproduced landscape clipping repair. Other versions and metadata are preserved. Daily Office's existing lesson file is added to package closure; it is not a new feature or version change. Third-party installed apps remain dynamically discovered; the firmware does not restrict launch to the source inventory.

## What the ledgers mean

- `ui-coverage.json` enumerates all 75 native activity headers, 150 native draw/render files, 37 source apps and five web pages. Each record separates source preservation from named host execution. Viewport dimensions are a requested coverage boundary, **not** a claim that every screen/state has been rendered.
- `preservation.json` records source deltas/carry evidence and the feature seams retained from the selected foundation.
- `remaining-references.json` retains exact path, line and text for each owned retired-board reference and its exception. Dependency internals, fonts/binary artwork, format provenance, serialized aliases, negative guards and algebra identifiers are not selectable hardware targets.
- Raw results, raster captures and the complete dependency-reference inventory are retained outside the repository under the task's host-data evidence directory. They are not thousands of release-source files.

The production logo helper and rotation/drawPixel paths are exercised at 480x800 and 800x480. Boot drawing is also exercised with stubbed text; this is **not full boot-font rendering or physical e-ink acceptance**. Lua production-raster fixtures use production shape/font APIs but host storage/input/theme adapters. Stock-app checks use synthetic drawing/bounds callbacks. Native AirDrop rendering and App Store geometry tests are host adapters, not full native-screen rendering.

Unmeasured: physical touch/e-ink, radio/provider interoperability, sleep/wake/SD durability and exhaustive native/theme/translation states. The final target compilation, independent frozen-source review and any branch publication belong to the parent release gate. None are implied by a passing host routine.

## Host-only routine

Use an existing Python with Pillow and existing read-only dependencies. No install, firmware build or publication is performed:

```sh
export PYTHONDONTWRITEBYTECODE=1
export TMPDIR=/root/.hermes/cache/scratch
export ARDUINOJSON_INCLUDE_DIR=/root/.hermes/cache/scratch/sticky-browser-backend/pio-libdeps/sticky-owner-test/ArduinoJson/src
export CP_FIXTURES=/root/github/.host-data/crosssmudge/lua-network-apps/public-fixtures-parent
export ESP_IDF_SOURCE_ROOT=/root/github/.host-data/crosssmudge/browser-platformio/packages/framework-espidf
/root/github/.host-data/crosssmudge/browser-toolchain/bin/python test/crosscub/run_all.py --output /root/github/.host-data/crosssmudge/crosscub/integrated-final
P=/root/github/.host-data/crosssmudge/browser-platformio/packages/tool-cmake/bin
"$P/cmake" -S test/crosscub -B /root/.hermes/cache/scratch/crosscub/ctest -DPython3_EXECUTABLE=/root/github/.host-data/crosssmudge/browser-toolchain/bin/python
"$P/ctest" --test-dir /root/.hermes/cache/scratch/crosscub/ctest --output-on-failure
```

These are host-only tests. Do not install or mutate shared dependencies if discovery fails; report the missing path. Retain original generated bear artwork (not a manufactured SVG) and the selected header SHA `dc812cfb672ae9bda1de2bc3edb211caf07f1028c27e58f308731d60a8666ad2`.
