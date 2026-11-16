# Owner + feedback integration gate

From the firmware repository, run:

```sh
SMUDGE_X3_CONSTRAINTS=1 python3 test/integration_owner/run.py --output /tmp/owner-integration
```

Provision the pinned `sticky-owner-test` dependencies first, under the shared
PlatformIO build lock when using the shared core. Set `ARDUINOJSON_INCLUDE_DIR`
if its source is elsewhere. The gate runs the existing app, visual, sandbox,
board-identity and HTTPS suites and records each command/exit/log. HTTPS, runtime
library startup and App Store suites include ASan/UBSan. Lua cases use their
explicit 75 KiB and S3 heap limits; S3-only previews are not claimed to fit C3.

The newer Sticky Solitaire geometry is checked by `replacement_solitaire` and
`lua_device_identity`. The older `solitaire_touch` fixture is run for the legacy
non-Sticky dispatch paths; its Sticky fixture predates the replacement API.
Historical external-source review runners are not this gate's dependencies;
the primary suites include the checked-in oracles and gameplay/save checks.

The owner-test overlay is produced by the production packaging script. Its
members must equal the repository bytes, include every runtime dependency and
catalog member, and exclude saves. The published catalog, versions and remote
store are not changed. Trivia remains in main and its own production tests; the
owner overlay continues to omit it according to the selected packaging scope.

This is host verification, not physical display, touch, TLS or memory acceptance.
On Sticky, exercise Applications swipes; Hourglass controls; Sudoku legacy-save
import, solving, records/help/result, exit/reopen; Solitaire draw/move/undo/save;
Five Letters Back/menu; and HN six-row retry, article/comment navigation, cancel,
offline reopen and networking opt-out/reenable. Check portrait and landscape,
internal heap/largest block, PSRAM and HTTP-worker watermark. UTC should sync
automatically and failed certificates must never send HTTP bytes. No EPUB cache
reset is required. App Store's one-row-per-swipe behavior remains unresolved.
