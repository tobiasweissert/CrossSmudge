#!/usr/bin/env python3
from pathlib import Path
import argparse,re,subprocess,tempfile,json,hashlib
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
source=(root/'src/activities/network/AirDropReceiveActivity.cpp').read_text();m=re.search(r'void AirDropReceiveActivity::buildScreen\([^)]*\) \{',source);assert m
start=m.start();i=source.index('{',start);depth=1;j=i+1
while depth:
 if source[j]=='{':depth+=1
 if source[j]=='}':depth-=1
 j+=1
builder=source[start:j]
strings={}
for line in (root/'lib/I18n/translations/english.yaml').read_text().splitlines():
 m=re.match(r'(STR_AIRDROP_\w+|STR_CANCEL):\s*(".*")\s*$',line)
 if m:strings[m[1]]=json.loads(m[2])
keys=sorted(strings);translations='enum class StrId {'+','.join(keys)+'};\nstatic const char* translated(StrId id){static const char* strings[]={'+','.join(json.dumps(strings[k],ensure_ascii=False)for k in keys)+'};return strings[static_cast<unsigned>(id)];}\n#define tr(id) translated(StrId::id)\n'
with tempfile.TemporaryDirectory(prefix='airdrop-ui-render-')as td:
 td=Path(td);(td/'ui_render_extracted.inc').write_text(builder);(td/'ui_render_strings.inc').write_text(translations)
 cmd=['g++','-std=c++20','-O1','-g','-I'+str(root/'freeink-sdk/libs/ui/FreeInkUI/include'),'-I'+str(root/'src'),'-I'+str(root/'lib/AirDrop'),'-I'+str(td),str(root/'test/airdrop/ui_render.cpp'),str(root/'freeink-sdk/libs/ui/FreeInkUI/src/FreeInkUI.cpp'),'-o',str(td/'render')]
 subprocess.run(cmd,check=True);subprocess.run([str(td/'render'),str(a.output)],check=True)
 print(json.dumps({'production_sha256':hashlib.sha256(source.encode()).hexdigest(),'builder_sha256':hashlib.sha256(builder.encode()).hexdigest(),'renders':len(list(a.output.glob('*.svg'))),'target':'host SVG; declared monospace metrics; production content/SDK components; not physical UI/font/RF proof'}))
