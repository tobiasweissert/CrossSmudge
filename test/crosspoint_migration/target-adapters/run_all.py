#!/usr/bin/env python3
"""Routine serial entrypoint for target-adapter contract and behavior checks."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
GTEST = Path('/root/.hermes/cache/scratch/library-finish-deps/googletest-1.17.0/googletest')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=True)
    sources = [HERE/'cases.cpp', ROOT/'src/MappedInputManager.cpp', ROOT/'src/util/ButtonNavigator.cpp', ROOT/'lib/hal/HalDisplay.cpp', GTEST/'src/gtest-all.cc', GTEST/'src/gtest_main.cc']
    base = [sys.executable, str(HERE/'probe.py')]
    flags = ['--sanitize'] if args.sanitize else []
    phases = [
        ('contract', [*base, 'contract', str(HERE/'contract.cpp'), '--syntax', '--evidence', str(args.evidence)]),
        ('cover-grid-source', [*base, 'cover-grid-source', str(HERE.parent/'target-ui-settings/cover_grid.cpp'), '--syntax', '--evidence', str(args.evidence)]),
        ('navigation-display', [*base, 'navigation-display', *map(str, sources), '--evidence', str(args.evidence), *flags]),
        ('creation-time', [sys.executable, str(HERE/'creation_time.py'), '--evidence', str(args.evidence/'creation-time'), *flags]),
    ]
    results = []
    for phase, command in phases:
        result = subprocess.run(command, capture_output=True, text=True, cwd=ROOT, timeout=180)
        (args.evidence/(phase+'-driver.log')).write_text(result.stdout+result.stderr)
        results.append({'phase': phase, 'exit': result.returncode, 'command': command})
        (args.evidence/'summary.json').write_text(json.dumps(results, indent=2)+'\n')
        print(phase, result.returncode, flush=True)
        if result.returncode:
            return result.returncode
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
