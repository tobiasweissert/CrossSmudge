"""Read-side recovery candidate regression runner; synthetic files, not SdFat."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
SCRATCH = Path(os.environ.get('TMPDIR', '/root/.hermes/cache/scratch'))
manifest = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
            for name in ('RecoverableDownloadStore.h', 'recovery_test.cpp', 'run.py')}
results = []
for mode, flags in (('normal', []), ('sanitize', ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'])):
    exe = SCRATCH / ('crosssmudge-recovery-' + mode)
    cmd = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fno-exceptions', '-fno-rtti',
           *flags, str(ROOT / 'recovery_test.cpp'), '-o', str(exe)]
    compile_result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    (ROOT / (mode + '-compile.log')).write_text(compile_result.stdout + compile_result.stderr)
    result = {'mode': mode, 'compile_command': cmd, 'compile_exit': compile_result.returncode}
    if compile_result.returncode == 0:
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
        (ROOT / (mode + '-run.log')).write_text(run.stdout + run.stderr)
        result.update(run_exit=run.returncode,
                      passed_groups=[line for line in run.stdout.splitlines() if line.startswith('PASS ')])
    results.append(result)
receipt = {'kind': 'portable read-side validator/recovery candidate with synthetic in-memory file adapter',
           'source_sha256': manifest, 'results': results,
           'not_qualified': ['write/seal/abort protocol', 'three-state commit contract', 'actual SdFat/block device',
                             'firmware integration', 'physical SD/power loss'],
           'hardware_download_activation': False}
(ROOT / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps(receipt, indent=2))
assert all(r['compile_exit'] == 0 and r.get('run_exit') == 0 for r in results)
expected_groups = {
    'PASS valid sealed record',
    'PASS newest valid generation recovered',
    'PASS every-byte corruption preserves prior generation',
    'PASS truncated and trailing records rejected',
    'PASS absence distinguished from size and read uncertainty',
    'PASS equal-generation conflicts include actual checksum collision',
    'PASS identical equal generations and comparison I/O failure',
    'PASS 2 MiB limit, bounded reads, numeric-slot admission, max generation read',
    'PASS empty storage',
}
assert all(set(r['passed_groups']) == expected_groups and len(r['passed_groups']) == len(expected_groups)
           for r in results)
