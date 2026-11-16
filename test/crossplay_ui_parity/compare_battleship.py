"""Compare real, same-state native/port PNG regions; never approve by bounds alone.

Both inputs must be unfiltered 8-bit grayscale exports from the native fixture.
Reference and candidate generators remain separate from this pixel comparator.
"""
import argparse
import json
from pathlib import Path
import struct
import zlib


def read_png(path):
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    cursor, payload = 8, b''
    while cursor < len(data):
        length = struct.unpack('>I', data[cursor:cursor + 4])[0]
        kind = data[cursor + 4:cursor + 8]
        body = data[cursor + 8:cursor + 8 + length]
        if kind == b'IHDR':
            w, h, depth, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', body)
            assert (depth, color, compression, filtering, interlace) == (8, 0, 0, 0, 0)
        elif kind == b'IDAT':
            payload += body
        cursor += length + 12
    raw = zlib.decompress(payload)
    assert len(raw) == (w + 1) * h
    assert all(raw[y * (w + 1)] == 0 for y in range(h)), 'filtered PNG requires explicit decoder'
    return w, h, b''.join(raw[y * (w + 1) + 1:(y + 1) * (w + 1)] for y in range(h))


def compare(reference, candidate):
    w, h, before = read_png(reference)
    width, height, after = read_png(candidate)
    assert (w, h) == (width, height) == (480, 800)
    regions = {
        'header': (0, 0, 480, 83), 'report': (16, 119, 464, 145),
        'target': (46, 151, 434, 539), 'own_fleet': (19, 550, 181, 712),
        'roster': (204, 551, 464, 711), 'footer': (16, 732, 464, 784),
        'whole_frame': (0, 0, w, h),
    }
    return {name: sum(before[y*w+x] != after[y*w+x] for y in range(t, b) for x in range(l, r))
            for name, (l, t, r, b) in regions.items()}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('reference', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = compare(args.reference, args.candidate)
    report = json.dumps(result, indent=2) + '\n'
    print(report, end='')
    if args.output:
        args.output.write_text(report)
    assert not any(result.values()), 'Original native and port pixels differ'
