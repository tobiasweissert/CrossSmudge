import subprocess,os,pathlib,json,time,argparse
parser=argparse.ArgumentParser();parser.add_argument('--output',type=pathlib.Path,required=True);args=parser.parse_args()
root=pathlib.Path(__file__).resolve().parents[2];os.chdir(root)
H=args.output.resolve();H.mkdir(parents=True,exist_ok=True);H.joinpath('host').mkdir(exist_ok=True)
env=dict(os.environ,MINESWEEPER_EVIDENCE=str(H/'host/minesweeper'),REPLACEMENT_EVIDENCE=str(H/'host'),TMPDIR='/root/.hermes/cache/scratch/sticky-main-integration-build',CROSSPLAY_EVIDENCE=str(H/'host'),SMUDGE_X3_CONSTRAINTS='1',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',ARDUINOJSON_INCLUDE_DIR=os.environ.get('ARDUINOJSON_INCLUDE_DIR',str(root/'.pio/libdeps/sticky-owner-test/ArduinoJson/src')))
commands=[['python3',p] for p in ['test/repair_round1/run.py','test/lua_startup/run.py','test/launcher_swipe.py','test/lua_http/auto_time_test.py','test/lua_allocator/run.py','test/lua_device_identity/test_device_identity.py','test/app_store_wifi/run_test.py','test/crossplay_ports/run.py','test/crossplay_ports/additions_run.py','test/crossplay_ports/five_run.py','test/replacement_chess/run.py','test/replacement_sudoku/run.py','test/replacement_solitaire/run.py','test/replacement_wordle/run.py','test/solitaire_touch/run.py','test/sticky_app_touch/dashboard/run.py','test/sticky_app_touch/hourglass/run.py','test/sticky_app_touch/watertracker/run.py','test/2048_design/run.py','test/minesweeper_graphics/run.py','test/board_visual_ports/run.py','test/chess_graphics/run.py','test/hearts_jaipur_graphics/run.py','test/murdle_ui/run.py','test/party_ui/run.py','test/seasalt_graphics/run.py','test/utility_ui/run.py','test/integration_catalog/run_sandbox.py']]
for cmd in commands:
 if cmd[1]=='test/solitaire_touch/run.py':cmd.extend(['--devices','x3','x4','x4pro','simulator','missing'])
 if cmd[1]=='test/2048_design/run.py':cmd.append(str(H/'host/2048'))
commands += [['bash','test/network_apps/run.sh'],['bash','test/app_store/run_tests.sh'],['python3','scripts/package_owner_test_apps.py',str(H/'owner-packages')],['python3','test/integration_catalog/check_catalog.py','--owner-overlay',str(H/'owner-packages/apps-SD-overlay.zip')],['bash','test/lua_http/run.sh']]
results=[]
for i,cmd in enumerate(commands):
 name=str(i)+'-'+cmd[1].replace('/','__');log=H/'host'/(name+'.log');start=time.time()
 try:
  with log.open('w') as f:p=subprocess.run(cmd,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=900)
  code=p.returncode
 except subprocess.TimeoutExpired:code=124
 row={'command':cmd,'exit':code,'seconds':round(time.time()-start,2),'log':str(log)};results.append(row);(H/'host-results.json').write_text(json.dumps(results,indent=2)+'\n');print(i,code,cmd,flush=True)

raise SystemExit(any(row['exit'] for row in results))
