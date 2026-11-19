#!/usr/bin/env python3
"""Bounded host regression: real persistence TU and extracted settings loader.
No toolchain installs; uses an existing ArduinoJson checkout, SD fault double.
Run python3 test_migration.py --evidence DIR [--sanitize].
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def method(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        if source[end] == '{': depth += 1
        elif source[end] == '}': depth -= 1
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=True)
    deps = Path(os.environ.get('ARDUINOJSON_INCLUDE', '/root/github/.host-data/crosssmudge/gallery-native-deps/sticky-simulator/ArduinoJson/src'))
    assert (deps / 'ArduinoJson.h').is_file(), 'Existing ArduinoJson required (no download)'
    with tempfile.TemporaryDirectory(prefix='settings-migration-', dir=os.environ.get('TMPDIR')) as tmp:
        work = Path(tmp)
        fixture = Path(__file__).parent
        for p in (fixture / 'stubs').iterdir():
            (work / p.name).write_bytes(p.read_bytes())
        source = (ROOT / 'src/CrossPointSettings.cpp').read_text()
        loader = method(source, 'bool CrossPointSettings::loadFromFile()')
        code = (fixture / 'migration.cpp').read_text().replace('// PRODUCTION_LOADER', loader)
        (work / 'migration.cpp').write_text(code)
        command = ['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-fno-exceptions', '-fno-rtti', '-DARDUINOJSON_ENABLE_ARDUINO_STRING=0', '-I' + str(work), '-I' + str(deps), '-I' + str(ROOT / 'lib/Serialization'), str(work / 'migration.cpp'), str(ROOT / 'lib/Serialization/PersistableStore.cpp'), '-o', str(work / 'probe')]
        if args.sanitize: command[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        compile_run = subprocess.run(command, capture_output=True, text=True, timeout=60)
        (args.evidence / 'compile.log').write_text(compile_run.stdout + compile_run.stderr)
        receipt = {'command': command, 'compile_exit': compile_run.returncode, 'source_sha256': {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in ['src/CrossPointSettings.cpp', 'lib/Serialization/PersistableStore.cpp', 'lib/Serialization/PersistableStore.h']}, 'coverage': 'production persistence TU; extracted production loadFromFile with SD/codec dependencies and settings parser double; not firmware build'}
        if not compile_run.returncode:
            run = subprocess.run([str(work / 'probe')], capture_output=True, text=True, timeout=30)
            receipt['run_exit'] = run.returncode
            (args.evidence / 'run.log').write_text(run.stdout + run.stderr)
        (args.evidence / 'receipt.json').write_text(json.dumps(receipt, indent=2))
        print(json.dumps(receipt, indent=2))
        return receipt.get('run_exit', compile_run.returncode)


if __name__ == '__main__':
    raise SystemExit(main())
