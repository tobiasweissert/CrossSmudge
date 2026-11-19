#!/usr/bin/env python3
"""Focused real-HAL/real-SDK-header host regression; no firmware build/network."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SDK = ROOT / 'freeink-sdk'
REV = '634a3f8c067a3714a6a7c622015138af096ab5d2'
CASES = ['arguments_success', 'restore_success', 'result_false', 'restore_false',
         'zero_geometry_delegated', 'uint16_boundary_delegated', 'default_restore_false',
         'recursive_lock_lifetime', 'exception_lock_lifetime']
SDK_INPUTS = ['libs/display/FreeInkDisplay/include/EInkDisplay.h',
              'libs/display/FreeInkDisplay/include/FreeInkDisplay.h',
              'libs/display/FreeInkDisplay/include/GrayscaleCapabilities.h',
              'libs/display/FreeInkDisplay/src/bus/EpdBus.h',
              'libs/display/FreeInkDisplay/src/FreeInkDisplay.cpp']

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=['normal', 'sanitized'], default='normal')
    parser.add_argument('--expect-red', action='store_true')
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    evidence = args.evidence.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    label = 'red' if args.expect_red else args.mode
    assert shutil.disk_usage(os.environ['TMPDIR']).free >= 650 * 1024 * 1024, 'Keep >=650 MiB scratch free'
    assert subprocess.check_output(['git', '-C', str(SDK), 'rev-parse', 'HEAD'], text=True).strip() == REV
    for name in SDK_INPUTS:
        pinned = subprocess.check_output(['git', '-C', str(SDK), 'show', REV + ':' + name])
        assert (SDK / name).read_bytes() == pinned, 'Changed SDK input: ' + name
    inputs = [ROOT / 'lib/hal/HalDisplay.h', ROOT / 'lib/hal/HalDisplay.cpp',
              ROOT / 'lib/hal/HalSpiBus.h', ROOT / 'lib/hal/HalSpiBus.cpp']
    inputs += [SDK / name for name in SDK_INPUTS]
    inputs += sorted(p for p in HERE.rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    manifest = {str(p.relative_to(ROOT)): digest(p) for p in inputs}
    (evidence / (label + '-inputs.json')).write_text(json.dumps(manifest, indent=2) + '\n')
    commands = []
    def run(command, name):
        result = subprocess.run([str(c) for c in command], capture_output=True, text=True, timeout=60,
                                env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                                         UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
        output = result.stdout + result.stderr
        (evidence / (label + '-' + name + '.log')).write_text(output)
        commands.append({'name': name, 'command': [str(c) for c in command], 'exit': result.returncode})
        print(label, name, result.returncode, output, flush=True)
        return result
    base = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-ffunction-sections', '-fdata-sections',
            '-I' + str(HERE / 'stubs'), '-I' + str(ROOT / 'lib/hal'),
            '-I' + str(SDK / 'libs/display/FreeInkDisplay/include')]
    if args.mode == 'sanitized':
        base += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g', '-fno-pie', '-no-pie']
    try:
        with tempfile.TemporaryDirectory(prefix='hal-retained-overlay-', dir=os.environ['TMPDIR']) as temp:
            build = Path(temp)
            assert run(base + ['-fsyntax-only', HERE / 'sdk-contract.cpp'], 'sdk-contract').returncode == 0
            contract = run(base + ['-fsyntax-only', HERE / 'contract.cpp'], 'hal-contract')
            if args.expect_red:
                assert contract.returncode != 0
                assert 'displayRetainedGrayscaleOverlay' in contract.stderr
                assert 'not a member of' in contract.stderr or 'has no member named' in contract.stderr
                print('EXPECTED RED: SDK declaration compiles, local HAL declaration is missing the method')
                return 0
            assert contract.returncode == 0
            binary = build / 'cases'
            result = run(base + [ROOT / 'lib/hal/HalDisplay.cpp', ROOT / 'lib/hal/HalSpiBus.cpp',
                                 HERE / 'cases.cpp', '-Wl,--gc-sections', '-o', binary], 'compile')
            assert result.returncode == 0
            (evidence / (label + '-binary.sha256')).write_text(digest(binary) + '  cases\n')
            suite = ET.Element('testsuite', name='hal-retained-overlay-' + args.mode, tests=str(len(CASES)))
            failures = 0
            for case in CASES:
                result = run([binary, case], case)
                node = ET.SubElement(suite, 'testcase', name=case, classname='HalRetainedOverlay')
                if result.returncode:
                    failures += 1
                    ET.SubElement(node, 'failure', message='exit ' + str(result.returncode)).text = result.stdout + result.stderr
            suite.set('failures', str(failures))
            ET.ElementTree(suite).write(evidence / (label + '-junit.xml'), encoding='unicode')
            assert failures == 0
            assert all(digest(ROOT / p) == value for p, value in manifest.items()), 'Inputs changed during run'
            print(f'PASS: {len(CASES)} cases; real HAL + SPI lock sources; pinned SDK declarations; hardware/link doubles')
    finally:
        (evidence / (label + '-commands.json')).write_text(json.dumps(commands, indent=2) + '\n')

if __name__ == '__main__':
    raise SystemExit(main())
