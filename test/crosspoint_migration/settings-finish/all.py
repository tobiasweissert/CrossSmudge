#!/usr/bin/env python3
"""Standalone six-path layer suite; every bounded child gets a durable receipt."""
from pathlib import Path
import argparse,json,subprocess,sys
T=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
results=[]
for name,script in [('storage','run.py'),('catalog','catalog.py'),('interfaces','interfaces.py')]:
 cmd=[sys.executable,str(T/script),'--evidence',str(a.evidence/name)]+(['--sanitize'] if a.sanitize else [])
 r=subprocess.run(cmd,capture_output=True,text=True,timeout=115)
 (a.evidence/(name+'.outer.log')).write_text(r.stdout+r.stderr)
 results.append({'suite':name,'returncode':r.returncode,'command':cmd})
 (a.evidence/'results.json').write_text(json.dumps(results,indent=2))
 print(name,r.returncode,r.stdout+r.stderr,flush=True)
raise SystemExit(any(r['returncode'] for r in results))
