#!/usr/bin/env python3
"""Independent CrossPlay chess source oracle and exact stock Lua ABI tests.
Run: TMPDIR=/root/.hermes/cache/scratch python3 test/replacement_chess/run.py
Set CROSSPLAY_ROOT to the read-only original CrossPlay checkout.
No SDK, firmware, network, or third-party Python packages are required.
"""
import hashlib,json,os,pathlib,shutil,subprocess,tempfile,time
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SOURCE=pathlib.Path(os.environ['CROSSPLAY_ROOT'])/'src/apps_local/chess' if os.environ.get('CROSSPLAY_ROOT') else HERE/'oracle'
APP=pathlib.Path(os.environ.get('CHESS_APP_DIR',str(ROOT/'apps/chess')))
OUT=pathlib.Path(os.environ.get('REPLACEMENT_EVIDENCE','/root/github/.host-data/crosssmudge/replacement-apps'))
SCRATCH=pathlib.Path(os.environ.get('TMPDIR','/root/.hermes/cache/scratch'))
SCRATCH.mkdir(parents=True,exist_ok=True)
results=[]
with tempfile.TemporaryDirectory(prefix='replacement-chess-',dir=SCRATCH) as tmp:
 work=pathlib.Path(tmp);core=SOURCE
 source_hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [core/'ChessCore.cpp',core/'ChessCore.h',core/'ChessEngine.cpp',core/'ChessEngine.h']}
 subprocess.run(['c++','-O2','-std=c++17','-I'+str(core),str(HERE/'oracle.cpp'),str(core/'ChessCore.cpp'),str(core/'ChessEngine.cpp'),'-o',str(work/'oracle')],check=True)
 subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),str(HERE/'runner.c'),*map(str,sorted((ROOT/'lib/Lua/src').glob('*.c'))),'-lm','-o',str(work/'runner')],check=True)
 app=work/'app';shutil.copytree(APP,app);(app/'legacy').mkdir()
 oracle=subprocess.check_output([str(work/'oracle')],text=True,timeout=60)
 cases=[v for v in oracle.split('END\n') if v.strip()]
 for i,case in enumerate(cases):(app/f'oracle-{i}.tsv').write_text(case+'END\n')
 (app/'oracle-count').write_text(str(len(cases)))
 def execute(test,cap=2097152,w=480,h=800,expected_failure=False,expected_error=None):
  begin=time.monotonic()
  p=subprocess.run([str(work/'runner'),str(HERE/test),str(cap),str(w),str(h)],cwd=app,text=True,capture_output=True,timeout=180)
  lines=p.stdout.strip().splitlines();row={'test':test,'expected_failure':expected_failure,'expected_error':expected_error,'cap':cap,'width':w,'height':h,'returncode':p.returncode,'seconds':round(time.monotonic()-begin,3),'stdout':p.stdout,'stderr':p.stderr,'metrics':json.loads(lines[-1]) if lines else {}}
  results.append(row);print(json.dumps(row),flush=True)
 baseline=(HERE/'fixtures/legacy-engine.lua').read_bytes()
 (app/'legacy-engine.lua').write_bytes(baseline);(app/'baseline-marker').write_text('baseline')
 execute('promotion.lua',expected_failure=True)
 (app/'baseline-marker').unlink();(app/'legacy-engine.lua').unlink()
 execute('promotion.lua')
 execute('compare.lua')
 execute('journal.lua')
 execute('validation.lua')
 execute('history.lua')
 execute('invariants.lua')
 execute('invariants.lua',76800,expected_error='not enough memory')
 execute('lean.lua',76800)
 execute('lean.lua',2097152)
 if (HERE/'runtime.lua').exists():
  for cap in [2097152]:
   for w,h in [(480,800),(800,480)]:execute('runtime.lua',cap,w,h)
 OUT.mkdir(parents=True,exist_ok=True)
 for w,h in [(480,800),(800,480)]:
  execute('capture.lua',2097152,w,h)
  shutil.copyfile(app/'capture.svg',OUT/f'chess-{w}x{h}-adapter.svg')
 report={'app_sha256':{str(p.relative_to(APP)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(APP.rglob('*')) if p.is_file()},'source_sha256':source_hashes,'oracle_positions':len(cases),'adapter':'Native stock API adapter; not hardware/rendering performance','results':results}
 (OUT/'chess-test-results.json').write_text(json.dumps(report,indent=2)+'\n')
 assert all((r['returncode']==1 and 'required four legal promotions; got 1' in r['stderr'] if r['expected_failure'] else (r['returncode']==1 and r['expected_error'] in r['stderr'] if r['expected_error'] else r['returncode']==0)) and r['metrics'].get('after_close')==0 for r in results),'test failure; see evidence'
