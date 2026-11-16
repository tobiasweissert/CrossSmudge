#!/usr/bin/env python3
"""Qualify all reconciled local SD candidates, serially, without publishing.

This is an integration gate over real app-specific callback/pixel routines.
Each routine retains its explicit original-orientation and service deviations.
"""
import argparse,hashlib,json,subprocess,sys,time,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--evidence',type=Path,required=True);p.add_argument('--packages',type=Path,required=True);p.add_argument('--reference',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--only-group',action='append');p.add_argument('--resume',action='store_true');a=p.parse_args();H=a.evidence.resolve();P=a.packages.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True);C=a.reference.resolve()
metadata=json.loads((P/'runtime-members.json').read_text());inventory=json.loads((H/'inventory/ledger.json').read_text());ids={r['target_id']for r in inventory['titles']if r['ported']};assert len(ids)==24 and metadata['complete_inventory']and{r['id']for r in metadata['packages']}==ids
A=P/'extracted/.crosssmudge/applications';A.mkdir(parents=True,exist_ok=True)
for row in metadata['packages']:
 archive=P/row['archive'];assert hashlib.sha256(archive.read_bytes()).hexdigest()==row['sha256']
 with zipfile.ZipFile(archive)as z:
  assert z.testzip()is None
  for name,entry in row['members'].items():
   source=ROOT/'apps'/row['id']/name;data=z.read('.crosssmudge/applications/'+row['id']+'/'+name);assert hashlib.sha256(data).hexdigest()==entry['sha256']and source.read_bytes()==data,(row['id'],name)
  z.extractall(P/'extracted')
 assert {str(f.relative_to(A/row['id']))for f in(A/row['id']).rglob('*')if f.is_file()}==set(row['members'])
T=O/'root/tooling';N=H/'native-catalog-references/native-catalog'
def py(path,*args):return[sys.executable,str(ROOT/path),*map(str,args)]
groups={
 'root':(['cp-battleship','solitaire'],[py('test/crossplay_ui_parity/run.py','--reference',C,'--output',O/'root','--apps-root',A,'--only-app','cp-battleship','--only-app','solitaire')]),
 'utility':(['cp-calculator','cp-notes','sudoku'],[py('test/utility_sudoku_original_ui/run.py','--work-dir',O/'utility','--runner',T/'capture-runner','--app-root',A)]),
 'chess':(['chess'],[py('test/chess_original_ui/run.py','--work-dir',O/'chess','--runner',T/'capture-runner','--app-dir',A/'chess')]),
 'social':(['cp-insider','cp-forehead'],[py('test/social_original_ui/run.py','--reference',C,'--native-source',H/'native-catalog-references','--tooling',T,'--output',O/'social','--apps-root',A)]),
 'murdle':(['cp-murdle'],[py('test/murdle_original_ui/build_native.py','--reference',C,'--native-source',H/'native-catalog-references','--output',O/'murdle-native'),py('test/murdle_original_ui/run.py','--tooling',T,'--native',O/'murdle-native/native-murdle','--output',O/'murdle','--app-root',A/'cp-murdle')]),
 'wordle':(['wordle'],[py('test/wordle_original_ui/run.py','--output',O/'wordle','--app-dir',A/'wordle','--tooling',T,'--native',N)]),
 'three-boards':(['cp-connectfour','cp-knucklebones','cp-checkers'],[py('test/three_original_boards/qualify.py','--output',O/'three-boards','--tooling',T,'--native',N,'--extra-native',H/'three-boards/native/native-three','--app-root',A)]),
 'four-boards':(['cp-go','cp-hex','cp-minesweeper','cp-yahtzee'],[py('test/four_original_boards/qualify.py','--output',O/'four-boards','--tooling',T,'--native',N,'--extra-native',H/'four-boards/native/native-four','--app-root',A)]),
 'hearts-jaipur':(['cp-hearts','cp-jaipur'],[py('test/hearts_jaipur_original_ui/all.py','--output',O/'hearts-jaipur','--app-root',A)]),
 'seasalt-toybattle':(['cp-seasalt','cp-toybattle'],[py('test/seasalt_toybattle_original_ui/all.py','--output',O/'seasalt-toybattle','--app-root',A)]),
 'wavelength':(['cp-wavelength'],[py('test/wavelength_original_ui/all.py','--output',O/'wavelength','--app-root',A)]),
 'trivia':(['cp-trivia'],[py('test/trivia_ui/run.py','--work-dir',O/'trivia','--app-dir',A/'cp-trivia')]),
 'hackernews':(['cp-hackernews'],[py('test/hackernews_original_ui/run.py','--work-dir',O/'hackernews','--app-dir',A/'cp-hackernews')])}
allids=[id for members,_ in groups.values()for id in members];assert len(allids)==24 and len(set(allids))==24 and set(allids)==ids
if a.only_group:assert set(a.only_group)<=groups.keys()
# Freeze the tested source and routine entry points before running anything.
def inputs():return{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest()for d in [ROOT/'apps',ROOT/'test',ROOT/'scripts/package_crossplay_ui.py']for f in([d]if d.is_file()else sorted(d.rglob('*')))if f.is_file()and '__pycache__'not in f.parts and '.pyc'!=f.suffix}
prior_report=json.loads((O/'results.json').read_text())if(O/'results.json').exists()else{}
frozen=inputs();fingerprint=hashlib.sha256(json.dumps(frozen,sort_keys=True).encode()).hexdigest();reports=[]
for name,(titles,commands)in groups.items():
 if a.only_group and name not in a.only_group:continue
 record=O/(name+'-qualification.json')
 if a.resume and record.exists():
  old=json.loads(record.read_text())
  reusable=old.get('input_sha256')==fingerprint and[run['command']for run in old.get('commands',[])]==commands
  delta=[]
  if not reusable and old.get('input_sha256')==prior_report.get('input_sha256')and old.get('passed'):
   before=prior_report.get('source_files',{});delta=[p for p in set(before)|set(frozen)if before.get(p)!=frozen.get(p)]
   own_tests={'root':'crossplay_ui_parity','utility':'utility_sudoku_original_ui','chess':'chess_original_ui','social':'social_original_ui','murdle':'murdle_original_ui','wordle':'wordle_original_ui','three-boards':'three_original_boards','four-boards':'four_original_boards','hearts-jaipur':'hearts_jaipur_original_ui','seasalt-toybattle':'seasalt_toybattle_original_ui','wavelength':'wavelength_original_ui','trivia':'trivia_ui','hackernews':'hackernews_original_ui'}
   def affects(path):
    if path=='test/crossplay_ui_parity/qualify_catalog.py':return False # orchestration only; exact commands checked below
    return any(path.startswith('apps/'+id+'/')for id in titles)or path.startswith('test/'+own_tests[name]+'/')or path.startswith(('test/crossplay_ui_parity/','test/crossplay_ports/'))
   reusable=bool(delta)and not any(affects(p)for p in delta)and[run['command']for run in old['commands']]==commands
  if reusable and old.get('passed'):
   if delta:
    old['execution_input_sha256']=old.get('execution_input_sha256',old['input_sha256']);old['unaffected_input_changes']=sorted(delta);old['reuse_reason']='Runtime app bytes, this group tests, shared renderer/VM tests and exact executed commands unchanged; only unrelated component test builder/orchestration changed.';old['input_sha256']=fingerprint;record.write_text(json.dumps(old,indent=2)+'\n')
   reports.append(old);print(name,'verified prior unchanged-dependency qualification',flush=True);continue
 log=O/(name+'.log');runs=[];print(name,'START',flush=True)
 with log.open('w')as output:
  for command in commands:
   start=time.monotonic();r=subprocess.run(command,cwd=ROOT,stdout=output,stderr=subprocess.STDOUT);runs.append({'command':command,'returncode':r.returncode,'seconds':round(time.monotonic()-start,3)})
   if r.returncode:break
 row=dict(group=name,titles=titles,commands=runs,passed=all(r['returncode']==0 for r in runs),input_sha256=fingerprint,log=str(log));record.write_text(json.dumps(row,indent=2)+'\n');reports.append(row);print(name,'PASS'if row['passed']else'FAIL',flush=True)
 if not row['passed']:break
assert inputs()==frozen,'Source changed during qualification'
qualified={title for row in reports if row['passed']for title in row['titles']};report=dict(scope='All24 local SD packages; actual production callback/pixel routines with explicit per-title deviations; no physical acceptance',input_sha256=fingerprint,source_files=frozen,groups=reports,qualified_titles=sorted(qualified),complete=qualified==ids and len(reports)==len(groups));(O/'results.json').write_text(json.dumps(report,indent=2)+'\n');assert all(r['passed']for r in reports)
if not a.only_group:assert report['complete'],'All24 qualification incomplete'
