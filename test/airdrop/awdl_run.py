#!/usr/bin/env python3
"""Compile actual production stop methods against deterministic driver/task/FIFO seams."""
from pathlib import Path
import argparse, hashlib, json, os, subprocess, tempfile, sys
root=Path(__file__).resolve().parents[2]
port=root/'lib/AirDrop/vendor/ESP32Drop/src/awdl/port'
parser=argparse.ArgumentParser();parser.add_argument('--sanitize',action='store_true');args=parser.parse_args()
source=(port/'awdl_port_esp32.cpp').read_text()
# Extract complete methods, preserving their exact production text. No parallel lifecycle implementation.
names=['awdl_is_ready','awdl_is_stopped','awdl_request_stop','awdl_poll_stop','awdl_end']
parts=[]
for name in names:
    import re
    match=re.search(r'^(?:bool|void|esp_err_t) '+name+r'\(void\) \{',source,re.M)
    assert match, name
    start=match.start(); i=source.index('{',start); depth=1;j=i+1
    while depth:
        if source[j]=='{':depth+=1
        if source[j]=='}':depth-=1
        j+=1
    parts.append(source[start:j])
with tempfile.TemporaryDirectory(prefix='awdl-lifecycle-',dir=os.environ.get('TMPDIR','/tmp')) as tmp:
    tmp=Path(tmp);(tmp/'awdl_lifecycle_extracted.inc').write_text('\n'.join(parts)+'\n')
    command=['g++','-std=c++20','-g','-O1','-pthread','-Wall','-Wextra','-Werror','-I'+str(port),'-I'+str(tmp),str(root/'test/airdrop/awdl_lifecycle_test.cpp'),'-o',str(tmp/'test')]
    if args.sanitize:command[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer',*(['-no-pie'] if sys.platform.startswith('linux') else [])]
    subprocess.run(command,check=True)
    subprocess.run([str(tmp/'test')],check=True)
    print(json.dumps({'production_sha256':hashlib.sha256(source.encode()).hexdigest(),'methods':names,'sanitized':args.sanitize,'exit':0}))
