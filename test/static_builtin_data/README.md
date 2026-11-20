# Fixed static-builtins host qualification

Run from the checkout root, serially:

```sh
python3 test/static_builtin_data/run.py
python3 test/static_builtin_data/run.py --sanitize
```

For the historical pre-integration checkout and separately retained proposal, append
`--proposal /absolute/path/pending-integration.patch` to either command. This
runs a minimal plain-file patch overlay, exact environment/opt-in guard AST,
inherited configuration and actual project-only LTO hook policy checks. It
never applies the proposal to that checkout or invokes the SDK/target build.
Do not reapply that historical patch to the integrated checkout. Both commands
above now exercise the actual integrated policy, six-setting SDK request delta,
target guard preprocessing and recording-environment bridge/LTO hooks.

`BitmapSemanticsTest.cpp` passes CRC/Adler-consistent glyph serializations
through the real decoder before semantic admission. It covers packed 1/2-bit
raw extents, terminal offsets and empty glyphs, null bitmap, grouped 2-bit
row-aligned individual/cumulative footprints, scratch length and exact counted
membership, including valid noncontiguous frequency mapping. Allocation is
forbidden throughout hydration and failed retry. PASS lines are observations
within an executable, not independent unit-test counts.

The runner owns `crosspoint-qualification.lock` for its whole invocation, uses
`$TMPDIR/static-builtins-host`, compiles one translation unit at a time, and
uses the real `lib/miniz/src/miniz_impl.c` prefixed decoder. No framework install,
SDK build, target build, `RLIMIT_AS`, network, malloc-backed decoder state,
buildscratch loan or separate sliding window is used. ASan/UBSan/leak mode
instruments both the decoder and application; intentional malformed-input tests
must return false, not crash.

The explicit target profile serializes `<BBHhhH2xI` / `<III`. The canonical
fixtures contain SHA-256 identities from the already measured CI12 public ELF
arrays/blobs; tests reproduce them from committed numeric font sources and the
existing generator's **strip-unused** language profile. No new language/string
cut is introduced. English and all offsets remain raw. All 35 languages and 972
existing built-in StrIds are exercised through unchanged `I18n.cpp`, with raw
and packed output compared byte-for-byte. Original font source skeletons,
including licenses, descriptors, bitmaps, groups, kerning and ligatures, and the
entire linked/unlinked include inventory are compared unchanged. Selected
private descriptors have one external `const EpdFontData` definition (not one
`static constexpr` graph per including translation unit); their initializer
values and static lifetime are unchanged. Original public font headers remain
untouched. Cross-TU descriptor address identity and pre-hydration legacy
`EpdFontFamily` metric construction are exercised. Public `EpdFont` lookups for
every covered codepoint and borrowed pointers survive locale switches. Every
invalid uint16 StrId is checked in every language, and all invalid uint8
Language values are exercised. The ordinary raw headers/offsets/metadata/C++
preprocess identically to independently generated ordinary output.

`STATIC_BUILTIN_HOST_TEST` is an explicit host-only mode, incompatible with the
target opt-in. Generated font copies live only in private build output. The
ordinary `gen_i18n.py` entrypoint/SCons branch remains raw and unchanged;
`generate_static_variant` is invoked only by the separate private generator.
The target header refuses disabled opt-in and unsupported mandatory early-PSRAM
external-BSS configuration; the integrated owner also requires boot HW init,
boot init, protection and memtest. The separate experimental environment requests
the six owner-authorized settings and fences the first setup statement with
hydration/abort. Ordinary environments remain raw. The decoder ABI assertion is
a **pending target gate**, not a measured target result. Setup runs after framework
startup/tasks; this fence is not proof of pre-task or global-constructor safety.

The boot owner reserves permanent typed backing and a typed fixed decoder.
`hydrateBuiltins()` decodes and validates all 110 streams before setting Ready;
failure is terminal and does not invoke UI, fonts, locale fallback or recovery.
The original descriptors and static array references necessarily retain fixed
addresses even before hydration. Therefore the caller's successful boot fence,
not a memory-access guard, is the publication boundary: absolutely no backing
consumer may run before it. This owner is single-boot-thread-only; it does not
add concurrency semantics to existing language switching. Retry after failure,
lazy language hydration and eviction are deliberately unsupported.

Host tests **do not prove** supported target SDK settings, external-BSS placement,
early zeroing, pre-fence framework/global-consumer safety, decoder target ABI,
actual map/flash fit, PSRAM physical size, available heap/workload headroom,
cache-disabled safety, stack high-water, boot/wake latency or device behavior.
Those remain independent target/physical acceptance gates. Manual CI routing is
non-distributing and retains encrypted diagnostics; it does not authorize hardware
execution or release. Frozen evidence is outside Git in the task evidence directory.
