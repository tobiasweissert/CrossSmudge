#!/usr/bin/env python3
"""Frozen exact-path reader UI + inherited reader/lifecycle regression gate."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--freeze',required=True,type=Path)
parser.add_argument('--evidence',required=True,type=Path)
args=parser.parse_args()
args.evidence.mkdir(parents=True,exist_ok=True)
manifest=json.loads(args.freeze.read_text())

def verify():
    for name,record in (manifest['files'] | manifest.get('dependencies', {})).items():
        data=(ROOT/name).read_bytes()
        assert len(data)==record['bytes'] and hashlib.sha256(data).hexdigest()==record['sha256'],name

verify()
commands=[
 ('normal',[sys.executable,str(HERE/'run.py')]),
 ('sanitized',[sys.executable,str(HERE/'run.py'),'--sanitize']),
 ('inherited-reader',[sys.executable,str(HERE.parent/'reader-activities/all.py')]),
 ('inherited-lifecycle-normal',[sys.executable,str(HERE.parent/'lifecycle/run.py')]),
 ('inherited-lifecycle-sanitized',[sys.executable,str(HERE.parent/'lifecycle/run.py'),'--sanitize']),
]
results=[]
for label,argv in commands:
    target=args.evidence/label
    argv += ['--evidence',str(target)]
    try:
        proc=subprocess.run(argv,capture_output=True,text=True,timeout=110)
        code=proc.returncode;log=proc.stdout+proc.stderr
    except subprocess.TimeoutExpired as exc:
        code=124;log=str(exc)
    (args.evidence/(label+'.log')).write_text(log)
    record={'name':label,'argv':argv,'returncode':code}
    if code==0 and label in ('normal','sanitized'):
        report=json.loads((target/'summary.json').read_text())
        assert report['all_passed'] and report['suites']==['chrome','consumers','menu','stats','session','popup']
        record['suites']=report['suites']
    if code==0 and label=='inherited-reader':
        report=json.loads((target/'summary.json').read_text())
        assert report['expected']==report['completed']==14 and all(item['passed'] for item in report['results'])
        record['identities']=[item['name'] for item in report['results']]
    results.append(record)
    (args.evidence/'summary.json').write_text(json.dumps({'results':results,'complete':False},indent=2)+'\n')
    print(label,code,flush=True)
    if code:
        print(log)
        raise SystemExit(code)
verify()
assert len(results)==len(commands)
(args.evidence/'summary.json').write_text(json.dumps({'results':results,'complete':True,'freeze_sha256':hashlib.sha256(args.freeze.read_bytes()).hexdigest()},indent=2)+'\n')
print('PASS frozen reader UI, reader and lifecycle gates; no target/visual/device qualification')
