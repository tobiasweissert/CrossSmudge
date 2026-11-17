# Project Vision & Scope: Crosscub

Crosscub is a Sticky-only continuation of the selected currentmain CrossSmudge
firmware, descended from CrossInk, CrossPoint Reader and FreeInk SDK. Those
projects retain their authorship and license credits. This is not a fresh
upstream reset and does not claim original authorship of inherited code.

## Core mission

Provide a stable reading-first firmware for the **Seeed reTerminal Sticky**,
keeping the current design and navigation while fitting its 480×800 portrait
and 800×480 landscape viewports. Preserve working features before optional
redesigns. Shared SDK hardware catalogues are dependency internals, not
Crosscub product targets.

## Existing features remain in scope

- EPUB, TXT, XTC and current document/font/dictionary/rendering features;
  bookmarks, clippings, reading progress, statistics, sleep and power behavior.
- Existing OPDS, Calibre, Wi-Fi, web file manager/WebDAV, local transfers,
  native AirDrop and the public API7 Browser backend; API3/API6 compatibility.
- The full current Lua catalog, loader, application sandbox, installed packages,
  games/utilities and their existing save/history/journal paths.
- Sticky touch, gesture, button, clock and sensor behavior already present in
  currentmain. The existing RTC/time-trust and network admission policies stay
  intact; this migration does not grant a new transport or memory budget.

## Product and data boundaries

- Crosscub is the active product identity. The repository URL and serialized
  `.crosspoint`, `.crosssmudge` and `.smudge` names remain where compatibility
  requires them. Rebranding must not reset SD/NVS or strand installed apps.
- The owner-controlled App Store is the normal default. OTA must validate a
  Crosscub release and application identity, never silently select a different
  upstream product. Manual SD application update/recovery remains usable.
- Keep mandatory copyrights, third-party SDK/vendor files, font/asset licenses
  and historical provenance honest. Do not mutate dependency board profiles to
  make a text search empty.
- Target compilation, host callback/render fixtures and physical e-ink/radio
  acceptance are different gates. A successful host capture is not a flash or
  a claim of physical installation.

## Change discipline

Use behavioral RED/GREEN tests for changed behavior. Keep the current graphical
style, fonts and navigation; repair reproduced clipping or inaccessible touch
controls with shared draw/hit geometry. Do not use this migration for unrelated
backend deployment, budget changes, broad refactors or a UI redesign. Optional
future work requires its own scope and acceptance criteria.
