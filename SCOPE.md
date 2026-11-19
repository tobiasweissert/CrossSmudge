# Crosscub scope and migration readiness

Crosscub is the active owner brand; no replacement name has been chosen.
Product target: **Seeed reTerminal Sticky only** (ESP32-S3). SDK board profiles
are dependency internals, not Crosscub product targets. Stable **1.6.5.5 remains
unchanged**. This direct-upstream candidate is **experimental, not release-ready**.

## Architecture and provenance

- Direct technical upstream: CrossPoint develop `9ac3475733e677b7f61ba3f6e8ce41ce1b9b2967`.
- Selective CrossInk source: `9914146eeae7b46b300f475a16c32426fc02ec1f`.
- Owner preservation baseline: `818b0befac4d9249c7abded67fa980187c4b41af`.
- FreeInk SDK: `634a3f8c067a3714a6a7c622015138af096ab5d2`;
  pioarduino platform 55.03.311, Core 6.1.19 outer and nested.

This is a fresh CrossPoint foundation with selected Ink and retained Crosscub
behavior, not mechanical replay through historical forks. Keep upstream fixes,
credits and licenses; do not fake ancestry by discarding modern features.

## Preservation requirements

Owner Home/welcome bear, grouped Settings, reader touch/preview/Quick Lock and
existing navigation routes remain. Preserve all 41 owner Store apps and complete
package assets/licenses, Lua/API7/saves, Browser 0.3.5 keyless/TLS/SSRF/cancellation,
AirDrop one-file fresh painted consent/30MB/no-overwrite/seamless Wi-Fi handoff,
file actions, simplified reading heatmap, owner OTA and manual SD recovery.
Legacy `.crosspoint`, `.crosssmudge`, `.smudge` names are compatibility identifiers;
never erase SD/NVS or durable book/app state for a rebrand or derived cache rebuild.

## New-feature ledger — source is not release acceptance

| Feature family | Candidate source / actual boundary | Remaining gate |
| --- | --- | --- |
| BLE page turners | Modern pairing/config/runtime and painted-reader admission; teardown does not stop Sync | Combined target/radio/physical contacts and AirDrop handoff |
| Declarative SD plugins | Modern manifests/jobs/subscriptions/web relay alongside Lua apps; queued lifecycle events | Independent PluginHttp/relay TLS/SSRF/security review; sleep networking default-off |
| OPDS 2 and custom headers | SDK Atom+JSON interfaces; verified shared HTTP transport and origin-bound secrets; owner naming retained | Full SDK compile, auth/redirect/device tests; overwrite/resume/recovery not complete |
| KOReader | Modern profiles/headers/metadata interfaces with owner metadata consent | Durable clipping sync/UI and actual end-to-end transport qualification |
| EPUB CSS/images | Modern dropcaps/smallcaps/ruby/RTL/box styles/GIF plus owner tables/selection, cooperative images | Final parser/layout composition, cache versions, font pixels and vendor JPEG sanitizer defect |
| Fonts | One upstream TtfEpdFont/FreeInkFont runtime; bitmap IDs/assets preserved; OTF/TTC discovery; vector 8–22pt reader | Actual vector rendering, memory/SD lifecycle; no full Ink hinting/weight/slant controls or profile UI |
| Library | Modern index/routes plus persisted Ink metadata/filter preferences | Real Ink indexed view/search/sort/filter parity and mutation invalidation |
| TXT/Markdown/JPEG | Modern engine interfaces/JPEG viewer coexist; TXT retains legacy raw progress | TXT anchor conversion is not implemented; combined routing and conversion transactions |
| Status bars/controls | Independent seven-slot configuration, side hold bindings, completed-contact edge actions | Actual reader status rendering/collision; full live edge dragging not implemented |
| Timezone/About | Modern timezone/DST and About route with owner quarter-hour fallback | RTC stays UTC for TLS; physical date/day/heatmap and UI qualification |
| Stats/persistence | Owner pace/history/heatmap; per-book stats-off; explicit migration/recovery guards | Full metadata moves/delete/reset, durable error acknowledgement, FAT interruption/recovery |
| Web/security | Modern plugin/fetch/protected routes plus owner applications; credentials denied independently of hidden preference | Enum display-index/raw POST, value16 and save acknowledgement; independent transports and publication limits |
| SDK/memory/display | New SDK APIs/dependencies/security overlays retain owner memory and refresh policies | Standard Sticky BIN+ELF compile, actual resolved toolchain, trust/display/power tests |

## Delivery/security gates

Normal PR or manual CI compiles only standard `sticky`; no release/Pages/Store
publication permission. Retain exact BIN+ELF, build log, source/SDK/toolchain manifest
and image-validation result immediately in access-controlled Actions artifacts.
A passing compile is not physical acceptance. This reconciliation has not run CI.

Stable publication remains an explicit owner operation from approved main ancestry,
with committed numeric descriptor/version, `Crosscub` project, `sticky` board,
`crosscub-v<V>` tag and `crosscub-sticky-v<V>.bin` asset. Validate exact ESP framing,
slot size, chip, descriptor, XOR checksum, appended hash and full-file SHA-256;
verify owner URL/tag/size/digest before publication. Never overwrite an existing
release, promote a feature build or redirect OTA to another product. TLS verification,
public-address SSRF admission, numeric pinning and secret isolation are not optional.

Independent combined-source review, full selected-SDK build, simulator/real-font
captures, physical input/radio/display/SD/power tests and separately approved release
remain mandatory. Marker-free source and portable doubles do not satisfy these gates.
