#!/usr/bin/env python3
"""Compile the real settings header's compatibility methods over existing ArduinoJson.
Hardware/renderer/BLE type doubles; does not compile SettingsList or activities.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from test_migration import ROOT
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='preferences-',dir=os.environ.get('TMPDIR')) as d:
    d=Path(d);(d/'Epub').mkdir()
    (d/'Epub/ReaderRenderSpec.h').write_text('#pragma once\nenum class EpubRenderMode { CrossSmudgeDefault };\nstruct ReaderRenderSpec {};\n')
    (d/'HalStorage.h').write_text('#pragma once\nclass HalFile {};\nusing FsFile=HalFile;\n')
    (d/'BlePageTurner.h').write_text('#pragma once\nnamespace bleturner {struct Config {bool enabled=false;};}\n')
    (d/'BlePageTurnerJson.h').write_text('#pragma once\n#include <ArduinoJson.h>\n#include <BlePageTurner.h>\nnamespace bleturner {inline void writeJson(const Config& c,JsonDocument& d){d["bleEnabled"]=c.enabled;} inline bool readJson(Config& c,JsonVariantConst d){c.enabled=d["bleEnabled"] | false;return true;}}\n')
    for name in ['Arduino.h','Logging.h']:(d/name).write_bytes((Path(__file__).parent/'stubs'/name).read_bytes())
    cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-DARDUINOJSON_ENABLE_ARDUINO_STRING=0','-I'+str(d),'-I'+os.environ.get('ARDUINOJSON_INCLUDE','/root/github/.host-data/crosssmudge/gallery-native-deps/sticky-simulator/ArduinoJson/src'),'-I'+str(ROOT/'lib/Serialization'),'-I'+str(ROOT/'src'),str(Path(__file__).parent/'preferences.cpp'),'-o',str(d/'probe')]
    if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    c=subprocess.run(cmd,capture_output=True,text=True,timeout=60);(a.evidence/'compile.log').write_text(c.stdout+c.stderr)
    receipt={'command':cmd,'compile_exit':c.returncode,'header_sha256':hashlib.sha256((ROOT/'src/CrossPointSettings.h').read_bytes()).hexdigest(),'coverage':'production header compatibility methods and ArduinoJson; hardware, renderer, BLE type doubles; no SettingsList/activities'}
    if c.returncode==0:
        r=subprocess.run([str(d/'probe')],capture_output=True,text=True,timeout=30);receipt['run_exit']=r.returncode;(a.evidence/'run.log').write_text(r.stdout+r.stderr)
    (a.evidence/'receipt.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt,indent=2))
    raise SystemExit(receipt.get('run_exit',c.returncode))
