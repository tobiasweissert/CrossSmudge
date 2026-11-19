#!/usr/bin/env python3
"""Routine bounded entrypoint. Carried sibling suites are read-only dependencies."""
from pathlib import Path
import argparse,json,subprocess,sys
R=Path(__file__).resolve().parents[3];T=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--evidence',required=True,type=Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
results=[]
for name,script in [('web-files',T/'run.py'),('bookmarks',T/'bookmarks.py'),('clipping',T.parent/'settings-finish/run.py'),('catalog',T.parent/'settings-finish/catalog.py'),('interfaces',T.parent/'settings-finish/interfaces.py')]:
 cmd=[sys.executable,str(script),'--evidence',str(a.evidence/name)]+(['--sanitize'] if a.sanitize else [])
 r=subprocess.run(cmd,capture_output=True,text=True,timeout=115);(a.evidence/(name+'.log')).write_text(r.stdout+r.stderr);print(name,r.returncode,r.stdout+r.stderr,flush=True)
 results.append({'suite':name,'command':cmd,'returncode':r.returncode});(a.evidence/'all.json').write_text(json.dumps(results,indent=2))
 if r.returncode:raise SystemExit(r.returncode)
