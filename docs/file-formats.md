> **Experimental migration candidate — not release-ready.** Crosscub remains Sticky-only.
> This branch uses pinned CrossPoint develop directly, selectively adopts CrossInk,
> and preserves Crosscub behavior. Stable Crosscub **1.6.5.5 is unchanged**.
> Source/host checks do not establish a complete firmware build or device acceptance.
> See [the feature and readiness ledger](../SCOPE.md).

# Candidate persistence contracts and historical format reference

This migration does **not** have a single verified combined wire specification.
The historical upstream reference below describes pinned upstream, not permission
to reinterpret owner durable bytes. Consult the actual serialized source before
writing a converter. Source reconciliation alone does not prove downgrade safety.

| Area | Candidate contract / remaining gate |
| --- | --- |
| Global settings | Owner `/.crosspoint/crosssmudge-settings.json` wins; recovery roots block unsafe retries; generic import is explicit. |
| Reader settings | Version 10 keeps the owner version-9 prefix and appends character spacing, percentage word spacing and indentation (three bytes). |
| Section/style caches | RUBY_CONTINUE=64, SMALL_CAPS=128; complete **and partial** version invalidation must be coordinated with the final Section.cpp. Historical versions below are not candidate acceptance. |
| CSS rules | Candidate CSS cache version 20 includes owner backgroundBlack; derived cache only. |
| Library index | CLX1 alone is insufficient: Ink format6/fold3 differs from upstream format2/fold5; never erase durable metadata to rebuild it. |
| Clippings | Owner v4 and upstream v4 differ. Candidate v5 union migration retains table/source/sync data and bounded deletion IDs; old versions are read-only until checked save. No older-firmware downgrade claim. |
| Font registry | Private derived catalog schema 2 stores bitmap/vector/loose/root metadata; v1 rebuilds. |
| Vector profiles | `CTP1`, bounded 24 families, only twoBit/cacheKiB/maxGlyphs; retained `.tmp/.bak` block automatic publication/recovery. |
| Statistics | Preserve per-book v1..v5, global pace/history and `reading_stats_off` plus pending marker on moves; disabling tracking never clears history. |
| Legacy TXT | Preserve raw byte-offset progress; do not reinterpret as generated EPUB anchors. |
| Optimizer | Existing PXC2/COIX sidecars remain owner transport, not upstream section cache bytes. See owner historical reference below. |

FAT rename/close/readback uncertainty preserves all roots and stops mutation.
Power-loss, complete file/metadata transactions and recovery lifecycle remain unqualified.

## Pinned upstream historical reference (not candidate wire authority)

# File Formats

These formats describe the SD-card cache files under `/.crosspoint/epub_<hash>/`.
All POD fields are written in the ESP32 little-endian representation used by
`Serialization.h`; strings are length-prefixed UTF-8 unless a format notes a
fixed-size char buffer.

## `/.crosspoint/sleep-image-index/<directory-hash>-{bmp,all}.idx`

### Version 1

`ImageFolderIndex` (`src/activities/boot_sleep/ImageFolderIndex.{h,cpp}`) keeps
a compact, rebuildable index per indexed folder. It backs both the sleep-image
folder and the boot-screen folder (`/bootscreen` or `/.bootscreen`); the
directory name predates the boot-screen use and was kept as-is so existing
sleep caches aren't orphaned by an unrelated rename. The index avoids walking
a folder on every selection while using only one fixed-size record at a time
in RAM. `bmp` contains BMP files and `all` contains BMP and PNG files (sleep's
Page Overlay mode only; the boot screen is BMP-only and always uses a `bmp`
index). The `validated` header flag means BMP headers were checked while
rebuilding after a failed render.

The index is disposable: a missing, malformed, or stale selected entry causes
one rebuild and then the caller falls back to its directory scan. File
transfer, file-browser, and preferred-folder changes invalidate affected
indexes via `ImageFolderIndex::invalidateForPath()`. Files added or changed
directly on the SD card have no notification path; they are picked up when a
cached entry is found missing or when an index is otherwise rebuilt.

```c++
struct ImageFolderIndexHeader {
    char magic[4];       // "CSIX"
    u8 version;          // 1
    u8 flags;            // bit 0: BMP+PNG, bit 1: BMP headers validated
    u16 pathLength;
    u16 recordCount;
    u16 recordSize;      // sizeof(ImageFolderIndexRecord)
    u32 recordsOffset;   // sizeof(header) + pathLength
    char directory[pathLength];
};

struct ImageFolderIndexRecord {
    u16 nameLength;
    u8 flags;             // bit 0: PNG (otherwise BMP)
    u8 reserved;
    char name[256];      // zero-padded UTF-8 filename, max 255 bytes
};
```

## `book.bin`

### Version 10

`book.bin` stores EPUB metadata plus lookup tables for spine and TOC entries.
The current firmware writes this version from `BookMetadataCache`.
Version 9 stores book and TOC title strings NFC-composed so decomposed
diacritics render correctly with device fonts. It also rebuilds metadata after
the EPUB guide start-reference handling changed.

ImHex pattern:

```c++
import std.mem;
import std.string;
import std.core;

#define EXPECTED_VERSION 10
#define MAX_STRING_LENGTH 65535

struct String {
    u32 length [[hidden, comment("String byte length")]];
    if (length > MAX_STRING_LENGTH) {
        std::warning(std::format("Unusually large string length: {} bytes", length));
    }
    char data[length] [[comment("UTF-8 string data")]];
} [[sealed, format("format_string"), comment("Length-prefixed UTF-8 string")]];

fn format_string(String s) {
    return s.data;
};

struct Metadata {
    String title [[comment("Book title")]];
    String author [[comment("Book author")]];
    String language [[comment("Book language code")]];
    String coverItemHref [[comment("Path to cover image")]];
    String textReferenceHref [[comment("Path to guided first text reference")]];
};

struct SpineEntry {
    String href [[comment("Resource path")]];
    u32 cumulativeSize [[comment("Cumulative uncompressed spine size through this entry")]];
    s16 tocIndex [[comment("Index into TOC, or inherited/previous TOC index when no direct entry exists")]];
};

struct TocEntry {
    String title [[comment("Chapter/section title")]];
    String href [[comment("Resource path")]];
    String anchor [[comment("Fragment identifier")]];
    u8 level [[comment("Nesting level")]];
    s16 spineIndex [[comment("Index into spine (-1 if none)")]];
};

struct BookBin {
    u8 version;
    if (version != EXPECTED_VERSION) {
        std::error(std::format("Unsupported version: {} (expected {})", version, EXPECTED_VERSION));
    }

    u32 lutOffset [[comment("Offset to lookup tables")]];
    u16 spineCount;
    u16 tocCount;

    Metadata metadata;

    u32 currentOffset = $;
    if (currentOffset != lutOffset) {
        std::warning(std::format("LUT offset mismatch: expected 0x{:X}, got 0x{:X}", lutOffset, currentOffset));
    }

    u32 spineLut[spineCount] [[comment("Spine entry offsets")]];
    u32 tocLut[tocCount] [[comment("TOC entry offsets")]];

    SpineEntry spines[spineCount];
    TocEntry toc[tocCount];
};

BookBin book @ 0x00;

u32 fileSize = std::mem::size();
u32 parsedSize = $;
if (parsedSize != fileSize) {
    std::warning(std::format("Unparsed data detected: {} bytes remaining at offset 0x{:X}", fileSize - parsedSize, parsedSize));
}
```

## `reader_settings.bin`

### Version 9

Each EPUB cache directory may contain `reader_settings.bin`. Missing files mean
the book uses global Reader settings and the default auto-page-turn interval.

Version 1 stored only:

- `u8 version`
- `u16 autoPageTurnSeconds`

Version 2 stores flags before the full reader-settings snapshot. Version 3 adds
the EPUB word-spacing level to that snapshot. Version 4 adds the EPUB indexing
method (`0` = incremental, `1` = full section). Version 5 appends a per-book
dictionary SD-font family name. Version 6 stores reader font sizes as physical
point sizes, version 7 appends the dictionary font's selected point size, and
version 8 splits the screen margin into vertical and horizontal values. Version
9 removes the obsolete per-book Dark Mode byte: Dark Mode is now a global
display setting.
This lets the
file preserve an auto-page-turn interval without forcing custom font/layout
settings for the book. It also stores a per-book EPUB render mode override,
which can be changed from book action menus before opening the book so a
problematic EPUB can be moved to Balanced or Light rendering without entering
the reader first. Safe Mode also uses this file to save Light rendering with
embedded styles, Focus Reading, and Guide Dots disabled after that final
fallback successfully opens a difficult book.

```c++
struct ReaderSettingsBin {
    u8 version; // 9
    u8 flags;   // bit 0 = custom reader settings, bit 1 = custom auto-page-turn interval, bit 2 = render mode override, bit 3 = dictionary font override
    u16 autoPageTurnSeconds;
    u8 renderMode; // 0 = Crosscub Default, 1 = Balanced, 2 = Light

    u8 fontFamily;
    u8 readerFontPointSize; // physical point size; versions 2-5 stored a size slot
    u8 lineHeightPercent;
    u8 wordSpacing; // 0 = natural font spacing; 1-4 widen each gap by ~75% per level
    u8 orientation;
    u8 screenMarginVertical;
    u8 screenMarginHorizontal;
    u8 publisherPageNumbers;
    u8 paragraphAlignment;
    u8 embeddedStyle;
    u8 hyphenationEnabled;
    u8 textAntiAliasing;
    u8 imageRendering;
    u8 extraParagraphSpacing;
    u8 forceParagraphIndents;
    u8 focusReadingEnabled;
    u8 guideReadingEnabled;
    u8 snapshotRenderMode;
    u8 indexingMethod; // 0 = incremental, 1 = full section
    char sdFontFamilyName[64];
    char dictionarySdFontFamilyName[64]; // meaningful only when flag bit 3 is set
    u8 dictionaryFontPointSize; // 0 = follow reader size
};
```

## `/.crosspoint/clippings/<bookType>_<crc32(path)>.bin`

### Versions 1-4

Clipping files store the per-book EPUB clipping list used by the reader. A
saved clipping is also what Crosscub renders as an in-reader highlight; there is
no separate highlight file. The file lives in `/.crosspoint/clippings/` instead
of the EPUB render-cache directory so clearing/rebuilding layout cache does not
delete user clippings.

The current implementation only writes EPUB clipping files, so `bookType` is
`epub`. The numeric suffix is `uzlib_crc32()` of the book's SD-card path, for
example:

```text
/.crosspoint/clippings/epub_1234567890.bin
```

Binary layout:

- `[0]` version (`1`, `2`, `3`, or current version `4`)
- `[1-2]` clipping count (`uint16_t` LE, maximum `256`)
- book title (`String`)
- book author (`String`)
- book path (`String`)
- repeated clipping records:
  - `spineIndex` (`uint16_t` LE)
  - `startPage` (`uint16_t` LE)
  - `endPage` (`uint16_t` LE)
  - `pageCount` (`uint16_t` LE, at least `1`)
  - `startWordIndex` (`uint16_t` LE)
  - `endWordIndex` (`uint16_t` LE)
  - `wordCount` (`uint16_t` LE)
  - `paragraphIndex` (`uint16_t` LE, `UINT16_MAX` when unavailable)
  - `timestamp` (`uint32_t` LE, seconds since firmware boot when saved)
  - versions 3-4: reader layout signature (`uint32_t` LE; font, spacing,
    viewport, and other section-layout inputs)
  - version 4: table selection (`uint16_t` LE; `UINT16_MAX` for non-table text)
  - `chapterTitle` (`char[48]`, null-terminated/truncated)
  - version 1: selected text (`String`; legacy files were written with a
    `512`-byte in-app limit)
  - versions 2-4: selected-text length (`uint16_t` LE) followed by that many
    UTF-8 bytes (the current in-app limit is `4096` bytes, defined by
    `CLIPPING_TEXT_MAX`)

The clipping selector has a separate navigation bound: it exposes at most
`240` visible words from at most three pages. This is a bounded in-memory
selection window for low-memory devices, not a character-count limit. The
selected text is still stored separately and is limited to `4096` UTF-8 bytes.

Crosscub uses the stored spine/page/paragraph fields as anchors, then searches
near that location for the stored clipping text after relayout. This is similar
to keeping both a DOM position and a text quote in a web app: the numeric
position gives a fast starting point, while the text makes jumps and highlights
survive font, layout, or page-count changes when possible.

Version 3 records which reader layout produced the numeric page/word anchor.
When that signature differs, Crosscub ignores the stale numeric range and
matches the saved text instead, including when both layouts happen to have the
same total page count. Legacy records without a layout signature use text
matching rather than trusting ambiguous numeric ranges. Version 4 adds the
table selection field; versions 1-3 remain readable.

Creating a clipping also appends a Kindle-style export entry to
`/My Clippings.txt` on the SD-card root. That text export can keep up to `2000`
bytes of the selected text and is append-only. Removing a clipping from the
reader deletes or rewrites only the binary clipping file; it does not remove
previous entries from `/My Clippings.txt`.

When Crosscub moves an EPUB through its built-in move-to-Read flow, it rewrites
the clipping file under the new path-derived name and removes the old one. If a
book is renamed or moved outside Crosscub, the path hash changes, so the old
clipping file may no longer be associated with the book until the file is moved
back or the clipping store is migrated.

## `stats_v5.bin`

### Version 5

`stats_v5.bin` stores per-book reading statistics for stats schema version 5.
Versioned filenames let firmware branches with different stats schemas keep
their own per-book stats files without overwriting each other. Version 5 extends
version 4 with a cached live reader book time-left estimate so Home and Reading
Stats can show the same estimate the reader last computed.

When `stats_v5.bin` is missing, Crosscub can read the previous versioned stats
filename (`stats_v4.bin` for version 5, `stats_v5.bin` after a future version 6
bump) before falling back to legacy `stats.bin` files with compatible stats
payloads. Future changes are always saved to the current versioned filename.

Binary layout:

- `[0]` version (`5`)
- `[1-2]` `sessionCount` (`uint16_t` LE)
- `[3-6]` `totalReadingSeconds` (`uint32_t` LE)
- `[7-10]` `totalPagesTurned` (`uint32_t` LE)
- `[11]` `isCompleted` (`uint8_t`)
- `[12-13]` `avgSecondsPerForwardPage` (`uint16_t` LE)
- `[14-15]` `paceSampleCount` (`uint16_t` LE)
- `[16]` flags (`bit0=startDateManual`, `bit1=finishedDateManual`)
- `[17-20]` `startDate` (`year uint16_t` LE, `month uint8_t`, `day uint8_t`)
- `[21-24]` `finishedDate` (`year uint16_t` LE, `month uint8_t`, `day uint8_t`)
- `[25-40]` `timeOfDaySeconds[4]` (`uint32_t` LE each)
- `[41-68]` `dayOfWeekSeconds[7]` (`uint32_t` LE each)
- `[69-72]` `estimatedTimeLeftSeconds` (`uint32_t` LE, `0` means unavailable)

## `section.bin`

### Version 74

Version 74 keeps the binary layout and rebuilds completed and partial sections.
Runtime TrueType advances no longer round up before the final pixel snap, which
can change cached line breaks and word positions. Bitmap fonts and `.cpfont`
files are unchanged. Book metadata and saved progress are kept.

### Version 73

Version 73 keeps the binary layout and rebuilds completed and partial sections.
TrueType vector fonts now render and advance the .notdef glyph for missing
characters instead of omitting them. Cached line breaks and word positions change
for text containing glyph fallbacks. Book metadata and saved progress are kept.

### Version 72

Version 72 makes ruby layout closer to how Chromium browser render text with
ruby characters by default (`ruby-align: space-around; ruby-overhang: auto;`)
Ruby annotation can overhang neighboring base text by at most the smaller of
half the ruby edge-character width and 1/4 of the neighboring word width. When
the ruby annotation is wider than its base text, spacing is inserted between
every character in the base text as well as at the start and the end of the
base text so that it becomes the same length as the ruby annotation.

### Version 71

Version 71 keeps the binary layout and rebuilds completed and partial sections.
Focus split offsets now locate the bold prefix in RTL words and the regular
suffix in LTR words. Book metadata and saved progress are kept.

### Version 70

Version 70 preserves the binary layout but rebuilds completed and partial
sections because SD-font layout now rounds each scaled glyph advance to pixels,
matching unkerned drawing. Cached line breaks and word positions change.
Book metadata and saved progress are kept.

### Version 69

Version 69 preserves the binary layout but rebuilds completed and partial
sections because line spacing now multiplies font size instead of the font's
newline distance (`advanceY`). Clipping layout signatures also distinguish the
new calculation so old page-local word ranges are not reused for a new layout.
Book metadata and saved progress are kept.

### Version 68

Version 68 preserves the binary layout but rebuilds completed and partial
sections to remove mid-line hyphens left by drop-cap layout. The lines beside
the drop cap use bounded greedy wrapping; the rest keep their usual wrapping.
Book metadata and saved progress are kept.

### Version 67

Version 67 preserves the binary layout but rebuilds completed and partial
sections for corrected justification of visually reordered text with attached
spaces. Book metadata and saved progress are kept.

### Version 66

Version 66 preserves the binary layout but rebuilds completed and partial
sections to remove leading non-breaking spaces carried across soft line wraps.
Explicit paragraph-leading spaces, book metadata, and saved progress are kept.

### Version 65

Version 65 preserves the binary layout but rebuilds completed and partial
sections for corrected justification of non-breaking spaces after hyphens and
ellipses. Book metadata and reading progress are unchanged.

### Version 64

Version 64 preserves the binary layout but rebuilds completed and partial
sections for corrected explicit font-weight/font-style inheritance. It retains
version 63's GIF rendering and paragraph-boundary fixes. CSS cache version 19 stores
`inherit` as value 2 in the existing font-style and font-weight bytes; the
winning declaration is resolved against the parent before applying the element's
style. Older CSS caches are rebuilt. Book metadata and reading progress are
unchanged.

### Version 63

Version 63 retains the version 60 rendering format and rebuilds completed and
partial sections to include previously omitted GIFs and their alternative text.
Earlier GIF/table development builds used versions 60 through 62; their caches
also rebuild to preserve both the current rendering format and corrected
paragraph boundaries. The partial-build sentinel remains derived from the final
section version. Book metadata and saved progress are kept, but pagination can
change as GIFs become visible.

### Version 60

The serialized layout is unchanged. Text that runs straight into a block or
heading start (`text<div>…`) now stays in its own paragraph instead of being
joined to the first word of the new block. Older completed and partial section
caches rebuild automatically; book metadata and reading progress are kept.

### Version 59

The serialized layout is unchanged. Stored CSS rules now apply even below
48 KiB free heap. Older completed and partial sections rebuild because they may
contain missing styles, different line breaks, or content that should be hidden.
Book metadata, CSS rule caches, and reading progress are kept.

### Version 58

The serialized layout is unchanged. Completed and partial sections rebuild for
corrected rendering layout and footnote placement. Book metadata and reading
progress are kept.

### Version 57

Version 57 combines the rendering layout below with line breaks after explicit
hyphens and dashes. It invalidates version 56 caches from both branches, which
used different layouts under the same version. Completed and partial sections
are rebuilt.

### Version 56

Version 56 adds the rendering layout below on top of version 55.

Each TextBlock's BlockStyle stores a signed 32-bit `fontId` after
`characterSpacing`: the renderer font the line was laid out in, or 0 for the
section font. Each TextBlock ends with a `hasWordFonts` flag. When set, it is
followed by three signed 32-bit font ids and one slot byte per word: 0 draws the
word in the block's font, `n` in font `n - 1`; bit 0x80 of a slot marks CSS
small caps (the word text keeps its original case). Mixed-size words share the baseline
of the tallest font. SD (.cpfont) and vector fonts use sized variants for
headings, CSS font-size blocks, inline runs and drop caps. A font-size span
holding a whole paragraph sizes that paragraph.

Pages gain two element types. `TAG_PageDropCap` (4) is an initial letter drawn
scaled up from a font's glyph: position (its `yPos` is the baseline of the last
spanned line), the renderer font id, the scale in 1/256ths, the word style and
up to 12 bytes of UTF-8 text. `TAG_PageBorderBox` (5) is a CSS border and/or
background shade around one page's slice of a block element: position, size,
width and style for the top, right, bottom and left edges (a side split off by a
page break has width 0), and a shade flag.

Layout changes keep headings with following content and avoid leaving a
paragraph's first line alone at a page bottom or its last line alone at a page
top. Preserved whitespace and inherited block emphasis affect word placement
and styles. Inline `<br>` breaks omit paragraph spacing, while standalone and
consecutive breaks add blank lines.

Table columns are sized from a markup pre-scan rather than equal widths.
Bordered tables frame each grid cell with `PageBorderBox` elements. Cells honor
CSS padding in grid and stacked layouts. Plain-text rows share a 160-token /
2 KB budget; rows containing blocks or images use full-width flow to preserve
paragraphs and illustrations. Older sections are rebuilt for the serialized
font data, page elements and layout changes.

CSS cache version 16 adds a boolean `preserveWhitespace` byte after `initialLetter`
and uses defined-property bit 30 for `white-space`.
CSS cache version 17 stores rules in source order and retains nonadjacent repeated
selectors so equal-specificity declarations cascade in stylesheet order.

CSS cache version 18 adds a border-component byte after the four border sides.
Bits 0-3 mark declared widths and bits 4-7 mark declared styles, in top, right,
bottom, left order. Width and style declarations cascade independently.

### Version 55

Each TextBlock adds a uint16 `paragraphStartWord` after `textBytes`. It is the
visual index of the paragraph's first logical word, or `UINT16_MAX` for a
continuation line. Clipping uses this marker independently of source-offset
gaps. Older completed and partial section caches rebuild automatically;
book metadata and reading progress are kept.

### Version 54

The serialized layout is unchanged. Word source ranges and split offsets now
include codepoints absorbed by NFC composition. The high bit of each word's
style byte marks a discretionary hyphen, so clipping can remove it independently
of source length. Rebuild completed and partial section caches to correct
clipping spaces and anchors for decomposed text.

### Version 53

Each TextBlock arena starts with one 8-byte source range per word (two uint32
chapter-visible Unicode-codepoint offsets, start inclusive and end exclusive).
Ranges follow words through BiDi ordering and line wrapping. This version also
includes the version 52 redaction layout changes. Older completed and partial
section caches are rebuilt automatically; book and progress files are kept.

### Version 52

The serialized layout is unchanged. Missing full-block (`U+2588`) and black-square
(`U+25A0`) symbols now use font-sized solid rectangles instead of replacement
glyphs. Rebuild older sections so cached line breaks and word positions match
their new widths.

### Version 51

The serialized layout is unchanged. Paragraph continuity and top spacing are
preserved across soft flushes. Older sections are rebuilt for the updated layout.

### Version 50

The header adds `paragraphIndentSpaces` after `extraParagraphSpacing`. The value
participates in cache validation, so sections with different indentation settings
are rebuilt. Version 49 was used by pre-release builds with a different header
layout and is skipped to prevent reuse of those caches.

### Version 48

Version 48 keeps the version 47 serialized layout unchanged. It was bumped
because Hangul text no longer has implicit line-break opportunities between
syllables: Korean words wrap at spaces (like CSS `word-break: keep-all`), and
with hyphenation enabled a word may also split at the end of a line wherever the
CJK line-breaking rules allow, without an inserted hyphen. Justification stretches only word spaces. Cached line breaks and word
positions from version 47 no longer match.

### Version 47

The section header adds signed `characterSpacing` (pixels) and unsigned
`wordSpacingPercent` after `focusReadingEnabled`; both participate in cache
validation. Each TextBlock's BlockStyle stores only `characterSpacing` after
`directionDefined`. Word spacing is resolved into cached word positions during
layout. Sections from earlier versions are rebuilt.

### Version 46

Version 46 keeps the version 45 serialized layout unchanged. It was bumped
because ordered lists now number their items, `list-style-type: none`
suppresses list markers, and `<ul>`/`<ol>` containers contribute their own
margins and padding to child block insets, changing cached word contents and
page layout.

### Version 45

Version 45 keeps the version 44 serialized layout unchanged. It was bumped
because internal EPUB links now preserve CSS superscript and subscript styles,
changing their cached word-style flags and page layout.

### Version 44

Each file in `sections/*.bin` stores one laid-out spine section. The header is
also the cache-busting key: if any layout-affecting setting differs from the
current reader settings, the section is discarded and rebuilt.

Version 44 appends the internal-link rectangles produced during text layout to
each serialized page. The reader uses these rectangles for touch navigation;
older caches are rebuilt because they contain no link geometry.

Version 43 keeps the version 42 serialized layout unchanged. It was bumped
because paragraph base direction now excludes direction changes from inline
elements.

Version 42 keeps the version 41 serialized layout unchanged. It was bumped
because closing a block now strips inherited vertical margins and padding.

Version 41 keeps the version 40 serialized layout unchanged. It was bumped
because simple HTML table rows are now laid out as positioned columns rather
than flattened paragraphs with synthetic row/cell labels.

Version 40 keeps the version 39 serialized layout unchanged. It was bumped
because ruby groups now remain intact when large text blocks are soft-flushed.

Version 39 keeps the version 38 serialized layout unchanged. It was bumped
because image top margins are now clamped to keep full-height images within the
page viewport.

Version 38 keeps the version 37 serialized layout unchanged. It was bumped
because Focus Reading now permits line breaks at visible hyphens and dashes
and hyphenates focus-split words as a whole, changing cached page layout.

Version 37 increases the fixed-size footnote href field from 96 to 256 bytes.
This changes each serialized footnote record from 128 to 288 bytes, so older
section caches must be discarded and rebuilt.

Version 36 keeps the version 35 serialized layout unchanged. It was bumped
because ruby and justified text positioning and CJK line breaking now use
corrected word measurements, so version 35 cached page layouts no longer match.

Version 35 adds a header offset and a `uint32_t` entry per page for the
visible-text offset LUT. The other section LUTs remain unchanged.

Version 34 is binary-identical to version 33. The version was bumped because
word-gap suppression was narrowed to tokens glued together in the source: v33
dropped the gap between any two words meeting at a CJK break opportunity, which
collapsed the spaces between Hangul words, so v33 word positions no longer match
what the layout engine now produces.

Version 30 is binary-identical to version 29. The version was bumped because
Arabic contextual shaping changed text measurement (`getTextAdvanceX` now
measures the shaped visual text), so word positions cached by v29 no longer
match what `drawText` renders.

Version 28 introduced serialized word style bits for underline, strikethrough,
superscript, and subscript. The format also includes:

Version 57 is binary-identical to version 56. The version was bumped because
word-gap suppression now applies only to tokens glued together in the source.
Older caches could collapse explicit spaces between Hangul words, so full and
suspended partial section caches rebuild together. Version 58 recalculates
Focus Reading split-run offsets with the renderer's combined advance and
kerning rounding, so old cached page positions rebuild.

Version 56 changes `<br>` layout: a line break after text no longer reapplies
the containing block's top or bottom spacing, while an empty `<br>` block keeps
the existing scene-break gap. Full and suspended partial section caches rebuild
together. Version 55 assigns compact IDs to internal EPUB links. The ID is
stored in the existing per-word flags byte and in each page's footnote entry so
touch devices can map tapped text to the existing fragment-navigation path
without retaining another per-word data structure. Version 54 adds compact
ruby-text annotations to serialized text blocks. Only words that begin a ruby
group store annotation text; continuation words use a dedicated style bit. This
keeps books without ruby markup unchanged apart from the cache version while
avoiding an empty string allocation for every word.
Version 53 stores each image's EPUB-internal source path so section indexing can
read only its header and defer full extraction until the page is shown. Version
52 keeps Guide Dots centered when extra word spacing is enabled. Version 51
preserves continuation state for oversized CJK word fragments. Version 50
paginates chapter-heading image runs within the reader viewport so they do not
overflow into the reserved status-bar area. Version 49 stores Focus Reading
split-run offsets in visual order so RTL word prefixes render on the right.
Version 48 changed Arabic contextual shaping and text measurement, so cached
word positions from version 47 no longer match what `drawText` renders.

Version 48 makes the EPUB word-spacing level widen the natural inter-word gap
(each level adds 10 pixels), which changes laid-out word positions, so
older sections must rebuild. Version 46 added the EPUB word-spacing level to the
cache-busting header. It retains the flat `TextBlock` arena and chapter-opener
anchor behavior introduced in version 45. It includes:

- cache-busting fields for font, line compression, extra paragraph spacing,
  forced paragraph indents, paragraph alignment, viewport size, hyphenation,
  embedded CSS, image rendering mode, Focus Reading, Guide Dots, word spacing,
  and EPUB render mode
- page offset LUT
- per-page visible-text offset LUT (zero-based Unicode codepoints in `<body>`)
- anchor-to-page map for fragment and footnote navigation
- paragraph and list-item LUTs retained for navigation and legacy sync fallback
- optional per-word Focus Reading split metadata
- optional per-word Guide Dot x-offset metadata
- optional per-word text flags for CSS backgrounds, layout-inserted hyphens,
  and internal-link IDs
- reading-aid layout that stores Focus Reading and Guide Dots as per-word metadata instead of temporary layout words
- publisher CSS page-break handling and adjusted justification spacing baked into page layout
- table fragments
- per-page footnote entries
- serialized word style bits for underline, strikethrough, superscript, and
  subscript
- flat TextBlock word storage (v29): per-word arrays plus one shared
  NUL-terminated text blob, replacing v28's length-prefixed word strings. The
  on-disk order mirrors the in-RAM arena so the firmware reads a whole block
  payload with a single allocation and a single SD read

ImHex pattern:

```c++
import std.mem;
import std.string;
import std.core;

#define EXPECTED_VERSION 64
#define MAX_STRING_LENGTH 65535
#define FOOTNOTE_NUMBER_LEN 32
#define FOOTNOTE_HREF_LEN 256

struct String {
    u32 length [[hidden, comment("String byte length")]];
    if (length > MAX_STRING_LENGTH) {
        std::warning(std::format("Unusually large string length: {} bytes", length));
    }
    char data[length] [[comment("UTF-8 string data")]];
} [[sealed, format("format_string"), comment("Length-prefixed UTF-8 string")]];

fn format_string(String s) {
    return s.data;
};

enum PageElementTag : u8 {
    TAG_PageLine = 1,
    TAG_PageImage = 2,
    TAG_PageHorizontalRule = 3,
    TAG_PageDropCap = 4,
    TAG_PageBorderBox = 5
};

enum WordStyle : u8 {
    REGULAR = 0,
    BOLD = 1,
    ITALIC = 2,
    BOLD_ITALIC = 3,
    UNDERLINE = 4,
    STRIKETHROUGH = 8,
    SUP = 16,
    SUB = 32,
    DISCRETIONARY_HYPHEN = 128  // trailing hyphen inserted by line breaking
};

enum TextAlign : u8 {
    JUSTIFIED = 0,
    LEFT_ALIGN = 1,
    CENTER_ALIGN = 2,
    RIGHT_ALIGN = 3,
    NONE = 4
};

struct BlockStyle {
    TextAlign alignment;
    bool textAlignDefined;
    s16 marginTop;
    s16 marginBottom;
    s16 marginLeft;
    s16 marginRight;
    s16 paddingTop;
    s16 paddingBottom;
    s16 paddingLeft;
    s16 paddingRight;
    s16 textIndent;
    bool textIndentDefined;
    bool isRtl;
    bool directionDefined;
    s8 characterSpacing;
    s32 fontId;
};

struct TextBlock {
    u16 wordCount;
    u8 hasFocus;
    u16 textBytes [[comment("Total size of text[], including one NUL per word")]];

    if (wordCount > 0) {
        u16 textOff[wordCount] [[comment("Byte offset of word i's text within text[]")]];
        s16 wordXPos[wordCount];
        if (hasFocus != 0) {
            u16 wordFocusSuffixX[wordCount] [[comment("Suffix x offset from word start")]];
        }
        WordStyle wordStyle[wordCount];
        if (hasFocus != 0) {
            u8 wordFocusBoundary[wordCount] [[comment("UTF-8 byte boundary between bold prefix and suffix")]];
        }
        char text[textBytes] [[comment("All words back to back, each NUL-terminated")]];
    }

    BlockStyle blockStyle;
    u8 hasWordFonts;
    if (hasWordFonts != 0) {
        s32 wordFonts[3];
        u8 wordFontSlot[wordCount] [[comment("0 = block font, n = wordFonts[n - 1]; bit 0x80 = small caps")]];
    }
};

struct ImageBlock {
    String imagePath;
    String srcPath;
    s16 width;
    s16 height;
};

struct PageLine {
    s16 xPos;
    s16 yPos;
    TextBlock block;
};

struct PageImage {
    s16 xPos;
    s16 yPos;
    ImageBlock image;
};

struct PageHorizontalRule {
    s16 xPos;
    s16 yPos;
    u16 width;
    u8 thickness;
};

struct PageDropCap {
    s16 xPos;
    s16 yPos;  // baseline of the last spanned line
    s32 fontId;
    u16 scale256;
    WordStyle style;
    u8 length;
    char text[length];
};

struct BorderSide {
    u8 width;
    u8 style;  // 0 none, 1 solid, 2 double, 3 dotted, 4 dashed
};

struct PageBorderBox {
    s16 xPos;
    s16 yPos;
    u16 width;
    u16 height;
    BorderSide sides[4];  // top, right, bottom, left
    bool shaded;
};

struct PageElement {
    PageElementTag pageElementType;
    if (pageElementType == TAG_PageLine) {
        PageLine pageLine [[inline]];
    } else if (pageElementType == TAG_PageImage) {
        PageImage pageImage [[inline]];
    } else if (pageElementType == TAG_PageTableFragment) {
        PageTableFragment tableFragment [[inline]];
    } else if (pageElementType == TAG_PageHorizontalRule) {
        PageHorizontalRule horizontalRule [[inline]];
    } else if (pageElementType == TAG_PageDropCap) {
        PageDropCap dropCap [[inline]];
    } else if (pageElementType == TAG_PageBorderBox) {
        PageBorderBox borderBox [[inline]];
    } else {
        std::error(std::format("Unknown page element type: {}", pageElementType));
    }
};

struct FootnoteEntry {
    char number[FOOTNOTE_NUMBER_LEN];
    char href[FOOTNOTE_HREF_LEN];
    u8 linkId;
};

struct PublisherPageMarker {
    s16 yPos;
    char label[16];
};

struct Page {
    u16 elementCount;
    PageElement elements[elementCount] [[inline]];

    u16 footnoteCount;
    FootnoteEntry footnotes[footnoteCount];

    u8 publisherPageMarkerCount;
    PublisherPageMarker publisherPageMarkers[publisherPageMarkerCount];
};

struct AnchorEntry {
    String anchor;
    u16 page;
};

struct AnchorMap {
    u16 count;
    AnchorEntry entries[count];
};

struct ParagraphLut {
    u16 count;
    u16 paragraphIndex[count];
};

struct SectionBin {
    u8 version;
    if (version != EXPECTED_VERSION) {
        std::error(std::format("Unsupported version: {} (expected {})", version, EXPECTED_VERSION));
    }

    s32 fontId;
    float lineCompression;
    bool extraParagraphSpacing;
    u8 paragraphIndentSpaces;
    u8 paragraphAlignment;
    u16 viewportWidth;
    u16 viewportHeight;
    bool hyphenationEnabled;
    bool embeddedStyle;
    u8 imageRendering;
    bool focusReadingEnabled;
    s8 characterSpacing;
    u8 wordSpacingPercent;

    u16 pageCount;
    u32 protectedImageUnits;
    u32 pageLutOffset;
    u32 anchorMapOffset;
    u32 paragraphLutOffset;
    u32 listItemLutOffset;
    u32 visibleTextLutOffset;

    Page pages[pageCount];

    u32 currentOffset = $;
    if (currentOffset != pageLutOffset) {
        std::warning(std::format("Page LUT offset mismatch: expected 0x{:X}, got 0x{:X}", pageLutOffset, currentOffset));
    }

    u32 pageLut[pageCount] [[comment("Page data offsets")]];

    if (anchorMapOffset != 0) {
        AnchorMap anchorMap @ anchorMapOffset;
    }

    if (paragraphLutOffset != 0) {
        ParagraphLut paragraphLut @ paragraphLutOffset;
    }

    if (listItemLutOffset != 0 && paragraphLutOffset != 0) {
        u16 listItemIndex[paragraphLut.count] @ listItemLutOffset;
    }

    if (visibleTextLutOffset != 0) {
	u32 visibleTextOffset[pageCount] @ visibleTextLutOffset;
    }
};

SectionBin section @ 0x00;

u32 fileSize = std::mem::size();
u32 parsedSize = $;
if (parsedSize != fileSize) {
    std::warning(std::format("Unparsed data detected: {} bytes remaining at offset 0x{:X}", fileSize - parsedSize, parsedSize));
}
```

## CLX1 — library index (`.crosspoint/library.idx`)

Written by `lib/LibraryIndex/LibraryBuilder.cpp`, read by `LibraryIndexFile`. One
file describing every book on the card, so the shelf can sort and search
thousands of titles without opening any of them.

Format version 2. An index written by another version fails validation on open
and is rebuilt; that is the entire migration mechanism.

### Layout

| Section | Offset | Contents |
|---|---|---|
| Header | 0 | 64 bytes, `ClixHeader` |
| Folders | `folderStart` | length-prefixed paths, one per folder |
| Records | `recordStart` | `bookCount` × 128-byte `ClixRecord` |
| Permutations | `permStart` | `bookCount` u16 author order, then `bookCount` u16 arrival order |
| Name blob | `nameStart` | per record: path hash, name, canonical author, title, source author (see below) |

The arrival permutation runs oldest first, keyed by the record's FAT
modification time (when the file landed on the card); `firstSeen` — the
build-assigned discovery counter — breaks ties and carries books whose
filesystem reports no time. Fold version 3 introduced the timestamp key; a
fold bump rebuilds ranks while preserving `firstSeen`.
Fold version 4 preserves leading articles in title sort and search keys.
Fold version 5 drops all Unicode 18.0 Hebrew combining marks, including U+05C8 and U+05C9.

Sections are 512-byte aligned so each starts on an SD block boundary.

### Records are exactly 128 bytes

A fixed stride is what lets the reader seek straight to record *n* without an
offset table, and read a screenful in one 4 KB block. `static_assert` enforces it.

Each record carries `fold[96]`, the title normalised for search and sorting —
accents stripped, case dropped, leading articles preserved — and `authorKey[12]`,
the author's words folded and sorted so that "Victor Hugo" and "Hugo Victor" group as
one person. `authorKey` is a GROUPING key, not an ordering one: the shelf orders by
surname, derived separately from the display name.

The byte before the folded title records metadata extraction status: not
attempted, extracted, or failed. The final four bytes contain the packed FAT
modification date and time returned by SdFat. A zero timestamp is not trusted.
These fields occupy the alignment and reserved bytes from version 1, so the
record remains exactly 128 bytes.

The header records whether EPUB metadata extraction was enabled for the build.
This prevents a metadata-disabled rebuild from making filename fallbacks look
fresh to a later metadata-enabled build.

### The name blob

Per record, at `nameStart + nameOff`:

```text
[u64 pathHash]    FNV-1a fingerprint of the complete path
[nameLen bytes]  filename, without the directory
[u8][author]     display author, one spelling chosen per authorKey across the library
[u8][title]      the book's own title, or length 0 if it never gave one
[u8][source]     cleaned author spelling before the library-wide spelling vote
```

The filename must stay the first textual field and stay the filename: `readPath`
rebuilds a book's path from it, so writing the display title there makes the book
impossible to open. That was a real defect, and it is why title has its own field.

The source author is separate from the displayed canonical author so a later
rebuild can repeat the spelling vote after books are added or removed. Existing
display reads still stop at the author or title fields and retain their offsets.

### Freshness and unchanged rebuilds

Reconciliation treats the persisted 64-bit complete-path fingerprint as the
book identity. Metadata is reused only when the fingerprint, size, nonzero FAT
timestamp, fold version, metadata mode, and expected extraction status agree.
EPUBs with a zero timestamp or a previous extraction failure are parsed again.

If every current record reuses metadata, the old and new counts agree, and no
unreadable entry was seen, the staging files are discarded and the live index is
left byte-for-byte unchanged. A normal rebuild action is therefore a freshness
check, not a forced metadata reread.

### Header flags

`RANKS_DEGRADED` says one or more orders fell back to walk order because a
checked sort allocation failed. Title and author each use a phase-local
`SortKey[bookCount]` allocation (14 bytes per book, 57,344 bytes at the 4,096-book
format ceiling); the first array is released before the second is requested.
Sorting is therefore best effort through the full format limit rather than
being disabled at an arbitrary library size.

`DEDUP_DEGRADED` says a directory exceeded the fixed 1024-entry duplicate-key
buffer, or that its fallible 8 KiB allocation failed. The walk still indexes
every enumerated book; it only stops remembering additional identities for
duplicate-dirent detection, so a damaged FAT may expose duplicates but cannot
make a real book disappear.

`selfSize` is the expected file size. Comparing it against the real one is a free
truncation guard: a build cut short by a power failure cannot pass.

## Clipping store (`/.crosspoint/clippings/epub_<path-hash>.bin`)

Version 4 retains the version 3 header and page-local range fields. After each
record's layout signature it stores `startOffset` and `endOffset` (uint32 chapter
codepoint range, end exclusive; UINT32_MAX means unavailable), `syncRevision`
(uint64), `pendingUpload` (one byte), and a 65-byte NUL-terminated sync ID. The
chapter title, text length, and text follow. Versions 1–3 remain readable.

Stable IDs are saved before upload. A sibling `.deleted` file stores fixed
65-byte IDs awaiting server acknowledgement. Deletions are queued before the
local record is removed and retried on the next enabled manual sync. A `.bak`
file is recovered if power interrupted replacement of the main store.

Clipping header strings are limited to 4 KiB on both reads and writes. A failed
load leaves no usable index and disables writes until a successful load. The
index is allocated with checked, bounded growth and released on unload.

Book moves rename the store and its `.deleted` journal (plus recovery sidecars)
together. The stored source path is informational and is refreshed on the next
save; the current file path selects the store. Local book deletion cleans up all
of these sidecars but preserves the independent `My Clippings.txt` export.

## Retained owner optimizer reference

## Optimizer image transport (PXC2 and COIX)

Optimizer images are optional EPUB sidecars. Original JPEG/PNG assets remain the
compatibility fallback. `META-INF/crossink/optimizer-v1.json`,
`META-INF/crossink/optimizer-images-v1.idx`, and all
`META-INF/crossink/pxc/*.pxc2` entries must use ZIP STORE. PXC2 has its own block
compression; the firmware rejects ZIP-deflated PXC2 before starting an inflater.
The manifest keeps `format: "crossink-optimizer"` and `version: 1`. Each image has
`href`, `pxc`, `width`, `height`, `pxcFormat: "pxc2"`, `pxcBytes` (complete transport
size), and `pixelCrc32`. Missing `pxcFormat` means legacy raw PXC1.

All integers below are unsigned little endian. Fields are serialized explicitly,
not by writing native C++ structures. CRCs use standard IEEE CRC32 (zlib).

### PXC2

The 32-byte header is:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | `PXC2` |
| 4 | 1 | Version 2 |
| 5 | 1 | Pixel format 1: four 2-bit pixels per byte, most significant first |
| 6 | 2 | Header bytes: 32 |
| 8 | 2 | Width |
| 10 | 2 | Height |
| 12 | 2 | Row bytes: `(width + 3) / 4` |
| 14 | 2 | Block bytes: 2048 |
| 16 | 2 | Block count: ceiling of raw bytes / 2048 |
| 18 | 2 | Flags: zero |
| 20 | 4 | Raw pixel bytes |
| 24 | 4 | Raw pixel CRC32 |
| 28 | 4 | Complete PXC2 file bytes |

Dimensions are 1–1024, raw data is at most 128 KiB, and there are at most 64
blocks. Each block starts with a 12-byte header: codec (`u8`, 0 RAW, 1 raw DEFLATE,
2 PackBits), zero flags (`u8`), raw length (`u16`), encoded length (`u16`), sequence
(`u16`, starting at zero), decoded-block CRC32 (`u32`). Lengths are 1–2048. All
non-final blocks have 2048 raw bytes. Compressed blocks must be smaller than raw;
otherwise producers use RAW. Every block is independent. PackBits controls 0–127
copy the next control+1 bytes; 129–255 repeat the next byte 257-control times;
128 is rejected. Raw DEFLATE must terminate exactly, with no extra input/output.
Python uses zlib level 8 and `wbits=-15`; the browser uses PackBits/RAW.

The reader reuses a fallibly allocated session workspace (5928 bytes on the
64-bit host; firmware size is compile-time limited below 6500), verifies all
lengths/CRCs and resizes rows into `img_*.pxc.optimizer.tmp`. It checks size and
syncs/closes before publishing. On filesystems that cannot rename over an existing
file, `.optimizer.previous` retains the previous cache until publication succeeds;
failed rollback leaves that backup available for recovery on the next attempt. PXC2 never needs a full raw source temporary file.
The local raw PXC layout remains two `u16` dimensions plus packed rows. No section
cache version change is required. Hardware heap and visual results remain device
acceptance checks, not implied by host workspace accounting.

### COIX version 1

The local index is `/.crosspoint/epub_<hash>/optimizer-images.idx`. Its header is:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | `COIX` |
| 4 | 2 | Version 1 |
| 6 | 2 | Header bytes: 32 |
| 8 | 2 | Record bytes: 208 |
| 10 | 2 | Record count: 0–256 |
| 12 | 4 | Flags/reserved: zero |
| 16 | 4 | Manifest central-directory CRC32 |
| 20 | 4 | Manifest uncompressed bytes |
| 24 | 4 | CRC32 of all records |
| 28 | 4 | CRC32 of header bytes 0–27 |

Each 208-byte record contains NUL-terminated `href[129]` (offset 0),
`pxcHref[65]` (129), width `u16` (194), height `u16` (196), format `u8` (198,
1 legacy or 2 PXC2), zero flags `u8` (199), complete sidecar bytes `u32` (200),
and pixel CRC32 `u32` (204). Paths are relative, bounded UTF-8; traversal,
backslashes, control characters, colons and percent escapes are rejected.
Sidecars must be beneath `META-INF/crossink/pxc/`. Duplicate image hrefs are invalid;
producers keep one optional sidecar per href and omit unsupported/over-limit
entries. Firmware can resize that sidecar for other layouts.

Reader setup validates a packaged index and copies it through a temporary local
file, or builds it from a legacy manifest while the framebuffer is loaned. Generic
`Epub::load()` does not build indexes. A lookup scans records on SD and retains
only one last hit. Manifest identity changes invalidate the local index. Per-page
preflight reuses local pixels first, then materializes just that page's sidecars,
then extracts original assets as fallback. Legacy ZIP inflation also runs under
the framebuffer loan. Sleep-page generation uses the same preflight. Temporary
index files are cleaned during setup and per-image temporaries during preflight.

Hardware acceptance: clear only the test book's cache, open legacy/PXC2 variants
with SD font and AA, visit/revisit image pages and sleep, compare portrait and
landscape output, and record internal free/largest heap blocks and low-water
marks. Repeat corrupt sidecars, full/read-only SD, interrupted writes and book
replacement at the same path on Sticky (SPI SD).

### CSS rules cache revision 18

Revision 18 adds the serialized `list-style-type` property used to number
ordered lists and suppress list markers. It also includes the PSRAM streamed
stylesheet path introduced in revision 16, which admits sources up to 512 KiB
on PSRAM readers while C3 retains its 128 KiB limit. Existing rule-count and
internal-memory guards still apply. Rebuilding an invalid CSS cache also
invalidates section caches through the existing EPUB-load path, so books that
previously cached zero rules can restore hidden content and layout rules.

## `/.crosspoint/font-catalog.bin`

### Version 1

Disposable font metadata cache, shared by reader, settings and web font controls.
The 24-byte little-endian header contains magic `0x46434931`, version, a 64-bit
inventory fingerprint, family count (maximum 128), and a reserved zero field.
It is followed by 152-byte family summaries: a NUL-terminated 128-byte name,
32-bit detail offset/byte count/FNV-1a hash, 16-bit file count, minimum/maximum
point sizes, four reserved zero bytes, and a 32-bit FNV-1a
checksum of the preceding summary bytes. Detail blocks follow the summaries.
Each detail is three bytes (point size, style, path length) followed by the
UTF-8 path bytes. Paths are at most 255 bytes; families contain at most 256
files. The whole cache is capped at 2 MiB.

Names/range labels load without retaining file paths. Only a requested family
hydrates its detail vector. Missing, incompatible or malformed summaries rebuild
the catalog by scanning names first and writing one family's paths at a time;
invalid detail blocks invalidate the cache for the next request.
Temporary memory failures preserve the font selection and cache for retry.
Writers finish and sync `font-catalog.tmp` before replacing the cache. An
interrupted replacement is safe because no user data is stored here.

On first font metadata access after boot or explicit invalidation, an inventory
walk checks names, directory kinds and file lengths in both font roots and their
immediate subdirectories. It reads no font contents on an index hit, and is not
repeated when catalog RAM is released and reloaded during the same session. This
detects externally added/removed files and changed lengths after restarting;
same-length content-only edits outside the firmware are not detectable by that
check. Firmware upload/delete/move/download paths invalidate explicitly,
including failed dedicated font uploads; USB Drive invalidates before handing
the card to the host. Manage Fonts performs a full rescan; alternatively remove
this cache to force reinspection after external same-length font changes.

EPUB layout cache versions and identities are unchanged by this catalog.

## `/Inbox/.airdrop/<nonce>/phase.1` through `phase.4`

### Version 1

Native AirDrop writes four exclusive immutable 512-byte publication records:
Receiving, CompleteStaged, PublishAttempted and PublishedVerified. `payload.part`
stays in this hidden session directory until one no-overwrite rename into Inbox.
These records do not change any EPUB cache format. Any regular file type,
including extensionless names, is stored as opaque bytes; size and SHA256
readback checks apply without EPUB or text-content filtering.

| Offset | Field |
| --- | --- |
| 0 | Eight-byte magic `ADSTG001` |
| 8 | Version byte `1` |
| 9 | Phase byte `1`..`4` |
| 12 | Little-endian 32-bit payload size, at most 30,000,000 |
| 16 | 32-byte SHA256; zero during Receiving |
| 64 | Safe UTF-8 filename, zero-terminated in 121 bytes |
| 185 | Final path, zero-terminated in 192 bytes |
| 400 | 32 lowercase hex bytes identifying the 128-bit nonce |
| 508 | Little-endian CRC32 of bytes 0..507 |

Unused bytes are zero. Recovery reconstructs paths and validates phase order,
identity, bounds, digest consistency and CRC. A phase3 record without a checked
phase4 receipt means publication uncertainty. Rename failure may already have
created shared-cluster aliases: neither possible path is deleted or retried.
Recovery is read-only; matching file hashes do not prove independent ownership.
A valid terminal receipt is historical evidence, not a claim that its book
still exists or is unchanged. See `lib/AirDrop/storage/README.md` for the
publication, validation, inventory limits and physical/exFAT qualification scope.
