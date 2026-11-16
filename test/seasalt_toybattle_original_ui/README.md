Sea Salt / Toy Battle original UI

Run `python3 test/seasalt_toybattle_original_ui/all.py --output /tmp/sea-toy-evidence`.
The routine builds fresh private objects, using the shared canonical production
Lua adapter, actual int32/float32 VM/libraries/Sticky identity/orientation bindings,
and the immutable original CrossPlay d3a49ce8 screens, fonts, primitives, core and
ToyBattle Activity prompt method. It reads the frozen native scaffold under
`.host-data/crosssmudge/ported-ui-parity/native-catalog-references`; nothing there
is modified. No PIO or foreign compiled cache is used.

The finite suite has 22 callback runs at 2 MiB, both entry orientations:

- Original menus; all 6 Sea Salt and 8 Toy Battle help pages; both map pages;
  setup; direct draw/keep/place/dig/selection/pair/call controls.
- Complete real matches, save/reopen, failed save/retry, closed-input behavior.
- All nine playable terrains plus a valid legacy Proving Ground save. The latter
  remains absent from the new-game map list, as in the original.
- Draw, steal, Captain chain, Jumbo, Recall, Draw-base, ShoveFrom/ShoveTo,
  Exhume/discard, Suppress and applied states. Base fixtures are prepared by legal
  old-engine moves, imported through the actual journal, and then driven by taps.
- Toy Battle wins/losses and HQ/medal/stuck endings. Stuck is a legal stack-only
  engine prelude loaded before its final real callback.
- Sea Salt STOP/Last Chance, summaries, four-mermaid and deck-exhaustion endings.
  Rare endings use legally engine-advanced save inputs before the final callback;
  no test setters are added to apps. The deck fixture uses a round starting seat 2.
- Actual checked long records (65535 played/won), fresh and resumed menus.

`verify.py` maps captured real state/drafts into original native models and saves
full PGM/PNG references and traces. The final receipt enumerates 304 comparisons:
302 whole frames have zero changed pixels. Two setup frames differ only in the
fixed opponent field, LOCAL instead of GENERAL, since the old core has one AI.
Original lifetime Toy Battle history and side/difficulty choices are not inferred
from fields the old model never stored. These are explicit adaptations, not
native pixel approvals for unavailable data. Later position space is not claimed
exhaustively enumerated by these finite fixtures.

Both apps use their original portrait composition from either entry orientation
and restore the host orientation. Toy Battle's original geometry assumes an
800px-tall portrait viewport; the existing landscape approximation is replaced by
this explicit supported-profile adaptation.

`preservation.py` checks immutable 621719e4 against current high-heap x4-pro and
75 KiB x3 callbacks, both entry orientations: eight equal successful pairs,
including exact primitive/text call traces and save bytes. The optional C text
trace adapter avoids Lua test-log allocation overhead; C3 built-in text pixels
are outside this bitmap seam and are not approved by these captures. Sticky tests
leave unsupported text APIs strict. `red_baseline.py` reproduces the old native
menu failures. Exact old `legacy_*` modules preserve the nonSticky branch.

`assets.py`, `help_assets.py`, and `brief_assets.py` regenerate licensed original
component art through native original methods. Playing boards, cards, values,
menus, prompts and controls are live model rendering. Fixed manual diagrams and
map-specific reference plates retain original glyph/art pixels. Only the active
help diagram/reference plate is cached as a compact bitmap string; no large Lua
pixel tables are built. Measured Lua peaks remain under 658 KiB in the rich suite;
this does not include HAL/C allocations or prove physical device headroom.

Hardware: on Sticky open each title from either orientation and check portrait
and restoration. Sea Salt: draw/keep/place, pair/undo, dig, STOP/Last Chance,
round/match/reopen. Toy Battle: duplicate rack cards, base targets, TAKE/SKIP/BACK,
Captain chain, all terrain gates, held ending board -> HOW IT ENDED -> result,
map/brief/help and reopen. Confirm unavailable Nearby preserves the local game.
On x3/x4/x4-pro check unchanged legacy presentation and saves. No save/cache reset
is required. No physical e-ink, release, remote or flash verification is claimed.
