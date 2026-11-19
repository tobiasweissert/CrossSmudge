---
name: custom-fonts
description: Work on Crosscub bitmap and runtime SD fonts without changing saved IDs.
---

# Crosscub fonts

Use docs/sd-card-fonts.md and actual SdCardFontSystem/registry/renderer APIs.
Keep owner Lexend/Bitter IDs and bitmap assets; modern Noto is additive. Only
upstream TtfEpdFont/FreeInkFont is selected; do not add HalScalableFont as a second
engine. Discover cpfont/ttf/otf/ttc from SD; web uploads accept cpfont/ttf/otf only.
Reader vector sizes are 8–22pt; CSS variants use the runtime's bounded range.

Register variants only while source bytes outlive them, unregister before release,
and serialize font/profile mutation with layout/render and storage/radio ownership.
Use family.availableSizes(), not vector file.pointSize=0, for choices. Source hashes
and profiles affect cache identity. Only twoBit/cacheKiB/maxGlyphs have implemented
profile adapters; hinting/weight/slant/stem-darkening and a profile UI are not done.

Run applicable font-finish host tests; disclose real vector pixels, target compile,
SD latency/memory and physical-device qualification separately. Preserve all font
licenses and do not call derived registry format documentation a durable source file.
