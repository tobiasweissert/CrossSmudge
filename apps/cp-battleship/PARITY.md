# Battleship — local Lua adaptation

Implemented: Manual legal five-ship placement, rotation and collision checks; 10×10 board with 5/4/3/3/2 fleet; alternating shots, hits/misses, win after 17 hits; separate own/target views; saved game. Computer hunts alternating cells and follows unsunk observed hits.

Limitations: Solo only. Computer is a smaller public-observation hunt/target strategy, not a claim of original gunner parity. Nearby, original art/statistics and named sunk-ship announcements absent. Restart abandons placement only after confirmation.

Persistence uses app-relative checked two-slot journals, semantic input validation and readback; failed writes leave the previous slot available. These saves are separate from original CrossPlay and existing CrossSmudge IDs. No firmware or network hooks are installed.

Validation: `test/crossplay_ports/additions_run.py` uses the exact stock int32/float32 Lua VM at 76,800 and 2,097,152 bytes, both orientations, rule fixtures, callback/render instrumentation and journal failures. See repository `docs/crossplay-ports.md`. These are native adapter results, not e-ink screenshots or hardware measurements.

Hardware verification on Sticky v1.6.5.3: Place all five ships in both orientations, reject overlaps/edge overflow, switch own/target views, fire hit/miss, complete a game and reopen. Check serial logs for OOM/Application Error, internal free/largest heap and PSRAM. Repeat touch and buttons in both orientations. No EPUB cache reset is needed.


## Original presentation on larger Lua heaps

At a Lua limit of at least 512 KiB the app now uses the original Jersey font masks and game-specific CrossPlay board geometry, piece/dice rendering, turn controls and result layouts. The 75 KiB path keeps the original lightweight UI; rules and journal formats are shared. Portrait is the original reference layout; landscape adapts it to fit the shorter screen.

Target selection now aims before the Fire button commits a shot. Own fleet and enemy roster are graphical. Placement still uses the existing sequential bow/rotation controls rather than the original drag/edit/shuffle/Ready flow; help remains text. Lifetime statistics, Nearby and a separate completed-match history remain absent; the front door previews the actual current journal position.

Verification: `python3 test/board_visual_ports/run.py` tests the exact shipped Lua numeric ABI, 76,800/2,097,152-byte allocators and both screen orientations, including real rendered touch targets, journal round trips and help pagination. Native draw-call captures are not physical e-ink screenshots. On Sticky, check board controls/help/results in both orientations, then save/reopen and watch Application Error/OOM logs. No cache reset is required.


The larger-heap front door now follows the source record/rule/ornament/bottom-door bands, with a live miniature of this port's current position and Play/Resume, Settings and How to Play. Settings exposes the existing new-game, local-player and save/exit actions; no lifetime counts are synthesized. The original Nearby door is replaced by supported settings. Native checks exercise physical and touch action mappings, and capture home/settings in both orientations.

The supplied original source has no illustrated HowTo screen for this app. The port retains accurate local text help; it is not described as an original illustrated tutorial.
