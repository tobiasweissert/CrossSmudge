# Minesweeper 0.1.4

The portrait board follows the supplied CrossPlay 480×800 screenshot: 76px black
header, board starting at y=84, 8×10 continuous 60px cells, 25% dither, original
Jersey 25 glyph pixels, Lucide flags, source mine bars, and counter/mode control.
The current CrossPlay C++ source places its board lower (y=98); this port follows
the owner's screenshot for that geometry. Landscape reduces cell size to keep
all controls visible. Host PNG captures are logical raster captures of production
Lua with the same bitmap/glyph data and drawing semantics, not hardware screenshots.
The reference PNG contains intermediate gray pixels; the firmware output is 1-bit.

Controls: tap a cell to dig; tap DIG/FLAG to change the tap mode. With the new
host firmware, hold a covered cell to toggle its flag once, independent of mode;
lifting after a hold does not dig. Old hosts retain the mode-button fallback.
Tap the header or press Back for the menu. Arrow/page buttons move the cursor;
Confirm acts. The menu provides Resume, mode toggle, confirmed New Game, Help,
and Save and Exit. Settled boards retain mines and a CLEARED/BOOM control.

The 8×10 rules, first-dig safety, flood opening, chording, win/loss and journal
schema are unchanged. Statistics/result pages from CrossPlay are not ported;
the existing local menu and save workflow are retained. No Nearby feature.

Compact font strings are loaded once at initialization; glyph slices are small
transient strings. No framebuffer is allocated by the app. The native tests use
the shipped int32/float32 Lua VM with 75 KiB and 2 MiB allocation caps. These
caps exclude native renderer, HAL and system allocations. The assertion-heavy
graphics contract and synthetic-board capture fixture require the larger test budget; actual app render,
basic real-input hold and repeated lifecycle cases are tested at 75 KiB. The
full instrumented input matrix uses 2 MiB.

Install only this package's listed files into cp-minesweeper. Keep existing
state-a.dat, state-b.dat and unknown files; the ZIP contains no saves. Back/menu
and exit save through the existing checked two-slot journal. Save failure stays
visible and allows retry. No EPUB cache reset is needed.

Hardware acceptance remains pending: install the matching new Sticky owner-test
firmware and app package, verify a stationary hold flags exactly once and release
does not open a cell; repeat to unflag, then short-tap to dig. Test motion, edge
Back/Home, disabled touch, release outside, portrait/landscape, save/reopen and
sleep/wake. Check physical e-ink legibility, refresh latency and free internal
heap/PSRAM. Verify Solitaire still responds to its usual taps. A successful build
and native input tests are not physical-device acceptance.
