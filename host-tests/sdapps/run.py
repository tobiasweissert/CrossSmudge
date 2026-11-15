#!/usr/bin/env python3
"""Focused SD app/ported-service regression tests; no live network or target execution."""
import argparse, hashlib, json, os, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
SRC=ROOT/'src/apps_local/sdapps'
SERV=SRC/'services'
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--modules',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
flags=['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
results=[]
def run(name,commands):
 log='';code=0
 for cmd in commands:
  r=subprocess.run([str(x) for x in cmd],text=True,capture_output=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  log+=r.stdout+r.stderr;code=r.returncode
  if code:break
 (a.output/(name+'.log')).write_text(log)
 results.append({'name':name,'exit':code,'commands':[[str(x) for x in c] for c in commands]})
 print(name,code,flush=True)
 if code:print(log[-4000:],flush=True)
def cpp(name,sources,extra=(),args=()):
 exe=a.output/name
 run(name,[['g++','-std=c++20','-Wall','-Wextra','-Werror',*flags,'-I'+str(SRC),'-I'+str(SERV),*extra,*sources,'-o',exe],[exe,*args]])
cpp('package',[HERE/'package_test.cpp',SRC/'Package.cpp'])
cpp('image',[HERE/'image_test.cpp',SRC/'ImageBudget.cpp'],args=[a.modules/'apps'/app/'app.elf' for app in ['browser','chatgpt']])
with tempfile.TemporaryDirectory() as tmp:
 cpp('catalog',[HERE/'catalog_test.cpp',SRC/'Catalog.cpp',SRC/'Package.cpp'],['-I'+str(HERE/'catalog-fakes'),'-I'+str(ROOT/'lib/Memory')],[tmp])
for app,entry in [('browser','browser_entry'),('chatgpt','chat_entry')]:
 obj=a.output/(app+'.o');exe=a.output/app
 test='chat' if app=='chatgpt' else app
 run(app,[['cc','-std=c11','-Wall','-Wextra','-Werror',*flags,'-I'+str(SRC),'-Dapp_main='+entry,'-c',ROOT/'sd-apps'/app/'app.c','-o',obj],
          ['g++','-std=c++20',*flags,'-I'+str(SRC),HERE/(test+'_test.cpp'),obj,'-o',exe],[exe]])
cases={'policy':['BrowserPolicy'],'http':['BrowserHttp'],'cache':['BrowserCache','BrowserPolicy'],'fetch':['BrowserFetch','BrowserPolicy','BrowserHttp'],'core_tests':['BrowserCore'],'stress_tests':['BrowserCore']}
run('controller-presenter', [['python3', HERE/'presenter_test.py', '--output', a.output/'presenter']])
for name,sources in cases.items():
 test=HERE/'services/browser'/(name+'.cpp' if name.endswith('tests') else name+'_test.cpp')
 extra=['-DFREEINK_DEVICE_STICKY=1','-I'+str(HERE/'services/browser/fakes')] if name=='fetch' else []
 cpp('browser-'+name,[test,*[SERV/(s+'.cpp') for s in sources]],extra)
for name,sources in {'capture':['ChatCapture'],'protocol':['ChatProtocol'],'post':['ChatPost','ChatProtocol'],'provider':['ChatProtocol','ChatCore'],'key':['ProviderKey','ChatProtocol'],'layout':[]}.items():
 cpp('chat-'+name,[HERE/'services/chat'/(name+'_test.cpp'),*[SERV/(s+'.cpp') for s in sources]],['-DFREEINK_DEVICE_STICKY=1','-I'+str(HERE/'services/chat/fakes')])
cpp('pdm',[HERE/'pdm_test.cpp',SERV/'PdmMicrophone.cpp',ROOT/'lib/hal/HalMicrophone.cpp'],['-DFREEINK_DEVICE_STICKY=1','-I'+str(HERE/'services/chat/fakes'),'-I'+str(HERE/'catalog-fakes'),'-I'+str(ROOT/'lib/hal')])
run('shelf',[['python3',HERE/'test_shelf.py']])
# Host loader relocates actual target instructions but never executes Xtensa code.
exe=a.output/'loader-probe'
run('loader',[['cc','-std=c11','-D_GNU_SOURCE','-g','-fsanitize=undefined','-fno-sanitize-recover=all','-Wno-pointer-to-int-cast','-Wno-int-to-pointer-cast','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DCONFIG_IDF_TARGET_ARCH_XTENSA=1','-DCONFIG_ELF_LOADER_BUS_ADDRESS_MIRROR=1','-I'+str(HERE/'loader-stubs'),'-I'+str(ROOT/'lib/ElfLoader/include'),HERE/'loader_probe.c',ROOT/'lib/ElfLoader/src/arch/esp_elf_xtensa.c',ROOT/'lib/ElfLoader/src/esp_elf_validate.c','-o',exe],*[ [exe,a.modules/'apps'/app/'app.elf'] for app in ['browser','chatgpt']]])
inputs={str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for d in [SRC,HERE,ROOT/'sd-apps',ROOT/'lib/ElfLoader'] for f in d.rglob('*') if f.is_file() and '__pycache__' not in str(f)}
(a.output/'results.json').write_text(json.dumps({'results':results,'inputs':inputs,'limits':'ASan/UBSan; LeakSanitizer disabled (sandbox ptrace). DNS/TLS/I2S are fakes. Target ELF relocated, not executed.'},indent=2)+'\n')
raise SystemExit(any(r['exit'] for r in results))
