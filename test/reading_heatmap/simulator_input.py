#!/usr/bin/env python3
"""Offline SDL input qualification for the production-bound heatmap launch fixture.
Supply the hash-pinned fixture binary; never uses a generic/stale simulator.
"""
from pathlib import Path
import argparse, subprocess, os, re, json, hashlib
p=argparse.ArgumentParser();p.add_argument('--simulator',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
manifest=json.loads(a.simulator.resolve().with_name('render-inputs.json').read_text())
assert hashlib.sha256(a.simulator.read_bytes()).hexdigest()==manifest['binary'], 'Simulator binary hash mismatch'
R=Path(__file__).resolve().parents[2]
for path,digest in manifest['candidate'].items():
 assert hashlib.sha256((R/path).read_bytes()).hexdigest()==digest, 'Simulator source mismatch: '+path
rows=[]
cases=[
 ('navigation','1200:RIGHT;1800:SWIPE:350,400,100,400,200;2500:TAP:194,780;3100:TAP:282,780;3700:LEFT;4200:RIGHT;4800:QUIT',[0,1,2,0,2,1,2],False),
 ('aggregate','1200:RIGHT;1800:RIGHT;2400:RIGHT;3000:LEFT;3600:LEFT;4200:RIGHT;4800:QUIT',[0,1,2,3,2,1,2],False),
 ('landscape','1200:RIGHT;1800:RIGHT;2400:LEFT;3000:RIGHT;4800:QUIT',[0,1,2,1,2],False),
 ('edit-dates','1200:RIGHT;1800:CONFIRM;2500:BACK;3200:RIGHT;4800:QUIT',[0,1,4,1,2],False),
 ('hardware-previous','1200:RIGHT;1800:LEFT;2400:RIGHT;3000:RIGHT;4800:QUIT',[0,1,0,1,2],False),
 ('aggregate-touch','1200:TAP:306,780;1800:TAP:174,780;2400:RIGHT;3000:RIGHT;4800:QUIT',[0,3,0,1,2],False),
 ('no-book','1200:RIGHT;1800:RIGHT;4800:QUIT',[0,1,2],False),
 ('hardware-back','4700:BACK;5300:QUIT',[0],True),
 ('header-back','4700:TAP:24,50;5300:QUIT',[0],True),
 ('confirm-exit','4700:CONFIRM;5300:QUIT',[0],True),
 ('swipe-exit','4700:SWIPE:100,400,350,400,200;5500:QUIT',[0],True),
]
for name,schedule,expected,exits in cases:
 work=O/('runtime-'+name);(work/'fs_/.crosssmudge').mkdir(parents=True,exist_ok=True)
 env={**os.environ,'HEATMAP_NAV':'1','CROSSPOINT_SIM_INPUT_SCRIPT':schedule,'SDL_AUDIODRIVER':'dummy','LIBGL_ALWAYS_SOFTWARE':'1'}
 for key in ['HEATMAP_STATE','HEATMAP_AGGREGATE','HEATMAP_LANDSCAPE','HEATMAP_NO_BOOK']:
  env.pop(key,None)
 if name.startswith('aggregate'):env['HEATMAP_AGGREGATE']='1'
 if name=='no-book':env['HEATMAP_NO_BOOK']='1'
 if name=='landscape':env['HEATMAP_LANDSCAPE']='1'
 env.pop('CROSSPOINT_SIM_SCREENSHOTS',None)
 cmd=['unshare','-n','xvfb-run','-a','-s','-screen 0 1024x1024x24',str(a.simulator.resolve())]
 with (O/(name+'.log')).open('w') as f:r=subprocess.run(cmd,cwd=work,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=20)
 text=(O/(name+'.log')).read_text();pages=[int(x) for x in re.findall(r'HEATMAP_PAGE=(\d+)',text)]
 did_exit='Exiting activity: BookStats' in text
 row={'case':name,'exit':r.returncode,'pages':pages,'expected':expected,'activity_exited':did_exit,'expected_exit':exits,'schedule':schedule,'boundary':'real simulator HalGPIO -> MappedInputManager -> candidate Activity loop and render','network':'unshare -n'}
 rows.append(row);(O/'results.json').write_text(json.dumps(rows,indent=2));print(row,flush=True)
 assert r.returncode==0 and pages==expected and did_exit==exits
