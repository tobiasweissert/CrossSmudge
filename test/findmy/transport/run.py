#!/usr/bin/env python3
"""Compile actual adapter with synthetic native C API; no controller/radio."""
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
OUT = Path('/root/.hermes/task-state/crosscub-findmy-implementation/transport')
OUT.mkdir(parents=True, exist_ok=True)
CASES = ['sync', 'frame', 'expiry', 'init-failure', 'sync-timeout',
         'reset-active', 'reset-queued', 'failure-address', 'failure-data',
         'failure-scan', 'failure-start', 'failure-stop', 'late-completion']
CASES += ['invalid-' + s for s in ('address-size', 'payload-size', 'null-address',
          'null-payload', 'untrusted', 'zero-ttl', 'long-ttl', 'prefix',
          'random-type', 'zero-address', 'ones-address', 'key-bits')]
mode = sys.argv[1] if len(sys.argv) > 1 else 'all'
if mode == 'all':
    subprocess.run([sys.executable, str(__file__), 'off'], check=True)
    subprocess.run([sys.executable, str(__file__), 'native'], check=True)
    subprocess.run([sys.executable, str(HERE / 'archive_link.py')], check=True)
    print('PASS complete transport suite: default-off, ' + str(len(CASES)) + ' native cases, archive retention')
    sys.exit(0)
cmd = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
       '-I' + str(HERE / 'stubs'), '-I' + str(ROOT / 'lib/HalFindMyBle'),
       str(HERE / 'transport_test.cpp'), str(ROOT / 'lib/HalFindMyBle/HalFindMyBle.cpp'),
       str(ROOT / 'lib/HalFindMyBle/EarlyBtRetention.cpp'), '-o', str(OUT / ('test-' + mode))]
if mode != 'off':
    cmd += ['-DCROSSCUB_FINDMY_EXPERIMENTAL=1', '-DARDUINO_ARCH_ESP32=1', '-DCONFIG_IDF_TARGET_ESP32S3=1']
if os.environ.get('SANITIZE') == '1':
    cmd += ['-fsanitize=undefined', '-fno-sanitize-recover=all']
print('COMPILE', ' '.join(cmd), flush=True)
subprocess.run(cmd, check=True)
for case in (sys.argv[2:] or (['inert'] if mode == 'off' else CASES)):
    print('CASE', case, flush=True)
    subprocess.run([str(OUT / ('test-' + mode)), case], check=True)
