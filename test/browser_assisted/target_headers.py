#!/usr/bin/env python3
"""Read-only pinned-header syntax diagnostic, not PlatformIO/full firmware."""
import pathlib,subprocess,json,os
import sys
sys.dont_write_bytecode=True
from guard import check
check()
R=pathlib.Path(__file__).resolve().parents[2];os.chdir(R)
old=pathlib.Path('/root/github/crosssmudge-airdrop');packages=pathlib.Path('/root/github/.host-data/crosssmudge/browser-platformio/packages')
O=pathlib.Path(os.environ.get('ASSISTED_HEADER_OUT','/tmp/crosssmudge-target-headers'));O.mkdir(parents=True,exist_ok=True)
compiler=packages/'toolchain-xtensa-esp-elf/bin/xtensa-esp32s3-elf-g++'
includes=set()
for dependency in ['HalGPIO.cpp.d','HalLuaHttp.cpp.d','HalStorage.cpp.d']:
 p=old/'.pio/build/sticky-owner-test/libc50/hal'/dependency
 for line in p.read_text().splitlines()[1:]:
  raw=line.strip().removesuffix(' \\');header=pathlib.Path(raw)
  if not header.is_absolute():header=old/header
  if header.exists():
   h=str(header)
   if header.name in ('FreeRTOSConfig.h','FreeRTOSConfig_arch.h','portmacro.h','xtensa_config.h'):includes.add(str(header.parent))
   if h.startswith(str(old/'freeink-sdk')) and '/include/'in h:includes.add(h.split('/include/',1)[0]+'/include')
   if h.startswith(str(old/'.pio/libdeps')) and '/src/'in h:includes.add(h.split('/src/',1)[0]+'/src')
# Public include roots for transitive includes, derived from the old qualified headers.
for tree in [packages/'framework-arduinoespressif32-libs/esp32s3/include']:
 for p in [*tree.rglob('include'),*tree.rglob('register')]:
  if p.is_dir():includes.add(str(p))
includes.add(str(packages/'framework-arduinoespressif32-libs/esp32s3/include/freertos/FreeRTOS-Kernel/include'))
includes.add(str(packages/'framework-arduinoespressif32-libs/esp32s3/include/esp_rom/esp32s3'))
includes.add(str(packages/'framework-arduinoespressif32-libs/esp32s3/include/esp_rom/esp32s3/include/esp32s3'))
flags=['-std=gnu++2a','-fsyntax-only','-fno-exceptions','-fno-rtti','-mlongcalls','-DARDUINO_ARCH_ESP32=1','-DARDUINO=10812','-DESP_PLATFORM','-DBOARD_HAS_PSRAM=1','-DFREEINK_DEVICE_STICKY=1','-DCONFIG_IDF_TARGET_ESP32S3=1','-Ilib/hal','-Ilib/Logging','-Ilib/LuaHttp','-Ilib/Memory','-Ilib/Lua/src','-Ilib/LuaBrowser']
for p in [str(packages/'framework-arduinoespressif32/cores/esp32'),str(packages/'framework-arduinoespressif32/variants/esp32s3'),str(packages/'framework-arduinoespressif32/libraries/Preferences/src'),str(packages/'framework-arduinoespressif32-libs/esp32s3/qio_opi/include'),str(packages/'framework-arduinoespressif32-libs/esp32s3/include/newlib/platform_include'),*sorted(includes)]:flags+=['-I'+p]
results=[]
for source in ['lib/hal/HalBrowserBackend.cpp','lib/LuaHttp/BackendConfig.cpp','lib/LuaHttp/LuaHttp.cpp','lib/LuaHttp/HttpCore.cpp']:
 cmd=[str(compiler),*flags,source];p=subprocess.run(cmd,capture_output=True,text=True)
 (O/(pathlib.Path(source).stem+'.log')).write_text(p.stdout+p.stderr)
 results.append({'source':source,'command':cmd,'exit':p.returncode})
(O/'receipt.json').write_text(json.dumps(results,indent=2)+'\n')
for r in results:print(r['source'],r['exit'])
raise SystemExit(any(r['exit']for r in results))
