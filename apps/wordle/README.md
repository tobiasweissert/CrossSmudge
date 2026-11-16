# Wordle 1.2.0

CrossPlay Wordle on Sticky; original game and saves unchanged on other devices.

See [PARITY.md](PARITY.md) for deliberately explicit source behavior and compatibility limits and [PROVENANCE.md](PROVENANCE.md) for attribution. Keep the existing `wordle` app ID. Install all files listed in its catalog record.

On Sticky, saves use `/.smudge/saved_data/wordle/crossplay-*.dat`, the app-private persistent directory already created by stock firmware. Each write targets the older slot, checks its length/checksum, and reads the bytes back before advancing the in-memory sequence. Invalid newest slots fall back to older valid states. Interrupted writes cannot erase both journal slots in one operation. Files are bounded, parsed as data, and never executed. Original legacy `.dat` files are never changed; bounded copies are stored as `legacy-*.bak`, existing differing backups are retained. Legacy engine schemas are not silently converted: a visible notice asks for a new game. Oversized legacy files remain untouched with an explicit notice.

Run standalone tests from the firmware checkout with `TMPDIR=/root/.hermes/cache/scratch python3 test/replacement_wordle/run.py`. No firmware build or SDK is needed. Test runners compile only the existing Lua sources and independently pinned native C++ oracle files.

Hardware verification: on Sticky with stock v1.6.5.3, open Apps > Wordle, play by touch and buttons in both orientations, exit/resume, and verify interrupted journal recovery and unchanged old saves. On X3/X4 (75 KiB), X4 Pro, and firmware without get_device, confirm the original game UI, buttons, gameplay and existing-save resume. No cache reset is needed.

CrossPlay gameplay is selected only when `smudge.get_device()` returns `"sticky"`. Every other device (including X3, X4, X4 Pro and simulator), and firmware without that API, runs the byte-identical original game from base `2c82f306`. Original modules, assets, callbacks and save APIs are preserved; the replacement core/journal is never loaded on that path.
