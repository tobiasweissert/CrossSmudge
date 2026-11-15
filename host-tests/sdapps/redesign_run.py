#!/usr/bin/env python3
"""Focused ABI2 regressions; renderer evidence is a builder/EpdFont harness."""
from pathlib import Path
import argparse,subprocess,os,json,hashlib
root=Path(__file__).resolve().parents[2];here=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
flags=['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
inc=['-I'+str(root/x) for x in ['src/apps_local/sdapps','freeink-sdk/libs/ui/FreeInkUI/include','lib/EpdFont','lib/Utf8']]
results=[]
def run(name,commands):
 out='';code=0
 for command in commands:
  command=list(map(str,command));r=subprocess.run(command,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});out+=r.stdout+r.stderr;code=r.returncode
  if code:break
 (a.output/(name+'.log')).write_text(out);results.append({'name':name,'exit':code,'commands':[list(map(str,c)) for c in commands]});print(name,code,flush=True)
 if code:print(out[-4000:])
def cpp(name,sources,args=()):
 exe=a.output/name;run(name,[['g++','-std=c++20','-Wall','-Wextra','-Wno-missing-field-initializers',*flags,*inc,*sources,'-o',exe],[exe,*args]])
cpp('package-v2',[here/'package_test.cpp',root/'src/apps_local/sdapps/Package.cpp'])
cpp('layout-v2',[here/'presentation_test.cpp',root/'src/apps_local/sdapps/services/BrowserCore.cpp'])
fonts=[root/'lib/EpdFont/EpdFont.cpp',root/'lib/Utf8/Utf8.cpp']
cpp('published-input',[here/'presentation_input_test.cpp',*fonts])
cpp('builder-raster',[here/'presentation_raster.cpp',*fonts],[a.output])
for name,app,entry in [('v2-chat','chatgpt','chat_entry'),('v2-browser','browser','browser_entry')]:
 obj=a.output/(name+'.o');exe=a.output/name
 test='v2_controller_test.cpp' if app=='chatgpt' else 'v2_browser_test.cpp'
 run(name,[['cc','-std=c11',*flags,*inc,'-Dapp_main='+entry,'-c',root/'sd-apps'/app/'app.c','-o',obj],['g++','-std=c++20',*flags,*inc,here/test,obj,'-o',exe],[exe]])
run('abi1-presenter',[['python3',here/'presenter_test.py','--output',a.output/'presenter']])
inputs={str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for d in [root/'src/apps_local/sdapps',root/'sd-apps',here] for f in d.rglob('*') if f.is_file() and '__pycache__' not in str(f)}
(a.output/'results.json').write_text(json.dumps({'results':results,'inputs':inputs,'limits':'Native ASan/UBSan. No Xtensa/device execution. Raster uses exact screen builder and EpdFont data, not GfxRenderer.'},indent=2)+'\n')
raise SystemExit(any(x['exit'] for x in results))
