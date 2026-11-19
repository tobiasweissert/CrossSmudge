#!/usr/bin/env python3
"""Compile full Library activity bodies against pinned real SDK UI headers.
Only external includes are replaced by explicit HAL/application doubles.
Not a firmware link, real-font render, or physical touch qualification.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, subprocess
p=argparse.ArgumentParser();p.add_argument('--evidence',required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;root=here.parents[2];out=Path(a.evidence);out.mkdir(parents=True,exist_ok=True)
build=Path(os.environ.get('TMPDIR','/root/.hermes/cache/scratch'))/'library-finish-ui';build.mkdir(exist_ok=True)
keys=set(re.findall(r'^\s*(STR_\w+):',(root/'lib/I18n/translations/english.yaml').read_text(),re.M))
match=re.search(r'enum class FileBrowserAction.*?\};',(root/'src/activities/home/FileBrowserActionActivity.h').read_text(),re.S)
assert match is not None
actions=match.group()
(build/'ui_schema.h').write_text('#pragma once\nenum class StrId{'+','.join(sorted(keys))+'};\nenum class Language{EnglishEn,ArabicAr,HebrewHe};\n'+actions)
def body(path):
 s=path.read_text();return '\n'.join(x for x in s.splitlines() if not x.startswith('#include') and x!='#pragma once')+'\n'
headers='\n'.join(body(root/'src/activities/library'/f) for f in ['LibraryActivity.h','LibrarySettingsActivity.h','LibraryListActivity.h'])
actual_settings=(root/'src/CrossPointSettings.h').read_text()
used_settings=set(re.findall(r'SETTINGS\.(\w+)',headers+'\n'+(root/'src/activities/library/LibraryActivity.cpp').read_text()+'\n'+(root/'src/activities/library/LibrarySettingsActivity.cpp').read_text()))
assert all(re.search(r'\b'+re.escape(field)+r'\b',actual_settings) for field in used_settings), 'Unimplemented settings field/API'
book_api=(root/'src/activities/home/BookActions.h').read_text()
used_book_api=set(re.findall(r'BookActions::(\w+)',(root/'src/activities/library/LibraryActivity.cpp').read_text()))
assert all(re.search(r'\b'+re.escape(name)+r'\b',book_api) for name in used_book_api), 'Missing shared BookActions API'
(out/'shared-abi.json').write_text(json.dumps({str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'src/CrossPointSettings.h',root/'src/activities/home/BookActions.h',root/'src/activities/home/BookActions.cpp',root/'src/util/ButtonNavigator.h',root/'src/components/OptionPopup.h']},indent=2))
(build/'ui_classes.h').write_text(headers)
receipts=[]
for name in ['LibraryActivity','LibrarySettingsActivity','LibraryListActivity']:
 path=root/'src/activities/library'/(name+'.cpp')
 text='#include "boundaries.h"\n#include "components/icons/libraryIcons.h"\n#include "components/icons/listIcons.h"\n'+headers+body(path)
 src=build/(name+'.cpp');src.write_text(text)
 cmd=['g++','-std=c++20','-fsyntax-only','-DSIMULATOR','-I'+str(here/'ui_stubs'),'-I'+str(build),'-I'+str(here/'library_builder/stubs'),'-I'+str(build.parent/'library-finish-sdk/libs/ui/FreeInkUI/include'),'-I'+str(root/'lib/LibraryIndex'),'-I'+str(root/'lib/Utf8'),'-I'+str(root/'lib/DateFormatting'),'-I'+str(root/'src/activities/library'),'-I'+str(root/'src'),str(src)]
 r=subprocess.run(cmd,capture_output=True,text=True,timeout=120);(out/(name+'.log')).write_text(r.stdout+r.stderr);receipts.append({'name':name,'exit':r.returncode,'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'argv':cmd});(out/'receipt.json').write_text(json.dumps(receipts,indent=2))
 if r.returncode:print(r.stderr[:24000]);raise SystemExit(r.returncode)
 print(name,'syntax PASS (real SDK UI / explicit application doubles)')
