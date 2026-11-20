#!/usr/bin/env python3
"""Standalone production-core regression entrypoint: g++, libcrypto, stdlib only."""
import json
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
OUT = pathlib.Path(os.environ.get('FINDMY_TEST_OUT', '/root/.hermes/task-state/crosscub-findmy-implementation/core'))
OUT.mkdir(parents=True, exist_ok=True)
label = sys.argv[1] if len(sys.argv) > 1 else 'final'
if pathlib.Path(label).name != label:
    raise SystemExit('invalid log label')
production = sorted((ROOT / 'lib/FindMy').glob('*.cpp'))
text = ''
code = 0
for name, flags in [('core_test', ['-DFINDMY_CRYPTO_OPENSSL']), ('no_crypto_test', [])]:
    cmd = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'lib/FindMy')]
    cmd += flags + [str(ROOT / 'test/findmy/core' / (name + '.cpp'))] + list(map(str, production))
    if flags:
        cmd += ['-lcrypto']
    cmd += ['-o', str(OUT / name)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    text += 'COMMAND: ' + ' '.join(cmd) + '\n' + r.stdout + r.stderr
    if r.returncode == 0:
        r = subprocess.run([str(OUT / name)], capture_output=True, text=True)
        text += r.stdout + r.stderr
        if name == 'core_test' and r.returncode == 0:
            actual = {int(parts[1]): parts[2] for line in r.stdout.splitlines() if (parts := line.split()) and parts[0] == 'VECTOR'}
            fixture = json.loads((ROOT / 'test/findmy/core/vectors.json').read_text())
            expected = {v['index']: v['public_x'] for v in fixture['vectors']}
            if actual != expected:
                text += 'FAIL: production public bytes differ from independent fixture\n'
                code = 1
            else:
                text += f'PASS: {len(expected)} independent public-key vectors match exact production bytes\n'
    text += f'{name} EXIT: {r.returncode}\n'
    code = code or r.returncode
text += f'OVERALL EXIT: {code}\n'
(OUT / (label + '.log')).write_text(text)
print(text, end='')
sys.exit(code)
