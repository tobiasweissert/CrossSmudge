#!/usr/bin/env python3
"""Standalone routine entrypoint; run each bounded suite with durable receipts."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
results=[]
for name in ['migration','preferences','utilities']:
    command=[sys.executable,str(Path(__file__).parent/f'test_{name}.py'),'--evidence',str(a.evidence/name)]
    if a.sanitize:command+=['--sanitize']
    r=subprocess.run(command,capture_output=True,text=True,timeout=110)
    (a.evidence/(name+'.log')).write_text(r.stdout+r.stderr)
    results.append({'suite':name,'exit':r.returncode})
    (a.evidence/'results.json').write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2));raise SystemExit(any(x['exit'] for x in results))
