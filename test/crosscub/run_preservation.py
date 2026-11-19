#!/usr/bin/env python3
"""Run existing preservation suites with bounded, private evidence paths.
No installs, firmware builds, SDK/cache writes, Git operations or network calls.
"""
from pathlib import Path
import argparse,json,os,subprocess,sys,time
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--group',choices=['apps','core'],default='apps');a=p.parse_args()
O=a.output.resolve();O.mkdir(parents=True,exist_ok=True);S=Path(os.environ.get('TMPDIR',str(O/'scratch')))/'crosscub-tests';S.mkdir(parents=True,exist_ok=True)
# Current original-design UI callbacks run separately through run_ui.py.
# The older *_graphics/*_ui wrappers assert superseded front-door layouts;
# retaining them as historical evidence is not a current Sticky acceptance run.
app_tests=['test/lua_startup/run.py','test/lua_allocator/run.py','test/lua_device_identity/test_device_identity.py','test/app_store_wifi/run_test.py','test/crossplay_ports/run.py','test/crossplay_ports/additions_run.py','test/crossplay_ports/five_run.py','test/replacement_chess/run.py','test/replacement_sudoku/run.py','test/replacement_solitaire/run.py','test/replacement_wordle/run.py','test/sticky_app_touch/dashboard/run.py','test/sticky_app_touch/hourglass/run.py','test/sticky_app_touch/watertracker/run.py','test/2048_design/run.py','test/minesweeper_graphics/run.py','test/hearts_jaipur_graphics/run.py','test/seasalt_graphics/run.py','test/integration_catalog/run_sandbox.py']
core_tests=['test/browser_assisted/run.py','test/browser_assisted/replay.py','test/browser_assisted/guard.py','test/airdrop/consent_epoch_run.py','test/airdrop/coordinator_run.py','test/airdrop/radio_gate_run.py','test/airdrop/recovery_cancel_run.py','test/airdrop/recovery_ownership_run.py','test/airdrop/inbox_visibility_run.py','test/airdrop/session_receiver_run.py','test/airdrop/ui_render.py','test/airdrop/discovery_run.py','test/airdrop/storage_run.py','test/lua_http/run.sh','test/app_store/run_tests.sh','test/network_apps/run.sh']
commands=app_tests if a.group=='apps' else core_tests
results=[]
env={**os.environ,'TMPDIR':str(S),'CROSSPLAY_EVIDENCE':str(O),'REPLACEMENT_EVIDENCE':str(O),'MINESWEEPER_EVIDENCE':str(O/'minesweeper'),'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','APP_STORE_TEST_BUILD_DIR':str(S/'app-store'),'NETWORK_TEST_OUT':str(S/'network')}
# Legacy test-only evidence defaults predate TMPDIR support. Execute identical
# tests with just those output literals redirected; no production code changes.
wrapper='import sys;from pathlib import Path;p=Path(sys.argv[1]);sys.path.insert(0,str(p.parent));sys.argv=sys.argv[1:];s=p.read_text();exec(compile(s.replace("/tmp/",__import__("os").environ["TMPDIR"]+"/"),str(p),"exec"),{"__name__":"__main__","__file__":str(p)})'
for rel in commands:
 path=R/rel
 if not path.exists():results.append({'test':rel,'exit':None,'status':'missing runner'});continue
 cmd=[sys.executable,'-c',wrapper,str(path)] if path.suffix=='.py' else ['bash',str(path)]
 if rel=='test/2048_design/run.py':cmd.append(str(O/'2048'))
 if rel=='test/airdrop/session_receiver_run.py':cmd.extend(['--sdk',os.environ['ESP_IDF_SOURCE_ROOT']])
 if rel=='test/airdrop/ui_render.py':cmd.extend(['--output',str(O/'airdrop-ui')])
 log=O/(rel.replace('/','__')+'.log');start=time.monotonic()
 try:
  with log.open('w') as f:r=subprocess.run(cmd,cwd=R,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=600)
  code=r.returncode
 except subprocess.TimeoutExpired:code=124
 results.append({'test':rel,'exit':code,'seconds':round(time.monotonic()-start,2),'log':str(log)})
 (O/f'{a.group}-results.json').write_text(json.dumps(results,indent=2)+'\n')
 print(rel,code,flush=True)
sys.exit(any(row['exit']!=0 for row in results))
