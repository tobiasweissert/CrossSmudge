#!/usr/bin/env python3
"""Production capture/update, builder, paint, loop, decision and pinned SDK routing."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--sanitize',action='store_true');p.add_argument('--output',type=Path);a=p.parse_args();r=Path(__file__).resolve().parents[2]
def method(text,marker):
 start=text.index(marker);i=text.index('{',start);depth=1;j=i+1
 while depth:
  depth+=(text[j]=='{')-(text[j]=='}');j+=1
 return text[start:j]
source=(r/'src/activities/network/AirDropReceiveActivity.cpp').read_text()
names=['sameIdentity','buildScreen','decide','render','loop','beginLeaving','handleHomeGesture','deferGlobalPowerAction','action']
if 'bool AirDropReceiveActivity::routeConsentInput(' in source:names+=['routeConsentInput']
methods=[method(source,('bool'if n in ['sameIdentity','routeConsentInput','handleHomeGesture','deferGlobalPowerAction']else'void')+' AirDropReceiveActivity::'+n+'(')for n in names]
inputs=(r/'src/MappedInputManager.cpp').read_text();helpers=(r/'src/components/UiAppHelpers.h').read_text()
methods+=[method(inputs,'void MappedInputManager::update('),method(helpers,'inline freeink::ui::InputSnapshot touchSnapshotFrom(')]
strings={}
for line in(r/'lib/I18n/translations/english.yaml').read_text().splitlines():
 m=re.match(r'(STR_AIRDROP_\w+|STR_CANCEL):\s*(".*")\s*$',line)
 if m:strings[m[1]]=json.loads(m[2])
keys=sorted(strings);translations='enum class StrId {'+','.join(keys)+'};\nstatic const char* translated(StrId id){static const char* strings[]={'+','.join(json.dumps(strings[k],ensure_ascii=False)for k in keys)+'};return strings[static_cast<unsigned>(id)];}\n#define tr(id) translated(StrId::id)\n'
with tempfile.TemporaryDirectory(prefix='airdrop-consent-epoch-')as tmp:
 out=a.output or Path(tmp);out.mkdir(parents=True,exist_ok=True)
 # The helper must precede the production activity methods that call it.
 (out/'epoch_extracted.inc').write_text('\n'.join(methods[-2:]+methods[:-2]));(out/'ui_render_strings.inc').write_text(translations)
 flags=['-std=c++20','-O1','-g','-pthread','-DCROSSINK_APP_CAP_AIRDROP=1']
 if a.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
 cmd=['c++',*flags,'-Ifreeink-sdk/libs/ui/FreeInkUI/include','-Isrc','-Ilib/AirDrop','-I'+str(out),'test/airdrop/consent_epoch_test.cpp','freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp','-o',str(out/'probe')]
 results=[]
 for i,command in enumerate([cmd,[str(out/'probe')]]):
  result=subprocess.run(command,cwd=r,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'));(out/f'{i}.log').write_text(result.stdout+result.stderr);print(result.stdout+result.stderr,end='');results.append({'command':command,'exit':result.returncode})
  deps=['src/MappedInputManager.cpp','src/components/UiAppHelpers.h','src/activities/network/AirDropReceiveActivity.cpp','src/activities/network/AirDropReceiveActivity.h','freeink-sdk/libs/ui/FreeInkUI/include/FreeInkApp.h','freeink-sdk/libs/ui/FreeInkUI/include/FreeInkUICore.h']
  if(r/'src/components/AirDropConsentInput.h').exists():deps+=['src/components/AirDropConsentInput.h']
  (out/'receipt.json').write_text(json.dumps({'commands':results,'methods':names,'inputs':{x:hashlib.sha256((r/x).read_bytes()).hexdigest()for x in deps},'sanitize':a.sanitize,'seams':'synchronous GPIO frame, panel completion, receiver queue; real mutex/threads, production builder/update/render/loop/decide and pinned SDK interaction routing; no panel proof'},indent=2)+'\n')
  if result.returncode:raise SystemExit(1)
