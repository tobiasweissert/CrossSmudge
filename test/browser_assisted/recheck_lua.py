#!/usr/bin/env python3
"""Recheck changed Lua against qualified binaries only if native inputs match."""
import os,pathlib,json,hashlib,subprocess
R=pathlib.Path(__file__).resolve().parents[2];os.chdir(R)
O=pathlib.Path(os.environ['ASSISTED_TEST_OUT'])
prior=json.loads((O/'source-inputs.json').read_text())
for path,expected in prior.items():
 if not path.startswith('apps/browser/'):
  assert hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()==expected,'Native input changed: run full qualification first'
source={str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in pathlib.Path('apps/browser').glob('*')if p.is_file()}
receipt=json.loads((O/'receipt.json').read_text());assert all(r['exit']==0 for r in receipt)
commands=[r['cmd']for r in receipt if pathlib.Path(r['cmd'][0]).name in ('lua','font-lua')]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',ASSISTED_PNG_FIXTURE=str(O/'palette.png'),SMUDGE_X3_CONSTRAINTS='1',TMPDIR=str(O/'tmp'))
results=[]
for cmd in commands:
 p=subprocess.run(cmd,env=env);results.append({'cmd':cmd,'exit':p.returncode});p.check_returncode()
assert source=={p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()for p in source},'Lua source changed during qualification'
inputs={'apps':source,'native_sources':{p:h for p,h in prior.items()if not p.startswith('apps/browser/')},'binaries':{str(O/n):hashlib.sha256((O/n).read_bytes()).hexdigest()for n in ('lua','font-lua')if(O/n).exists()},'prior_receipt_sha256':hashlib.sha256((O/'receipt.json').read_bytes()).hexdigest(),'commands':results}
(O/'lua-recheck.json').write_text(json.dumps(inputs,indent=2)+'\n');print('PASS final Lua source-stable recheck against unchanged qualified native inputs:',len(commands))
