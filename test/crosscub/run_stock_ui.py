#!/usr/bin/env python3
"""Exercise unchanged stock Lua apps in both Sticky viewports, no firmware build."""
from pathlib import Path
import argparse,json,subprocess,shutil,tempfile,sys
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
apps=['blackjack','blockdrop','codex','dailyoffice','dice','lifecounter','rosary','stopwatch'];rows=[]
with tempfile.TemporaryDirectory(prefix='stock-ui-')as tmp:
 S=Path(tmp);runner=S/'runner';subprocess.run(['cc','-O2','-I'+str(R/'lib/Lua/src'),str(R/'test/crossplay_ports/runner.c'),*map(str,sorted((R/'lib/Lua/src').glob('*.c'))),'-lm','-o',str(runner)],check=True)
 for app in apps:
  for w,h in [(480,800),(800,480)]:
   case=S/f'{app}-{w}';shutil.copytree(R/'apps'/app,case)
   t=subprocess.run([str(runner),str(R/'test/crosscub/stock_ui.lua'),'2097152',str(w),str(h)],cwd=case,capture_output=True,text=True,timeout=120)
   rows.append(dict(app=app,width=w,height=h,exit=t.returncode,stdout=t.stdout,stderr=t.stderr));(O/f'{app}-{w}x{h}.log').write_text(t.stdout+t.stderr);print(app,w,t.returncode,t.stderr.strip(),flush=True)
   (O/'results.json').write_text(json.dumps(rows,indent=2)+'\n')
sys.exit(any(r['exit']for r in rows))
