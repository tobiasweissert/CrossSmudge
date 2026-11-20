#!/usr/bin/env python3
"""Offline Find My candidate suites. No installation, account requests or radio."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]

def snapshot():
    result = {}
    for name in ['lib/FindMy', 'lib/HalFindMyBle', 'lib/FindMySound', 'lib/FindMyRuntime',
                 'src/findmy', 'tools/findmy', 'tools/findmy_registration', 'test/findmy']:
        for path in sorted((ROOT / name).rglob('*')):
            if path.is_file() and '__pycache__' not in path.parts:
                result[str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sdk', action='store_true', help='also require existing cached target compiler/headers')
    args = parser.parse_args()
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        parser.error('output must be outside the checkout')
    output.mkdir(parents=True, exist_ok=True)
    commands = [
        ('core', [sys.executable, '-B', str(ROOT / 'test/findmy/core/run.py')], {}),
        ('transport', [sys.executable, '-B', str(ROOT / 'test/findmy/transport/run.py')], {}),
        ('transport-ubsan', [sys.executable, '-B', str(ROOT / 'test/findmy/transport/run.py')], {'SANITIZE': '1'}),
        ('sound', [sys.executable, '-B', str(ROOT / 'test/findmy/sound/run.py')], {}),
        ('runtime', [sys.executable, '-B', str(ROOT / 'test/findmy/runtime/run.py')], {}),
        ('profile', [sys.executable, '-B', str(ROOT / 'test/findmy/runtime/profile_run.py')], {}),
    ]
    for name in ['host', 'registration']:
        directory = ROOT / 'test/findmy' / name
        if not list(directory.glob('test*.py')):
            parser.error('required nonempty offline suite missing: ' + name)
        commands.append((name, [sys.executable, '-B', '-m', 'unittest', 'discover', '-s', str(directory), '-v'], {}))
    if args.sdk:
        for name in ['core', 'transport', 'runtime']:
            commands.append((name + '-sdk-syntax', [sys.executable, '-B', str(ROOT / 'test/findmy' / name / 'sdk_syntax.py')], {}))
    before = snapshot()
    results = []
    for name, command, additions in commands:
        log = output / (name + '.log')
        start = time.monotonic()
        env = {**os.environ, 'PYTHONDONTWRITEBYTECODE': '1', **additions}
        # Never inherit a sanitizer selector into another suite accidentally.
        if 'SANITIZE' not in additions:
            env.pop('SANITIZE', None)
        with log.open('w') as stream:
            try:
                result = subprocess.run(command, cwd=ROOT, env=env, stdout=stream,
                                        stderr=subprocess.STDOUT, timeout=180)
                code = result.returncode
            except subprocess.TimeoutExpired:
                code = 124
        results.append({'name': name, 'exit': code, 'log': str(log),
                        'seconds': round(time.monotonic() - start, 3)})
        (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
        print(name, code, flush=True)
    after = snapshot()
    stable = before == after
    (output / 'source-stability.json').write_text(json.dumps({'stable': stable, 'files': after}, indent=2) + '\n')
    print('source-stable', stable, flush=True)
    return int(not stable or any(row['exit'] for row in results))

if __name__ == '__main__':
    sys.exit(main())
