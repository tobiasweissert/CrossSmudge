#!/usr/bin/env python3
"""Host syntax qualification of full real production declarations and TUs.
Hardware-only doubles; actual pinned SDK/adapter/activity/dictionary types.
Invoke under the shared qualification flock. Not a firmware/target build.
"""
from pathlib import Path
import hashlib, importlib.util, json, os, subprocess, sys
R = Path(__file__).resolve().parents[3]
O = Path(sys.argv[1]).resolve()
O.mkdir(parents=True, exist_ok=True)
S = R/'freeink-sdk'
assert subprocess.check_output(['git','-C',str(S),'rev-parse','HEAD'],text=True).strip() == '634a3f8c067a3714a6a7c622015138af096ab5d2'
B = Path('/root/.hermes/cache/scratch/target-dictionary-declarations')
B.mkdir(exist_ok=True)
(B/'esp_random.h').write_text('#pragma once\n#include <cstdint>\nuint32_t esp_random();\n')
inc = [R/'test/crosspoint_migration/target-adapters/hardware', B, R/'src', R/'include', R/'lib/hal', Path('/root/github/crosspoint-reader/.pio/libdeps/sticky/SdFat/src'), Path('/root/github/crosspoint-reader/.pio/libdeps/sticky/ArduinoJson/src')]
inc += [x for x in (R/'lib').iterdir() if x.is_dir()]
inc += sorted((R/'lib').rglob('include'))
inc += sorted(x for x in (S/'libs').rglob('include') if x.is_dir())
inc += [S/'libs/display/FreeInkDisplay/src', Path('/root/github/crosssmudge/.pio/libdeps/sticky-simulator/simulator/src')]
spec = importlib.util.spec_from_file_location('gen', R/'scripts/gen_i18n.py')
assert spec is not None and spec.loader is not None
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)
loaded = gen.load_translations(str(R/'lib/I18n/translations'))
gen.generate_keys_header(*loaded[:4], str(B/'I18nKeys.h'), False, {'EN'})
cmd = ['g++','-std=c++20','-DFREEINK_DEVICE_STICKY','-DCROSSPOINT_EMULATED=0','-DARDUINOJSON_ENABLE_ARDUINO_STRING=0','-DARDUINOJSON_ENABLE_ARDUINO_STREAM=0','-DARDUINOJSON_ENABLE_PROGMEM=0','-DENABLE_ARDUINO_FEATURES=0','-DCROSSSMUDGE_APP_CAP_TOUCH=1','-DCROSSINK_APP_CAP_USB_DRIVE=0','-DSPI_DRIVER_SELECT=3','-DENABLE_ARDUINO_SERIAL=0','-DUSE_FCNTL_H=0','-fsyntax-only']
cmd += ['-include', 'Arduino.h']
cmd += ['-I'+str(p) for p in inc]
cmd += [str(R/s) for s in ['test/crosspoint_migration/target-dictionary/declarations.cpp','src/activities/reader/DictionaryDefinitionActivity.cpp','src/util/DictionaryRegistry.cpp','src/vocabulary/VocabularyStore.cpp']]
p = subprocess.run(cmd,capture_output=True,text=True,timeout=160)
(O/'declarations.log').write_text(p.stdout+p.stderr)
receipt = {'argv':cmd,'compile_exit':p.returncode,'sdk_head':'634a3f8c067a3714a6a7c622015138af096ab5d2','boundary':'host full production declaration/TU syntax, hardware doubles, no target build'}
paths = [R/s for s in ['src/activities/reader/DictionaryDefinitionActivity.h','src/activities/reader/DictionaryDefinitionActivity.cpp','src/util/DictionaryRegistry.h','src/util/DictionaryRegistry.cpp','src/vocabulary/VocabularyStore.cpp','src/MappedInputManager.h','lib/GfxRenderer/GfxRenderer.h','lib/hal/HalStorage.h']]
paths += list((S/'libs/ui/FreeInkUI/include').rglob('*.h'))
receipt['inputs'] = {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
(O/'declarations.json').write_text(json.dumps(receipt,indent=2))
print('full production syntax exit',p.returncode)
print((p.stdout+p.stderr)[-10000:])
sys.exit(p.returncode)
