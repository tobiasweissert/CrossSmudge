#!/usr/bin/env python3
import argparse,json,os,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--evidence',required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parents[2];out=Path(a.evidence);out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(here/'ui_syntax.py'),'--evidence',str(out/'syntax')],check=True,timeout=120)
build=Path(os.environ.get('TMPDIR','/root/.hermes/cache/scratch'))/'library-finish-ui'
cmd=['g++','-std=c++20','-O1','-g0','-DSIMULATOR','-I'+str(here/'ui_stubs'),'-I'+str(build),'-I'+str(here/'library_builder/stubs'),'-I'+str(build.parent/'library-finish-sdk/libs/ui/FreeInkUI/include'),'-I'+str(root/'lib/LibraryIndex'),'-I'+str(root/'lib/Utf8'),'-I'+str(root/'lib/Serialization'),'-I'+str(root/'src/activities/library'),'-I'+str(root/'src')]
if a.sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
cmd+=[str(here/'ui_data.cpp')]+[str(build/(n+'.cpp')) for n in ['LibraryActivity','LibrarySettingsActivity','LibraryListActivity']]+[str(build.parent/'library-finish-sdk/FreeInkUI.cpp')]+[str(root/'lib/LibraryIndex'/(n+'.cpp')) for n in ['LibraryBuilder','LibraryIndexFile','LibraryMetadataCache','LibrarySort','LibraryFormat','LibraryText']]+[str(root/'lib/Utf8/Utf8.cpp'),'-lexpat','-o',str(build/'ui-data')]
receipt=[]
for name,command in [('compile',cmd),('run',[str(build/'ui-data')])]:
 r=subprocess.run(command,capture_output=True,text=True,timeout=120,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/(name+'.log')).write_text(r.stdout+r.stderr);receipt.append({'name':name,'exit':r.returncode,'argv':command});(out/'receipt.json').write_text(json.dumps(receipt,indent=2));print(name,r.returncode,(r.stdout+r.stderr)[-12000:]);
 if r.returncode:raise SystemExit(1)
