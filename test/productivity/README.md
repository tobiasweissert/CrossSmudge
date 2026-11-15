# Five Lua productivity apps

Run from the repo root:

```sh
bash test/productivity/run.sh
```

The runner compiles the vendored Lua with its shipped int32/float32 configuration.
It checks portrait 480×800 and landscape 800×480 at a 76,800-byte app budget,
with `SMUDGE_X3_CONSTRAINTS=1`. The budget includes the measured essential
library set (plus UTF-8 for network apps). Lua test scaffolding and its `io`
library are measured before app loading and excluded; device UI and storage
operations are native rather than Lua closures in firmware. Test-side runtime
allocations are still counted. Save slots live outside the Lua allocator in a
native file double, matching SD ownership. Every run checks zero retained Lua
allocations on shutdown. This is a host app test, not an ESP32 TLS or total
firmware RAM measurement.

Checks cover touch and button input, text editing, UTF-8 truncation, save/reopen
and interrupted writes, card/habit removal confirmation, calendar rollover,
Pomodoro transitions/millisecond wrap/close snapshots, malformed provider data,
forecast rows, request cancellation, late completion, browser navigation, and
bounded page parsing. The UI mock checks coordinates against screen bounds;
its font measurements approximate firmware typography.

`fixtures/provenance.json` records the URLs, retrieval date and checksums of
real host responses. Fixtures are test inputs only; shipped apps do not contain
fake network responses. Open-Meteo data attribution: CC BY 4.0. Browser text was
retrieved through Jina Reader from the Example Domain page.

For approximate drawing previews (requires Pillow and Arial or DejaVu Sans):

```sh
python3 test/productivity/preview.py
```

PNGs are written under `/tmp/crosssmudge-productivity-previews`. These use system
fonts and mocked chrome; they are layout aids, not firmware screenshots. The
preview uses a larger allocator so recording output cannot distort app memory
checks. Hardware verification remains described in
[`docs/productivity-apps.md`](../../docs/productivity-apps.md).
