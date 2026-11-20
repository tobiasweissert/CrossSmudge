#!/usr/bin/env python3
"""Read-only cached ESP32-S3 SDK syntax check; no SDK generation/link/RF."""
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]
OUT=Path('/root/.hermes/task-state/crosscub-findmy-implementation/transport')
PACKAGES=Path('/root/github/.host-data/crosssmudge/browser-platformio/packages')
SDK=PACKAGES/'framework-arduinoespressif32-libs/esp32s3'
CXX=PACKAGES/'toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-g++'
config=SDK/'dio_qspi/include'
includes=[config,SDK/'include',SDK/'include/freertos/config/include/freertos']
for p in sorted((SDK/'include').rglob('*')):
    if p.is_dir() and p.name in ('include','esp_additions','register','freertos','platform_include') and 'deprecated_include' not in p.parts:
        includes.append(p)
cmd=[str(CXX),'-std=gnu++17','-fsyntax-only','-DESP_PLATFORM=1',
     '-DARDUINO_ARCH_ESP32=1','-DCROSSCUB_FINDMY_EXPERIMENTAL=1',
     '-I'+str(ROOT/'lib/HalFindMyBle')]
cmd += ['-I'+str(p) for p in includes]
cmd += [str(ROOT/'lib/HalFindMyBle/HalFindMyBle.cpp'),str(ROOT/'lib/HalFindMyBle/EarlyBtRetention.cpp')]
OUT.mkdir(parents=True,exist_ok=True)
# No forced sdkconfig: each production translation unit must select S3 itself.
preprocess=[arg for arg in cmd if arg != '-fsyntax-only']
preprocess.insert(1,'-E')
pre=subprocess.run(preprocess,capture_output=True,text=True,check=True)
assert 'rc = nimble_port_init()' in pre.stdout, 'Actual native implementation was preprocessed away'
assert 'bool btInUse() { return true; }' in pre.stdout, 'Retention override was preprocessed away'
print('SDK syntax only, no link/radio\n'+' '.join(cmd),flush=True)
r=subprocess.run(cmd)
sys.exit(r.returncode)
