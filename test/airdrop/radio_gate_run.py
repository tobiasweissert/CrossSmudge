#!/usr/bin/env python3
from pathlib import Path
import argparse,subprocess,tempfile,json,hashlib
r=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');a=p.parse_args()
port=r/'lib/AirDrop/vendor/ESP32Drop/src/awdl/port'
with tempfile.TemporaryDirectory(prefix='radio-gate-')as td:
 for name,flags in [('esp32',['-DARDUINO_ARCH_ESP32=1']),('native',[]),('simulator',['-DARDUINO_ARCH_ESP32=1','-DSIMULATOR=1'])]:
  cmd=['g++','-std=c++20','-pthread','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(r/'test/airdrop/radio_stubs'),'-I'+str(port),*flags,str(r/'test/airdrop/radio_gate_test.cpp'),'-o',str(Path(td)/name)]
  if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
  subprocess.run(cmd,check=True);subprocess.run([str(Path(td)/name)],check=True)
 print(json.dumps({'exit':0,'profiles':3,'production_gate_sha256':hashlib.sha256((port/'awdl_owned_gate.h').read_bytes()).hexdigest(),'sanitized':a.sanitize,'scope':'actual conditional branch; fake FreeRTOS portMUX via host mutex for semantic exclusion only'}))
