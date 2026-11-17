#!/usr/bin/env python3
"""Compatibility entrypoint: the owner profile is now the normal Crosscub product.
All actual qualification lives in the current-design Crosscub host runner.
No firmware build, publication, shared-cache write or deployment is performed.
"""
import argparse,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2]
raise SystemExit(subprocess.run([sys.executable,str(root/'test/crosscub/run_all.py'),'--output',str(a.output.resolve())],cwd=root).returncode)
