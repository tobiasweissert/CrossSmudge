"""Routine isolated candidate suites. No firmware build or hardware I/O."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
scratch = Path(os.environ.get('TMPDIR', '/root/.hermes/cache/scratch'))
results = []
for suite in ('recovery', 'admission', 'body', 'preseal', 'stage_receipt', 'seal', 'lua_outcome'):
    for mode, flags in (('normal', []), ('sanitize', ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'])):
        exe = scratch / ('crosssmudge-' + suite + '-' + mode)
        cmd = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fno-exceptions', '-fno-rtti',
               *flags, str(ROOT / (suite + '_test.cpp')), '-o', str(exe)]
        c = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        prefix = 'all-' + suite + '-' + mode
        (ROOT / (prefix + '-compile.log')).write_text(c.stdout + c.stderr)
        result = dict(suite=suite, mode=mode, compile_exit=c.returncode)
        if c.returncode == 0:
            r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
            (ROOT / (prefix + '-run.log')).write_text(r.stdout + r.stderr)
            groups = [line for line in r.stdout.splitlines() if line.startswith('PASS ')]
            result.update(run_exit=r.returncode, groups=groups)
        results.append(result)
sdfat = subprocess.run([sys.executable, str(ROOT / 'run_sdfat.py')], capture_output=True, text=True, timeout=180)
(ROOT / 'all-sdfat-run.log').write_text(sdfat.stdout + sdfat.stderr)
assert sdfat.returncode == 0, 'Pinned SdFat block-device fixture failed; see all-sdfat-run.log'
staging = subprocess.run([sys.executable, str(ROOT / 'run_sdfat_staging.py')], capture_output=True, text=True, timeout=180)
(ROOT / 'all-sdfat-staging-run.log').write_text(staging.stdout + staging.stderr)
assert staging.returncode == 0, 'Pinned FAT staging fault fixture failed; see all-sdfat-staging-run.log'
io = subprocess.run([sys.executable, str(ROOT / 'run_sdfat_io.py')], capture_output=True, text=True, timeout=180)
(ROOT / 'all-sdfat-io-run.log').write_text(io.stdout + io.stderr)
assert io.returncode == 0, 'Pinned FAT read/sync fault fixture failed; see all-sdfat-io-run.log'
receipt = dict(io_receipt_sha256=hashlib.sha256((ROOT / 'io-receipt.json').read_bytes()).hexdigest(), staging_receipt_sha256=hashlib.sha256((ROOT / 'staging-receipt.json').read_bytes()).hexdigest(),
               sdfat_receipt_sha256=hashlib.sha256((ROOT / 'sdfat-receipt.json').read_bytes()).hexdigest(),
               kind='portable recovery and sealing plus real pinned FAT library with synthetic block device',
               sha256={n: hashlib.sha256((ROOT / n).read_bytes()).hexdigest()
                       for n in ('RecoverableDownloadStore.h', 'WriteAdmission.h', 'BodyStage.h', 'PreSealCheck.h', 'SealStage.h', 'recovery_test.cpp',
                                 'admission_test.cpp', 'body_test.cpp', 'preseal_test.cpp', 'stage_receipt_test.cpp', 'seal_test.cpp',
                                 'LuaRecoveryContract.h', 'lua_outcome_test.cpp', 'LUA-RECOVERY-API5.md', 'run_all.py')},
               results=results, hardware_enabled=False,
               pending=['exFAT; physical power loss', 'independent review',
                        'API5 production bindings, uncertainty/startup admission and client adoption', 'target and physical acceptance'])
(ROOT / 'all-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps(receipt, indent=2))
assert all(r['compile_exit'] == 0 and r.get('run_exit') == 0 and
           len(r.get('groups', [])) == {'recovery': 9, 'admission': 6, 'body': 7, 'preseal': 5, 'stage_receipt': 3, 'seal': 5, 'lua_outcome': 4}[r['suite']] for r in results)
