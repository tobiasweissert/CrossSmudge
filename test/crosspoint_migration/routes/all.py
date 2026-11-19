#!/usr/bin/env python3
"""Routine source-bound routes suite; every child <=95 seconds, no install/build."""
from pathlib import Path
import argparse, subprocess, sys, json, hashlib
R=Path(__file__).resolve().parents[3];HERE=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
flags=['--sanitize'] if a.sanitize else [];commands=[]
for name in ['contracts','run','flows','input_core','status_bars','images','sync_settings']:
 commands.append((name,[sys.executable,str(HERE/(name+'.py')),*flags,'--evidence',str(a.evidence/name)]))
commands += [('recents',[sys.executable,str(R/'test/file_actions/recents_run.py'),*flags]),('ota',[sys.executable,str(R/'test/ota_transition/run.py'),*flags,'--output',str(a.evidence/'ota')])]
dependency_paths=[R/p for p in ['src/util/BookMoveUtils.h','src/util/BookMoveUtils.cpp','src/activities/home/BookActions.cpp','src/activities/home/FileActionPaths.h','src/activities/home/FileMutationInputGate.h','src/activities/home/FileBrowserActionActivity.h','src/CrossPointSettings.h','src/main.cpp','src/SilentRestart.h','lib/HttpHeader/HttpHeader.h','lib/HttpHeader/HttpHeader.cpp','lib/AsciiText/AsciiText.h']]
for subtree in ['test/file_actions','test/ota_transition','test/crosspoint_migration/routes']:
 dependency_paths += [f for f in (R/subtree).rglob('*') if f.is_file() and f.suffix in {'.py','.h','.cpp'}]
def hashes(paths):return {str(p.relative_to(R)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths if p.exists()}
dependency_hashes=hashes(dependency_paths)
results=[]
for name,cmd in commands:
 try:
  c=subprocess.run(cmd,cwd=R,capture_output=True,text=True,timeout=95);code=c.returncode;output=c.stdout+c.stderr
 except subprocess.TimeoutExpired as ex:code=124;output=str(ex)
 log=a.evidence/(name+'.log');log.write_text('COMMAND '+repr(cmd)+'\n'+output+'\nEXIT='+str(code)+'\n')
 cases=[l for l in output.splitlines() if l.startswith('PASS ')]
 if name=='run' and (a.evidence/name/'results.json').exists():
  cases=[x['name']+': '+line for x in json.loads((a.evidence/name/'results.json').read_text()) for line in x.get('passes',[])]
 results.append({'name':name,'rc':code,'cases':cases,'log':str(log),'script_sha256':hashlib.sha256(Path(cmd[1]).read_bytes()).hexdigest()})
 (a.evidence/'suite.json').write_text(json.dumps({'sanitize':a.sanitize,'complete':len(results)==len(commands),'results':results},indent=2)+'\n')
 print(name,code,len(cases),flush=True)
if any(x['rc'] or not x['cases'] for x in results):sys.exit(1)
owned=json.loads((a.evidence/'contracts/contracts-results.json').read_text())['hashes']
drift=[p for p,h in owned.items() if hashlib.sha256((R/p).read_bytes()).hexdigest()!=h]
drift += [p for p,h in dependency_hashes.items() if hashlib.sha256((R/p).read_bytes()).hexdigest()!=h]
(a.evidence/'input-binding.json').write_text(json.dumps({'owned':owned,'dependencies':dependency_hashes,'drift':drift},indent=2)+'\n')
assert not drift,drift
cases=[x['name']+': '+c for x in results for c in x['cases']];assert len(cases)==len(set(cases))
print('PASS routes suite:',len(cases),'named cases; '+('ASan/UBSan' if a.sanitize else 'normal'))
