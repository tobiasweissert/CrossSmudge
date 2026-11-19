# Reading activity heatmap qualification

`python3 test/crosscub/run_all.py --heatmap-only --output /absolute/evidence/path`
runs the production calendar and extracted, unchanged Activity input methods in normal
and ASan/UBSan modes. Both are also registered in `test/crosscub/CMakeLists.txt`.
No network, dependency installation, migration or firmware build is involved.

The calendar model reads the existing 730-day Boolean bitset. It displays the last
365 days inclusive of today (shortened only at the supported calendar's 2000-01-01
boundary), in Monday-first week columns. Outside-period/future padding is absent;
out-of-retention/missing history is unknown, not a zero-duration reading day.
The anchor is the latest recorded activity, not today's date. There is no daily
minutes/intensity field and the heatmap does not create one.

The first page is Reading activity, followed by Per Book, This Device and optional
All Devices. Both portrait and landscape show exactly one continuous calendar grid.
Only a compact active-day count (or `No history`), small month/weekday labels and the
native header remain; invalid clocks show `Set clock`. Solid cells mean recorded
activity, hollow cells inactivity, dots unavailable history; an extra outline marks
today. Missing history is anchor zero **with bit zero clear**; a recorded 2000-01-01
is anchor zero **with bit zero set**, and is covered through the production writer.
Left/Right and swipes navigate pages; Confirm or the started-date touch target on
Per Book still opens date editing. No-RTC devices retain the combined stats route.

The portable tests compile the real `ReadingStatsUtils.cpp` with only clock/settings
HAL boundaries stubbed, and compare Gregorian boundaries against Python datetime.
The Activity slice tests execute production loop/navigation/exit functions, with
synthetic input and date-editor seams (they do not emulate the whole firmware).
`render_probe.py` also executes the actual heatmap render function with synthetic
font/SDK draw-call spies to assert one grid, cell bounds, compact non-overlapping
labels and truthful empty/invalid states. It is run by the routine in both modes;
it is not a screenshot or a substitute for the real-font SDL captures.

For full input qualification, supply an **already-built, source/hash-bound heatmap
SDL launch fixture**, not an arbitrary historical simulator:

```
READING_HEATMAP_SIMULATOR=/absolute/path/heatmap-simulator \
READING_HEATMAP_SIM_OUTPUT=/absolute/path/input-evidence \
python3 test/crosscub/run_all.py --heatmap-only --output /absolute/path/suite-evidence
```

The optional simulator suite exercises the real HalGPIO recognizer, MappedInputManager,
Activity loop and render: hardware navigation, swipe, direct page-dot touch, optional
All Devices fourth page, landscape, date-editor entry/Back, hardware Back, touch header
Back and Confirm exit, first-page swipe exit, no-book entry and aggregate dot touch.
It is launched in `unshare -n`; synthetic input/state is labelled. The supplied
binary and candidate source hashes must match its sibling `render-inputs.json`.
The fixture must expose only launch/access/telemetry seams, not replace the UI renderer.
A missing full fixture does not silently imply this integration was exercised.

Candidate evidence, build recipe, exact reused-object hashes and genuine production-font
screenshots for this task are retained under
`/root/github/.host-data/crosssmudge/reading-stats-heatmap/simplified/`.
Earlier captures/review/freezes in the parent evidence directory are historical,
not approval of this simplified successor.
The simulator uses the native 480x800 / 800x480 logical bounds; its screenshot exporter
removes a two-pixel border, producing 476x796 / 796x476 PNGs. No artwork is synthesized.
Target compilation and physical-device behavior remain separate acceptance gates.
