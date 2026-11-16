#!/usr/bin/env python3
"""Compile actual production stream core; host zlib is fixture generation only."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--sanitize', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
flags = ['-O1', '-g']
if args.sanitize:
    flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
with tempfile.TemporaryDirectory(prefix='airdrop-stream-') as tmp:
    obj = str(Path(tmp) / 'miniz.o')
    exe = str(Path(tmp) / 'stream-test')
    native = []
    if sys.platform == 'darwin':
        source = Path(tmp) / 'native-source'
        source.mkdir()
        (source / 'book.txt').write_bytes(b'abc')
        archive = Path(tmp) / 'native.cpio'
        subprocess.run(['/usr/bin/ditto','-c','--norsrc','.',str(archive)],cwd=source,check=True)
        native = [str(archive)]
    commands = [
        ['cc', '-std=c11', *flags, '-Ilib/miniz/src', '-c', 'lib/miniz/src/miniz_impl.c', '-o', obj],
        ['c++', '-std=c++20', '-Wall', '-Wextra', '-Werror', *flags,
         '-Ilib/AirDrop/stream', '-Ilib/miniz/src', 'test/airdrop/stream_test.cpp',
         'lib/AirDrop/stream/StreamDecoder.cpp', obj, '-lz', '-o', exe],
        [exe,*native],
    ]
    # LSan cannot inspect tasks under this sandbox's tracing. ASan and UBSan
    # remain enabled; the codec owns no dynamic allocations.
    for command in commands:
        print(json.dumps(command), flush=True)
        subprocess.run(command, cwd=root, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0'), check=True)
