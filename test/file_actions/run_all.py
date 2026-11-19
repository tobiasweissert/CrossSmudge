#!/usr/bin/env python3
"""Offline Files UI, nested metadata, and checked-state production regressions."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

HERE = Path(__file__).resolve().parent

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    for mode, flags in [('normal', []), ('sanitized', ['--sanitize'])]:
        checks = []
        commands = []
        groups = [('files', 'run.py', []), ('nested', 'nested_run.py', []),
                  ('state', 'state_run.py', []), ('state-ui', 'state_run.py', ['--ui']),
                  ('recents', 'recents_run.py', [])]
        for group, script, extra in groups:
            command = [sys.executable, str(HERE/script), *extra, *flags]
            env = dict(os.environ, NESTED_LOG=str((args.output/(mode+'-nested-probe.log')).resolve()))
            completed = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       text=True, timeout=240, env=env)
            log = args.output/(mode+'-'+group+'.log')
            log.write_text('COMMAND: '+repr(command)+'\n'+completed.stdout+'\nEXIT='+str(completed.returncode)+'\n')
            print(completed.stdout, end='', flush=True)
            found = [line[5:] for line in completed.stdout.splitlines() if line.startswith('PASS ')]
            # Retain the original 23 Files identities verbatim; namespace the new groups.
            checks.extend(found if group == 'files' else [group+': '+name for name in found])
            commands.append({'group': group, 'command': command, 'exit': completed.returncode,
                             'checks': found, 'log': str(log)})
        code = int(any(item['exit'] != 0 or not item['checks'] for item in commands))
        results.append({'mode': mode, 'exit': code, 'checks': checks, 'commands': commands})
    passed = (all(item['exit'] == 0 for item in results) and bool(results[0]['checks'])
              and results[0]['checks'] == results[1]['checks']
              and len(set(results[0]['checks'])) == len(results[0]['checks']))
    (args.output/'results.json').write_text(json.dumps({'passed': passed, 'runs': results}, indent=2)+'\n')
    print('Files normal + ASan/UBSan:', 'PASS' if passed else 'FAIL')
    return 0 if passed else 1

if __name__ == '__main__':
    sys.exit(main())
