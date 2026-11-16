#!/usr/bin/env python3
"""Exercise production publication core, then actual pinned SdFat fault seams."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--sanitize', action='store_true')
parser.add_argument('--fat', action='store_true')
parser.add_argument('--sdfat', type=Path)
parser.add_argument('--receipt', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
if args.fat and args.sdfat is None:
    candidates = [root / '.pio/libdeps/sticky/SdFat/src', root / '.pio/libdeps/sticky-owner-test/SdFat/src']
    args.sdfat = next((path for path in candidates if (path / 'FatLib/FatFile.cpp').is_file()), None)
    if args.sdfat is None:
        raise SystemExit('--fat needs --sdfat pointing to qualified SdFat 2.3.1 source')
flags = ['-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-deprecated-declarations', '-fno-exceptions', '-fno-rtti']
if args.sanitize:
    flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
with tempfile.TemporaryDirectory(prefix='airdrop-storage-') as tmp:
    obj = str(Path(tmp) / 'miniz.o')
    exe = str(Path(tmp) / 'storage-test')
    commands = [
        ['cc', '-std=c11', *[flag for flag in flags if flag != '-fno-rtti'], '-Wno-error', '-Ilib/miniz/src', '-c', 'lib/miniz/src/miniz_impl.c', '-o', obj],
    ]
    sources = ['test/airdrop/storage_test.cpp', 'lib/AirDrop/storage/StorageSink.cpp', 'lib/AirDrop/stream/StreamDecoder.cpp']
    extra = []
    if args.fat:
        assert hashlib.sha256((args.sdfat / 'FatLib/FatFile.cpp').read_bytes()).hexdigest() == 'c1757a57ea134df2aab650d2db109b7d731f66cf309e5a070a9b50667c258948'
        sources[0] = 'test/airdrop/storage_fat_test.cpp'
        sources += [str(p) for p in sorted((args.sdfat / 'FatLib').glob('*.cpp'))]
        sources += [str(p) for p in sorted((args.sdfat / 'common').glob('*.cpp'))]
        extra = ['-DENABLE_ARDUINO_FEATURES=0', '-DENABLE_ARDUINO_SERIAL=0', '-DENABLE_ARDUINO_STRING=0',
                 '-DSPI_DRIVER_SELECT=3', '-DUSE_BLOCK_DEVICE_INTERFACE=1', '-DUSE_UTF8_LONG_NAMES=1',
                 '-include', 'test/airdrop/storage_host_compat.h', '-I' + str(args.sdfat)]
    commands += [
        ['c++', '-std=c++20', *flags, '-Ilib/AirDrop/storage', '-Ilib/miniz/src', *extra, *sources, obj, '-lcrypto', '-lz', '-o', exe],
        [exe],
    ]
    inputs = set((root / 'lib/AirDrop/storage').glob('*')) | set((root / 'test/airdrop').glob('storage_*'))
    inputs |= {root / source for source in sources if not source.startswith('/')}
    inputs |= {root / 'lib/miniz/src/miniz_impl.c', root / 'lib/miniz/src/MinizConfig.h', root / 'lib/miniz/third_party/miniz.c', root / 'lib/miniz/third_party/miniz.h'}
    if args.fat:
        inputs |= set(args.sdfat.rglob('*.h')) | {Path(source) for source in sources if source.startswith('/')}
    def hashes():
        return {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(inputs) if path.is_file()}
    before = hashes()
    for command in commands:
        print(json.dumps(command), flush=True)
        subprocess.run(command, cwd=root, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'), check=True)

    after = hashes()
    assert before == after, 'Qualification inputs changed during storage test'
    if args.receipt:
        args.receipt.write_text(json.dumps({'commands': commands, 'exit': 0, 'before': before, 'after': after, 'sanitizers': args.sanitize, 'leak_sanitizer': 'disabled under sandbox tracing', 'actual_fat': args.fat, 'sdfat': str(args.sdfat), 'hardware_proof': False}, indent=2) + '\n')

if not args.fat:
    subprocess.run(['python3',str(root/'test/airdrop/recovery_ownership_run.py'),*(['--sanitize'] if args.sanitize else [])],cwd=root,check=True)
