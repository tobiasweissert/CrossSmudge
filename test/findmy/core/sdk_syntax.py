#!/usr/bin/env python3
"""Actual S3/qio_opi mbedTLS backend syntax check; no link or hardware execution."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
PACKAGES = Path('/root/github/.host-data/crosssmudge/browser-platformio/packages')
SDK = PACKAGES / 'framework-arduinoespressif32-libs/esp32s3'
CXX = PACKAGES / 'toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-g++'
includes = [SDK / 'qio_opi/include', SDK / 'include', SDK / 'include/freertos/config/include/freertos',
            SDK / 'include/esp_rom/esp32s3/include/esp32s3']
includes += [p for p in sorted((SDK / 'include').rglob('*')) if p.is_dir() and p.name in ('include', 'esp_additions', 'register', 'freertos', 'platform_include') and 'deprecated_include' not in p.parts]
base = [str(CXX), '-std=gnu++17', '-DESP_PLATFORM=1', '-DFINDMY_CRYPTO_MBEDTLS=1',
        '-DMBEDTLS_CONFIG_FILE="mbedtls/esp_config.h"', '-I' + str(ROOT / 'lib/FindMy')]
base += ['-I' + str(p) for p in includes]
sources = [str(ROOT / 'lib/FindMy/FindMyCore.cpp'), str(ROOT / 'lib/FindMy/FindMyCrypto.cpp')]
pre = subprocess.run(base + ['-E'] + sources, capture_output=True, text=True)
if pre.returncode:
    print(pre.stderr, file=sys.stderr)
    sys.exit(pre.returncode)
assert 'rc=mbedtls_ecp_mul' in pre.stdout, 'mbedTLS curve backend preprocessed away'
result = subprocess.run(base + ['-fsyntax-only'] + sources, capture_output=True, text=True)
if result.stdout:
    print(result.stdout)
if result.stderr:
    print(result.stderr, file=sys.stderr)
print('Actual ESP32-S3 qio_opi mbedTLS backend syntax:', 'PASS' if result.returncode == 0 else 'FAIL')
print('No SDK regeneration, linking, controller, account or physical execution.')
sys.exit(result.returncode)
