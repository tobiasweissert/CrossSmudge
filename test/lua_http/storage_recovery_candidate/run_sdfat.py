"""Compile unmodified pinned FAT/common sources against a host sparse RAM disk."""
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent
SDK = Path('/root/github/crosssmudge-lua-http/.pio/libdeps/sticky/SdFat/src')
assert hashlib.sha256((SDK / 'FatLib/FatFile.cpp').read_bytes()).hexdigest() == 'c1757a57ea134df2aab650d2db109b7d731f66cf309e5a070a9b50667c258948'
sources = sorted((SDK / 'FatLib').glob('*.cpp')) + sorted((SDK / 'common').glob('*.cpp'))
inputs = sources + list(SDK.rglob('*.h')) + [ROOT / n for n in ('SdFatFixture.h', 'HostSdFatCompat.h', 'sdfat_test.cpp', 'run_sdfat.py', 'SealStage.h', 'BodyStage.h', 'PreSealCheck.h', 'WriteAdmission.h', 'RecoverableDownloadStore.h')]
def hashes():
    return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
before = hashes()
results = []
for mode, flags in [('normal', []), ('sanitize', ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'])]:
    exe = Path(os.environ['TMPDIR']) / ('crosssmudge-sdfat-' + mode)
    cmd = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-fno-exceptions', '-fno-rtti', *flags,
           '-DENABLE_ARDUINO_FEATURES=0', '-DENABLE_ARDUINO_SERIAL=0', '-DENABLE_ARDUINO_STRING=0',
           '-DSPI_DRIVER_SELECT=3', '-DUSE_BLOCK_DEVICE_INTERFACE=1', '-include', str(ROOT / 'HostSdFatCompat.h'),
           '-I' + str(SDK), str(ROOT / 'sdfat_test.cpp'), *map(str, sources), '-o', str(exe)]
    c = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    (ROOT / ('sdfat-' + mode + '-compile.log')).write_text(c.stdout + c.stderr)
    result = dict(mode=mode, compile_exit=c.returncode, command=cmd)
    if not c.returncode:
        r = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
        (ROOT / ('sdfat-' + mode + '-run.log')).write_text(r.stdout + r.stderr)
        result.update(run_exit=r.returncode, groups=[s for s in r.stdout.splitlines() if s.startswith('PASS ')])
    results.append(result)
receipt = dict(kind='actual pinned FAT/common library; synthetic sparse RAM block device, not hardware SD',
               compiler=subprocess.check_output(['g++', '--version'], text=True), results=results,
               before=before, after=hashes(), hardware_enabled=False)
(ROOT / 'sdfat-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(json.dumps(results, indent=2))
assert receipt['before'] == receipt['after']
assert all(r['compile_exit'] == 0 and r.get('run_exit') == 0 and len(r.get('groups', [])) == 3 for r in results)
