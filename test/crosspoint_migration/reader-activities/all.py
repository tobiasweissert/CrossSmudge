#!/usr/bin/env python3
"""Routine bounded reader migration entrypoint (normal + ASan/UBSan)."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
p=argparse.ArgumentParser()
p.add_argument('--evidence',required=True,type=Path)
p.add_argument('--freeze',type=Path)
a=p.parse_args()
a.evidence.mkdir(parents=True,exist_ok=True)
def verify():
    if not a.freeze:return
    manifest=json.loads(a.freeze.read_text())
    for path, expected in manifest['files'].items():
        actual=hashlib.sha256((ROOT/path).read_bytes()).hexdigest()
        assert actual==expected['sha256'],path
verify()
results=[]
for sanitize in [False,True]:
    for mode in ['stats','activities','txt','side','runtime','ble','codec']:
        name=mode+('-asan-ubsan' if sanitize else '-normal')
        out=a.evidence/name
        cmd=[sys.executable,str(HERE/'run.py'),'--evidence',str(out)]
        if mode!='stats':cmd+=['--'+mode]
        if sanitize:cmd+=['--sanitize']
        try:
            proc=subprocess.run(cmd,capture_output=True,text=True,timeout=110)
            log=proc.stdout+proc.stderr
            code=proc.returncode
        except subprocess.TimeoutExpired as error:
            log=str(error);code=124
        out.mkdir(exist_ok=True)
        (out/'outer.log').write_text(log)
        receipt=json.loads((out/'receipt.json').read_text()) if (out/'receipt.json').exists() else {}
        steps=receipt.get('steps',[])
        complete=[s['name'] for s in steps]==['compile','execute'] and all(s['returncode']==0 for s in steps)
        passed=code==0 and complete
        results.append({'name':name,'argv':cmd,'returncode':code,'complete':complete,'passed':passed})
        (a.evidence/'summary.json').write_text(json.dumps({'expected':14,'completed':len(results),'results':results},indent=2))
        print(name,'PASS' if passed else 'FAIL',flush=True)
        if not passed:
            print(log)
            sys.exit(1)
verify()
assert len(results)==14 and all(r['passed'] for r in results)
print('PASS 14/14 bounded production-TU/body probes; not a full activity/SDK/firmware compile')
