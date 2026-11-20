#!/usr/bin/env python3
"""Bounded cached Xtensa/ESP-IDF syntax probes, no link/build/download/RF."""
from pathlib import Path
import subprocess, tempfile, os, sys
ROOT=Path(__file__).resolve().parents[3]
OUT=Path('/root/.hermes/task-state/crosscub-findmy-implementation/integration')
PACKAGES=Path('/root/github/.host-data/crosssmudge/browser-platformio/packages')
SDK=PACKAGES/'framework-arduinoespressif32-libs/esp32s3'
ARDUINO=PACKAGES/'framework-arduinoespressif32'
CXX=PACKAGES/'toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-g++'
FREEINK=Path('/root/github/crosssmudge-browser-backend/freeink-sdk')
incs=[SDK/'qio_opi/include',SDK/'include',SDK/'include/freertos/config/include/freertos',SDK/'include/esp_rom/esp32s3/include/esp32s3',ARDUINO/'cores/esp32',ARDUINO/'variants/esp32s3',ROOT/'lib/hal',ROOT/'lib/FindMy',ROOT/'lib/FindMyRuntime',ROOT/'lib/HalFindMyBle',ROOT/'src',ROOT/'include']
incs += [p for p in sorted((SDK/'include').rglob('*')) if p.is_dir() and p.name in ('include','esp_additions','register','freertos','platform_include') and 'deprecated_include' not in p.parts]
incs += [p for p in sorted((ARDUINO/'libraries').glob('*/src')) if p.is_dir()]
incs += [p for p in sorted((ROOT/'lib').iterdir()) if p.is_dir()]
incs += [p for p in sorted((FREEINK/'libs').rglob('include')) if 'test' not in p.parts]
# Reuse populated production dependencies read-only; no simulator/header stubs.
DEPS=Path('/root/github/.host-data/crosssmudge/ota-libdeps/sticky')
incs += [DEPS/'SdFat/src', DEPS/'ArduinoJson/src']
base=[str(CXX),'-std=gnu++2a','-fsyntax-only','-DESP_PLATFORM=1','-DESP32=1','-DARDUINO=10819','-DFREEINK_DEVICE_STICKY=1','-DCROSSSMUDGE_APP_CAP_TOUCH=1','-DCROSSINK_APP_CAP_TOUCH=1','-DCROSSINK_APP_CAP_USB_DRIVE=0','-DARDUINO_ARCH_ESP32=1','-DCROSSCUB_FINDMY_EXPERIMENTAL=1','-DFINDMY_CRYPTO_MBEDTLS=1','-DMBEDTLS_CONFIG_FILE="mbedtls/esp_config.h"']+['-I'+str(p) for p in incs]
logs=[];code=0
with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR','/root/.hermes/cache/scratch')) as d:
 p=Path(d)/'runtime.cpp';p.write_text('#include <Runtime.h>\n#include <PrivateState.h>\n')
 for name,source in [('portable',p),('firmware',ROOT/'src/findmy/Firmware.cpp'),('owner-ui',ROOT/'src/activities/settings/FindMySettingsActivity.cpp')]:
  r=subprocess.run(base+[str(source)],capture_output=True,text=True)
  code=code or r.returncode
  logs.append(f'{name} actual cached Xtensa syntax EXIT {r.returncode}\n'+r.stdout+r.stderr)
text='\n'.join(logs)+'\nNo linking, SDK regeneration, installation or target execution.\n'
(OUT/'sdk-syntax.log').write_text(text);print(text);sys.exit(code)
