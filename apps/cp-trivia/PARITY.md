# Trivia original presentation and retained provider

Sticky renders the supported original CrossPlay Notice and Choice/result
composition using the original Jersey14/30 and NotoSerif14 masks and kerning.
Reference revision: `d3a49ce8e85f9bd0d3e0c774a54adc267fd7547b`.
`PROVENANCE.md` and `art/sources.json` record immutable source bytes/licenses.
`test/trivia_ui/run.py` checks actual callbacks, long text reachability and
rendered pixels. Native Notice is a full-frame match; twelve Choice/result
regions match. Provider status and unsupported controls are explicitly outside
those scoped comparisons. Landscape option rows shrink to48pixels because the
original builder leaves a negative clue region at that size.

Open Trivia DB remains the real question source. Existing model/cache/JSON and
legacy UI are unchanged. No original pack, category progression, modes,
difficulty, score history, reporting or settings backend is fabricated. Answers
shuffle, requests retain their six-second cooldown and checked question caches,
and saved replay starts unanswered. The supported Skip action occupies the
otherwise empty primary footer. NEXT/END remain; unsupported HIDE is omitted.
Visible READ and answer controls open complete paginated provider strings.
Unsupported Unicode is displayed as explicit codepoint escapes, preserving
provider bytes and answer equality. Cache/request status occupies the native
unused difficulty strip or header-right slot.

Question data is Open Trivia DB's CC BY-SA4.0 content; no question data is
bundled. The application code retains its MIT license. The source fixture peaks
at269079bytes under the2MiB Lua budget. Non-Sticky/missing identity runs the
unchanged entry point. The existing full legacy fixture reaches the same known
75KiB OOM in baseline and candidate; this is not a C3 pass.

For hardware verification, merge local candidate files without deleting the
two question caches. On Sticky test cached/live questions, every option,
correct/wrong results, next/skip cooldown, long question/answer pages, physical
page/Confirm controls, orientation, END and reopen. No cache reset is required.
Firmware Wi-Fi launch policy still applies. Host raster/input tests do not
establish live provider, physical TLS or e-ink acceptance. No store release.
