#!/usr/bin/env python3
"""Compile Sticky admission against all normalized flags in a pinned SDK blob.

Read-only Git object access avoids initializing the SDK. This checks application
admission, not the hardware SDK's compilation or any physical board selection.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--sdk-git-dir', required=True)
p.add_argument('--sdk-ref', default='634a3f8c067a3714a6a7c622015138af096ab5d2')
p.add_argument('--board-tag', action='store_true')
a = p.parse_args()
blob = subprocess.check_output(['git', '--git-dir', a.sdk_git_dir, 'show',
                               a.sdk_ref + ':libs/hardware/BoardConfig/include/BoardConfig.h'])
text = blob.decode()
flags = sorted(set(re.findall(r'#ifndef\s+(FREEINK_DEVICE_\w+)', text)))
assert 'FREEINK_DEVICE_STICKY' in flags
selection = text.split('// Compile-time default device', 1)[1].split('// True when', 1)[0]
assert set(re.findall(r'FREEINK_DEVICE_\w+', selection)) <= set(flags)
failures = []
cases = []
with tempfile.TemporaryDirectory(prefix='latest-sdk-admission-') as tmp:
    tmp = Path(tmp)
    (tmp / 'BoardConfig.h').write_text('#define FREEINK_CAP_TOUCH 1\n#define FREEINK_CAP_USB_MSC 0\n')
    (tmp / 'admission.cpp').write_text('#include "AppCapabilities.h"\nint main() {}\n')
    common = ['c++', '-std=c++20', '-fsyntax-only', '-I' + str(tmp),
              '-I' + str(ROOT / 'include'), '-DARDUINO_ARCH_ESP32=1',
              '-DCROSSSMUDGE_APP_CAP_TOUCH=1', '-DCROSSINK_APP_CAP_USB_DRIVE=0']
    targets = [('app', str(tmp / 'admission.cpp'))]
    if a.board_tag:
        targets.append(('board-tag', str(ROOT / 'src/network/FirmwareBoardTag.cpp')))
    for name, source in targets:
        for enabled, allowed in [(['FREEINK_DEVICE_STICKY'], True), ([], False)]:
            r = subprocess.run(common + ['-D' + f + '=1' for f in enabled] + [source], capture_output=True)
            cases.append({'target': name, 'enabled': enabled, 'expected_allowed': allowed, 'exit': r.returncode})
            if (r.returncode == 0) != allowed:
                failures.append(name + ': incorrect admission ' + repr(enabled))
        for flag in flags:
            if flag == 'FREEINK_DEVICE_STICKY':
                continue
            r = subprocess.run(common + ['-DFREEINK_DEVICE_STICKY=1', '-D' + flag + '=1', source], capture_output=True)
            cases.append({'target': name, 'enabled': ['FREEINK_DEVICE_STICKY', flag], 'expected_allowed': False, 'exit': r.returncode})
            if r.returncode == 0:
                failures.append(name + ': competing profile admitted: ' + flag)
print(json.dumps({'sdk_ref': a.sdk_ref, 'board_config_sha256': hashlib.sha256(blob).hexdigest(),
                  'normalized_flags': flags, 'cases': cases, 'failures': failures}, indent=2))
raise SystemExit(bool(failures))
