# Murdle original casebook presentation

Sticky restores the original CrossPlay menu, four difficulty rows, illustrated
rules, replacement confirmation, clue pages, grid/key, dossier, accusation and
verdict using its original font and icon bytes. The source renderer is pinned
at `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`; see PROVENANCE.md.

`test/murdle_original_ui/run.py` imports each of the64 actual case journals and
maps every menu, clue page, grid, dossier and accusation sheet to the original
native model. Eight boundary cases also exercise real grid No/Yes/refusal,
wrong/correct accusations and completed menus. All790 complete-frame
comparisons match, across both entry orientations. The original portrait
profile is forced and restored on exit; this does not claim a native landscape
composition. Callback tests separately cover canceled replacement, old-save
reopen, failed new-case writes and queued old-screen contacts. New screen
contacts are retired until repaint so a queued confirmation cannot replace two
cases. Native API adapter rule/save tests include the exact75KiB cap, separate
from the production raster tests at2MiB. Six non-Sticky before/after pairs match
pixels, traces and save bytes.

Rules, generator-exported data, IDs, icons, manifests and the checked two-slot
core journal remain unchanged. The finite bank replaces unlimited native
on-device generation: next case cycles within its16-case tier. This port opens
an actual case on first launch, so the native empty-case front door is not its
startup state. Clue ticks and next-case difficulty remain this visit's UI
state; the current controller does not restore them. No historical lifetime
record is invented. Per-cell focus corners appear for physical navigation.

On hardware, merge local SD application files in place and retain saved data.
On Sticky, enter from both orientations and check restoration on exit. Exercise
all three header doors, every clue page (case36 has two), empty staircase
corners, No/Yes/derived marks and blocked matches. Read all four-tier dossiers,
accuse every category, view wrong/correct verdicts, cancel a replacement and
reopen an old journal. Check write-failure notice/retry and physical navigation.
No cache reset is needed. Host raster evidence does not establish physical
screen, timing, SD transport, internal-heap or PSRAM acceptance. No firmware,
store, version, remote or release update is part of this candidate.
