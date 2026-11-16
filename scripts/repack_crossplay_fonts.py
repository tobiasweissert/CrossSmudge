"""Losslessly repack original CrossPlay Epd ASCII masks for Lua bitmap draws."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

parser = argparse.ArgumentParser()
parser.add_argument('reference', type=Path)
parser.add_argument('app', type=Path)
args = parser.parse_args()
target = args.app / 'art'
target.mkdir(exist_ok=True)
provenance = {}
for size in (10, 20, 30):
    source = args.reference / f'src/apps_local/ui/fonts/toybox_{size}.h'
    text = source.read_text()
    bits = bytes(int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', text.split('Bitmaps')[1].split('};')[0]))
    glyphs = [list(map(int, x.split(','))) for x in re.findall(r'\{([\d, -]+)\}', text.split('Glyphs[]')[1].split('};')[0])][:59]
    header, pixels = bytearray(), bytearray()
    for width, height, advance, left, top, count, offset in glyphs:
        start = len(glyphs)*7 + len(pixels)
        header.extend([width, height, (advance+8)//16, left % 256, top, start % 256, start//256])
        stride = (width+7)//8
        raw = bytearray([255]*(stride*height))
        for y in range(height):
            for x in range(width):
                bit = y*width+x
                if bits[offset+bit//8] & (128 >> (bit % 8)):
                    raw[y*stride+x//8] &= ~(128 >> (x % 8))
        pixels.extend(raw)
    (target / f'font{size}.bin').write_bytes(header+pixels)
    provenance[str(source.relative_to(args.reference))] = hashlib.sha256(source.read_bytes()).hexdigest()
shutil.copyfile(args.reference / 'site/assets/fonts/OFL-Jersey25.txt', args.app / 'OFL-Jersey25.txt')
(target / 'sources.json').write_text(json.dumps(provenance, indent=2)+'\n')
