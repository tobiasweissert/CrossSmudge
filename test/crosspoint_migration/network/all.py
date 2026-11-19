#!/usr/bin/env python3
"""Routine bounded network-layer entrypoint (normal and ASan/UBSan)."""
from pathlib import Path
import subprocess,sys,json
T=Path(__file__).resolve().parent
results=[]
for script in ['run.py','http_run.py','policy_run.py','opds_run.py']:
 for mode in [[],['--sanitize']]:
  p=subprocess.run([sys.executable,str(T/script),*mode],timeout=90)
  results.append({'script':script,'mode':mode,'returncode':p.returncode})
O=Path('/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/integration/network/tests')
(O/'all.json').write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2))
sys.exit(1 if any(r['returncode'] for r in results) else 0)
