#!/usr/bin/env python3
"""One bounded entry point for these three packages, no PIO or external writes."""
import argparse,json,subprocess
from pathlib import Path
H=Path(__file__).resolve().parent;p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--tooling',type=Path,required=True);p.add_argument('--native',type=Path,required=True);p.add_argument('--extra-native',type=Path,required=True);p.add_argument('--app-root',type=Path,default=H.parents[1]/'apps');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
for fixture in ('callbacks','gameplay','results','lifecycle','failed_records','long_records','malformed_records'):
 subprocess.run(['python3',str(H/'run.py'),'--fixture',fixture+'.lua','--output',str(a.output/fixture),'--tooling',str(a.tooling),'--native',str(a.native),'--extra-native',str(a.extra_native),'--app-root',str(a.app_root)],check=True)
results={name:json.loads((a.output/name/'results.json').read_text())for name in ('callbacks','gameplay','results','lifecycle','failed_records','long_records','malformed_records')}
(a.output/'results.json').write_text(json.dumps(dict(scope='Only cp-connectfour/cp-knucklebones/cp-checkers; finite supported-state references, no all24 approval',tests=sum(len(x['tests'])for x in results.values()),components=results),indent=2)+'\n')
