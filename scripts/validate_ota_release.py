#!/usr/bin/env python3
"""Fail-closed Crosscub Sticky release qualification (stdlib only)."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys

REPOSITORY = 'tobiasweissert/Crosscub'


def validate_version(version):
    """Match the firmware's 32-byte NUL-terminated numeric descriptor field."""
    if not isinstance(version, str) or len(version.encode('utf-8')) > 31 or not re.fullmatch(
            r'[0-9]{1,9}(?:\.[0-9]{1,9}){0,3}', version):
        raise ValueError('version must be 1-4 decimal components, <=9 digits each and <=31 bytes')
    return version


def validate_release(path, version, repository=REPOSITORY, expected_sha256=None):
    validate_version(version)
    path = Path(path)
    if repository != REPOSITORY:
        raise ValueError('release repository must be ' + REPOSITORY)
    if path.name != f'crosscub-sticky-v{version}.bin':
        raise ValueError('noncanonical firmware filename')
    if path.is_symlink() or not path.is_file() or not 65536 <= path.stat().st_size <= 0x640000:
        raise ValueError('image must be a regular file fitting both Sticky OTA slots (65536..6553600 bytes)')
    data = path.read_bytes()
    if data[0] != 0xe9 or not 1 <= data[1] <= 16 or data[23] != 1:
        raise ValueError('invalid ESP header or missing appended image SHA256')
    if struct.unpack_from('<H', data, 12)[0] != 9:
        raise ValueError('image chip must be ESP32-S3')
    pos, checksum, segments = 24, 0xef, []
    for _ in range(data[1]):
        if pos + 8 > len(data):
            raise ValueError('truncated segment header')
        size = struct.unpack_from('<I', data, pos + 4)[0]
        pos += 8
        if size == 0 or pos + size > len(data):
            raise ValueError('invalid segment length')
        segment = data[pos:pos+size]
        segments.append(segment)
        for byte in segment:
            checksum ^= byte
        pos += size
    first = segments[0]
    if len(first) < 256 or struct.unpack_from('<I', first)[0] != 0xabcd5432:
        raise ValueError('missing ESP application descriptor in first segment')
    def field(offset):
        raw = first[offset:offset+32]
        if b'\0' not in raw:
            raise ValueError('unterminated descriptor field')
        return raw.split(b'\0', 1)[0]
    if field(48) != b'Crosscub' or field(16) != version.encode('ascii'):
        raise ValueError('descriptor product/version mismatch')
    tags = []
    for segment in segments:
        tags.extend(re.findall(rb'CROSSPOINT-BOARD-V1:([^;\x00]{0,64});', segment))
        if segment.count(b'CROSSPOINT-BOARD-V1:') != len(re.findall(rb'CROSSPOINT-BOARD-V1:([^;\x00]{0,64});', segment)):
            raise ValueError('malformed board marker')
    if not tags or any(tag != b'sticky' for tag in tags):
        raise ValueError('missing or conflicting Sticky board tag')
    end = (pos + 16) & ~15
    if end + 32 != len(data):
        raise ValueError('image size does not exactly match segments, padding and hash')
    if data[end-1] != checksum:
        raise ValueError('ESP segment XOR checksum mismatch')
    if hashlib.sha256(data[:end]).digest() != data[end:]:
        raise ValueError('ESP appended image SHA256 mismatch')
    digest = hashlib.sha256(data).hexdigest()
    if expected_sha256 is not None and digest != expected_sha256:
        raise ValueError('artifact SHA256 mismatch')
    return {'repository': REPOSITORY, 'version': version, 'tag': f'crosscub-v{version}',
            'filename': path.name, 'project': 'Crosscub', 'board': 'sticky',
            'size': len(data), 'sha256': digest}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', required=True)
    parser.add_argument('--firmware', required=True, type=Path)
    parser.add_argument('--repo', default=REPOSITORY)
    parser.add_argument('--expected-sha256')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    try:
        report = validate_release(args.firmware, args.version, args.repo, args.expected_sha256)
    except (ValueError, OSError) as exc:
        parser.exit(1, f'release validation failed: {exc}\n')
    output = json.dumps(report, indent=2) + '\n'
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(output, encoding='utf-8')
    print(output, end='')


if __name__ == '__main__':
    main()
