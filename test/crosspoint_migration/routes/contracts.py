#!/usr/bin/env python3
"""All 51 owned sources: source-marker/translation/API structural gates only."""
from pathlib import Path
import json, re, argparse, hashlib
R=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();a.evidence.mkdir(parents=True,exist_ok=True)
pairs={'boot_sleep':['Sleep'],'browser':['OpdsBookBrowser'],'home':['FileBrowser','Home','RecentBooks'],'settings':['ButtonRemap','ClockOffset','FontDownload','FontSelection','KOReaderSettings','KeyboardLayouts','LanguageSelect','OpdsServerList','OpdsSettings','OtaUpdate','Settings','StatusBarSettings'],'util':['BmpViewer','Confirmation','FrontlightPanel','IntervalSelection','KeyboardEntry']}
paths=[f'src/activities/{folder}/{stem}Activity.{ext}' for folder,stems in pairs.items() for stem in stems for ext in ['h','cpp']]
paths+=['src/activities/home/CrashActivity.cpp']+[f'src/activities/settings/{stem}Activity.cpp' for stem in ['ClearCache','ClockSync','KOReaderAuth','SdFirmwareUpdate']]+['src/activities/util/KeyboardLayoutSet.h','src/activities/util/KeyboardLayoutSet.cpp']
assert len(paths)==len(set(paths))==51
keys=set(re.findall(r'^STR_[A-Z0-9_]+',(R/'lib/I18n/translations/english.yaml').read_text(),re.M))
issues=[]
for path in paths:
 p=R/path
 if not p.exists():issues.append([path,'missing']);continue
 t=p.read_text()
 if re.search(r'^(<<<<<<<|=======|>>>>>>>)',t,re.M):issues.append([path,'conflict marker'])
 missing=sorted(set(re.findall(r'\bSTR_[A-Z0-9_]+',t))-keys)
 if missing:issues.append([path,'unknown translations',missing])
 if re.search(r'UiAppHost<',t):issues.append([path,'obsolete templated UiAppHost'])
opds=(R/'src/activities/browser/OpdsBookBrowserActivity.cpp').read_text()
assert 'STR_OVERWRITE' not in opds and 'PUBLICATION_UNCERTAIN' in opds and 'server.filenameFormat' in opds
assert opds.count('if (!admitNewDownload(filename)) return;')==2
assert 'CatalogActivity::onExit()' not in opds
bars=(R/'src/activities/settings/StatusBarSettingsActivity.cpp').read_text()
assert 'viewInputGate.allowInput(mappedInput)' in bars and 'viewInputGate.endPaint()' in bars
(a.evidence/'contracts-results.json').write_text(json.dumps({'paths':sorted(paths),'count':len(paths),'issues':issues,'qualification':'source structure only',
 'hashes':{path:hashlib.sha256((R/path).read_bytes()).hexdigest() for path in paths if (R/path).exists()}},indent=2)+'\n')
assert not issues,issues
print('PASS all 51 owned paths present/marker-free, known translation IDs, shared-host spelling, OPDS no-overwrite/uncertainty/per-server naming gates and status view-fence wiring (structural only)')
