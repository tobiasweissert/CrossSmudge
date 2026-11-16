Actual Wavelength production callbacks and original native pixels

Run from the firmware checkout:

    python3 test/wavelength_original_ui/all.py --output /absolute/evidence/output

For extracted SD qualification:

    python3 test/wavelength_original_ui/all.py --app-root /absolute/extracted/.crosssmudge/applications --output /absolute/evidence/extracted-output

The default app root is repo/apps. The parent of cp-wavelength is the app root.
Every actual app is copied to a private temporary directory, so tests never write
user saves or source folders. Native/generated C/C++/objects stay in evidence.
The routine uses the shared canonical production renderer builder with real
Sticky get_device, has_feature and orientation bindings, actual restricted
int32/float32 Lua libraries, shipped glyphs, 2MiB Lua allocation budget, and
production Lua main/on_draw/touch/button/release/lifecycle callbacks. Host
storage, input and clock are controlled adapters; no fake draw/font fallback.

native.py compiles immutable original WavelengthScreens.cpp/Core.cpp, original
fonts, actual pinned SDK UI and source-checked primitives. Its host scaffold is
read from the frozen native-catalog-references/native_catalog.cpp evidence input;
this is an explicit reproducibility dependency. Actual rendered game models are
exported from real app fields, never separately seeded target/reference games.
Screens source revision d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b is checked before
compiling, with the SDK checked against its original submodule object.

all.py runs states, all20 distance verdicts, exhausted-deck single-choice,
65535 observations/chart scaling, privacy/release, failed-write/retry/reopen
abandon-count saturation, mode-toggle completion deduplication, and unchanged
exhaustive1600 scoring cases. verify.py retains fullsize native
PGM/PNG/text traces and pixel metrics. Actual draw traces accompany port PNGs.
Both480x800 and800x480 ENTRY orientations deliberately produce portrait, matching
the original supported profile, and restore the previous orientation on exit.
This is not a native landscape composition claim.

The final finite run has118 same-state native comparisons:100 wholeframe zero
differences,18 exact outside the declared UI data/control adaptations. Menu
regions x16..463/y508..525 and744..783 label observed records since this UI update
and preserve old Co-op/Teams choice. Summary x16..463/y705..733 similarly labels
observed history. Those are explicit exceptions, never pixel equality claims.
Existing files contain no lifetime histogram: the new UI-only readback-checked
observation slots count only actually observed scored rounds, skip practice and
fence imported old finals; they never reconstruct historical values. Original
game codec, journal, scoring, deck, main release-feature opt-in stay unchanged.
The native source always prints the practice footnote on the call screen even
for scored rounds; the port faithfully reproduces that source behavior.

preservation.py compares baseline621719e4/current pixel/primitive/text-call traces
and exact journal bytes for highheap x4-pro,75KiB x3, unknown/missing get_device,
and missing touch_release capability, both entry orientations:10 pairs. Compact
built-in text is recorded at its actual call seam; no C3 textpixel approval is
claimed. Three legacy UI modules are exact immutable baseline bytes. red.py
re-extracts original port files from git and asserts the actual57875 menu pixel
mismatch against native for each entry orientation.

Privacy tests require a bare tap/off-pad hold to expose nothing; actual pad hold
reveals, outside release hides+fullrefreshes, hardware Confirm release hides
immediately before update, mixed holds stay visible only while one remains,
Back/exit hide and private-save reopen uses the pass screen. No test-only UI
state setter is used. Input simulation and valid journal imports are identified
by their fixture sources. Lifetime/abandon values observed since this update
cannot reproduce unavailable old historical fields. No RTC/cross-boot owner
recognition is invented; the existing saved game resumes through its safe phase.

The host Lua peak is measured; native C/HAL allocation and physical e-ink timing
are not. No PIO, commit, remote, store, release, flash or hardware approval.
Hardware: on Sticky open from each orientation; START -> PASS -> PICK; verify
bare taps reveal nothing, long hold/release outside hides without a ghost, repeat
holds work, I HAVE MY CLUE clears the number, lock is separated from the strip,
Co-op skips side call, Teams endpoints offer one valid side, and Back/pause,
abandon, summary, new session and reopen preserve scores. Check legacy on x3/x4
and x4-pro. Keep existing state-a/b.dat; no cache reset required. UI observations
use separate ui-observed-a/b.dat and never replace game journals.
