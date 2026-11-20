#!/usr/bin/env python3
"""Lossless numeric source serializer. Never interprets C++ expressions.

Target profile is explicitly little-endian ESP32 scalar ABI, not host sizeof.
Original font sources remain authoritative and are never modified.
"""
import re
import struct

LAYOUTS = {'EpdGlyph': ('<BBHhhH2xI', [(0,255), (0,255), (0,65535),
                                    (-32768,32767), (-32768,32767), (0,65535), (0,4294967295)]),
           'EpdUnicodeInterval': ('<III', [(0,4294967295)] * 3)}


def encode_array(kind, initializer):
    if kind not in LAYOUTS:
        raise ValueError('unsupported target type')
    fmt, ranges = LAYOUTS[kind]
    initializer = re.sub(r'//[^\n]*|/\*.*?\*/', ' ', initializer, flags=re.S)
    number = r'-?(?:0x[0-9A-Fa-f]+|0|[1-9][0-9]*)'
    row = r'\{\s*' + number + (r'\s*,\s*' + number) * (len(ranges)-1) + r'\s*\}'
    if not re.fullmatch(r'\{\s*' + row + r'(?:\s*,\s*' + row + r')*\s*,?\s*\}', initializer):
        raise ValueError('unsupported numeric initializer syntax')
    result = bytearray()
    for text in re.findall(r'\{([^{}]*)\}', initializer[1:-1]):
        values = [int(x.strip(), 16 if 'x' in x.lower() else 10) for x in text.split(',')]
        if any(not lo <= x <= hi for x, (lo, hi) in zip(values, ranges)):
            raise ValueError('numeric initializer outside target field range')
        result.extend(struct.pack(fmt, *values))
    return bytes(result)


ARRAY = re.compile(r'static const (EpdGlyph|EpdUnicodeInterval) (\w+)\[\] = (\{.*?\});', re.S)


def generate_fonts(root, out, profile):
    """Transform only the measured 38 fonts; retain original include inventory."""
    import hashlib
    import json
    import zlib
    from pathlib import Path
    if profile != 'esp32-le-v1':
        raise ValueError('unsupported target ABI profile')
    root, out = Path(root), Path(out)
    out.mkdir(parents=True, exist_ok=True)
    (out / 'static_fonts').mkdir(exist_ok=True)
    (out / 'static_font_declarations').mkdir(exist_ok=True)
    source = root / 'lib/EpdFont/builtinFonts'
    aggregator = (source / 'all.h').read_text()
    names = re.findall(r'#include <builtinFonts/(.*?\.h)>', aggregator)
    selected = [n for n in names if n.startswith(('inter_', 'bitter_', 'lexenddeca_', 'ui_symbols_'))]
    if len(selected) != 38 or len(set(selected)) != 38:
        raise ValueError('font profile inventory changed; review required')
    declarations = ['#pragma once', '#include "StaticBuiltinData.h"']
    definitions, blocks, fonts, inventory = [], [], [], []
    for filename in selected:
        text = (source / filename).read_text()
        matches = list(ARRAY.finditer(text))
        if len(matches) != 2 or {m[1] for m in matches} != set(LAYOUTS):
            raise ValueError('unexpected font metadata declarations')
        transformed = text
        counts = {}
        for m in reversed(matches):
            kind, name, initializer = m.groups()
            raw = encode_array(kind, initializer)
            count = len(raw) // struct.calcsize(LAYOUTS[kind][0])
            counts[kind] = count
            packed = zlib.compress(raw, 9)
            (out / (name+'.zlib')).write_bytes(packed)
            inventory.append(dict(name=name, source='lib/EpdFont/builtinFonts/'+filename,
                                  source_sha256=hashlib.sha256(text.encode()).hexdigest(),
                                  sha256=hashlib.sha256(raw).hexdigest(), raw=len(raw),
                                  compressed=len(packed), count=count, type=kind))
            declarations += [f'extern static_builtin::Backing<{kind}, {count}> sb_{name};',
                             f'using sb_type_{name} = static_builtin::Backing<{kind}, {count}>;',
                             f'static_assert(offsetof(sb_type_{name}, values) == 0 && offsetof(sb_type_{name}, terminal) == {len(raw)} && sizeof(sb_type_{name}) == {(len(raw)+4)//4*4} && alignof(sb_type_{name}) == 4, "typed wrapper layout");']
            alias = f'static constexpr const {kind} (&{name})[{count}] = sb_{name}.values;'
            transformed = transformed[:m.start()] + alias + transformed[m.end():]
            definitions += [f'STATIC_BUILTIN_BSS static_builtin::Backing<{kind}, {count}> sb_{name};',
                            f'static const unsigned char packed_{name}[] = {{'+','.join(map(str,packed))+'};']
            blocks += [f'{{packed_{name}, sizeof(packed_{name}), reinterpret_cast<unsigned char*>(&sb_{name}), {len(raw)}, sizeof(sb_{name}), {zlib.crc32(raw)}u}}']
        # Non-metadata source text, including licenses, bitmap, group, kerning,
        # ligature and pointer-bearing descriptor initializers is copied verbatim.
        stem = filename[:-2]
        descriptor = f'static constexpr EpdFontData {stem} ='
        if transformed.count(descriptor) != 1:
            raise ValueError('unexpected pointer-bearing descriptor declaration')
        # One raw descriptor/bitmap graph, owned only by the generated registry
        # TU. Application includes declarations, never a second graph copy.
        transformed = transformed.replace(descriptor, f'extern const EpdFontData {stem} =')
        transformed = '#include "StaticBuiltinBacking.h"\n' + transformed
        (out / 'static_fonts' / filename).write_text(transformed)
        (out / 'static_font_declarations' / filename).write_text(
            '#pragma once\n#include "EpdFontData.h"\n' + f'extern const EpdFontData {stem};\n')
        fonts += [f'{{&{stem}, {counts["EpdGlyph"]}, sizeof({stem}Bitmaps)}}']
        aggregator = aggregator.replace(f'<builtinFonts/{filename}>', f'"static_font_declarations/{filename}"')
    (out / 'StaticBuiltinBacking.h').write_text('\n'.join(declarations)+'\n')
    (out / 'StaticBuiltinFonts.h').write_text(aggregator)
    registry = ['#include "StaticBuiltinBacking.h"']
    registry += [f'#include "static_fonts/{n}"' for n in selected]
    registry += definitions
    registry += ['namespace static_builtin {', 'static Block fontBlocks[] = {', ',\n'.join(blocks), '};',
                 'static const FontCheck fontChecks[] = {', ',\n'.join(fonts), '};',
                 'Tables fontTables() { return {fontBlocks, sizeof(fontBlocks)/sizeof(fontBlocks[0]), fontChecks, sizeof(fontChecks)/sizeof(fontChecks[0]), nullptr, 0}; }', '}']
    (out / 'StaticBuiltinFonts.cpp').write_text('\n'.join(registry)+'\n')
    inventory.sort(key=lambda x: x['name'])
    (out / 'font-inventory.json').write_text(json.dumps(inventory, indent=2)+'\n')
    return inventory


def generate(root, out, profile):
    """Private deterministic build output; no SDK flags or source mutations."""
    from pathlib import Path
    import gen_i18n as i18n
    root, out = Path(root).resolve(), Path(out).resolve()
    if out == root or root in out.parents and '.pio' not in out.parts:
        raise ValueError('output must be private build output, not source')
    fonts = generate_fonts(root, out, profile)
    languages, names, bcp, keys, translations, _ = i18n.load_translations(root/'lib/I18n/translations')
    used = i18n.find_used_string_keys([str(root/'src'), str(root/'lib')])
    keys = [key for key in keys if key in used]  # exact existing strip-unused path
    translations = {key: translations[key] for key in keys}
    i18n.generate_keys_header(languages, names, bcp, keys, str(out/'I18nKeys.h'))
    i18n.generate_strings_header(languages, str(out/'I18nStrings.h'))
    i18n.generate_strings_cpp(languages, names, keys, translations, str(out/'I18nStrings.cpp'))
    langs = i18n.generate_static_variant(out)
    (out/'StaticBuiltinOwner.cpp').write_text('''#include "StaticBuiltinData.h"
namespace static_builtin {
static STATIC_BUILTIN_BSS Context owner;
bool hydrateBuiltins() {
  const Tables tables[] = {fontTables(), languageTables()};
  return hydrateAll(owner, tables, 2);
}
bool builtinsReady() { return owner.phase == Phase::Ready; }
}
''')
    return fonts, langs


if __name__ == '__main__':
    import argparse
    from pathlib import Path
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--profile', required=True, choices=['esp32-le-v1'])
    args = parser.parse_args()
    generate(args.root, args.out, args.profile)
