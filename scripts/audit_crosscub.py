#!/usr/bin/env python3
"""Text-only board-reference, source-carry and exhaustive UI inventory.
No Git/SDK/cache writes. Run from any checkout; evidence is explicit output.
"""
from pathlib import Path
import argparse,collections,hashlib,json,re
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--baseline',type=Path);a=p.parse_args();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
EXCLUDE={'.git','.pio','__pycache__','node_modules'}
VENDOR=('freeink-sdk/','managed_components/','assets/tabler-icons/','lib/expat/','lib/miniz/','lib/Lua/','lib/AirDrop/vendor/')
GENERATED=('lib/I18n/I18nKeys.h','lib/I18n/I18nStrings.h','lib/I18n/I18nStrings.cpp')
BINARY={'.png','.jpg','.jpeg','.gif','.webp','.raw','.bin','.pdf','.epub','.zip','.woff','.woff2','.ttf','.otf','.ico'}
TOKEN=re.compile(r'\bx[34](?:[- _]?(?:pro|classic))?\b|(?:FREEINK|SIMULATOR)_DEVICE_X[34]\w*|deviceIsX[34]|XteinkX[34]\w*|CROSSINK_APP_DEVICE_X4CLASSIC|SMUDGE_X3_CONSTRAINTS|X4PRO_\w*',re.I)
records=[];files={};binary=[];symlinks=[]
for item in sorted(R.rglob('*')):
 rel=item.relative_to(R).as_posix()
 if any(x in EXCLUDE for x in item.relative_to(R).parts)or rel.startswith('docs/crosscub/'):continue
 if item.is_symlink():symlinks.append(rel);continue
 if not item.is_file():continue
 vendor=rel.startswith(VENDOR)
 data=item.read_bytes()
 if not vendor and '.generated.'not in item.name and rel not in GENERATED:
  files[rel]={'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)}
 if item.suffix.lower()in BINARY or b'\0'in data[:8192]:
  if TOKEN.search(rel):binary.append({'path':rel,'category':'binary/font/art payload; not a hardware text policy'})
  continue
 try:text=data.decode('utf-8')
 except UnicodeDecodeError:continue
 for line_no,line in enumerate(text.splitlines(),1):
  if not TOKEN.search(line):continue
  if vendor:category='immutable-third-party SDK/vendor internals'
  elif any(x in item.name.upper()for x in ['LICENSE','NOTICE','PROVENANCE']):category='mandatory license/attribution/provenance'
  elif rel.startswith('test/'):category='historical source-oracle/legacy-data or negative-admission fixture (not a build target)'
  elif rel in {'CHANGELOG.md','docs/MAC-HANDOFF.md','docs/crossplay-ports.md'}or item.name=='PARITY.md':category='inherited historical provenance; not current hardware guidance'
  elif 'SMUDGE_X3_CONSTRAINTS'in line:category='legacy low-memory stress environment alias; production budgets unchanged'
  elif rel=='include/AppCapabilities.h':category='negative compile-time rejection of retired hardware'
  elif rel in {'lib/GfxRenderer/BitmapHelpers.cpp','lib/GfxRenderer/BitmapHelpers.h'}:category='inherited grayscale calibration provenance; not a product target'
  elif rel.startswith('lib/Xtc/'):category='inherited XTC file-format provenance/resolution; not a current hardware target'
  elif rel=='src/CrossPointSettings.h':category='legacy serialized Home-key actions; retain enum values and fields'
  elif rel in {'src/network/FirmwareBoardTag.h','src/network/OtaBootSwitch.h','src/platform/skip_efuse_blk_check.c','src/util/SleepWakePolicy.h','lib/hal/HalDisplay.h','lib/GfxRenderer/GfxRenderer.h','lib/GfxRenderer/GfxRenderer.cpp','lib/hal/HalTiltSensor.cpp','src/util/BatteryDiagnosticLog.h'}:category='SDK/bootloader/diagnostic legacy behavior provenance; no selectable board'
  elif ('//' in line and not TOKEN.search(line.split('//',1)[0])) or line.lstrip().startswith(('//','*','--','# '))or(rel.startswith('.claude/')and 'Historical'in text):category='inherited implementation/provenance comment; no selectable board'
  elif re.search(r'\bx3\s*[,=)]|\by3\b|triangle\(',line):category='coordinate/algebra identifier; not hardware'
  else:category='owned residual compatibility/reference; inspect exact line'
  records.append({'path':rel,'line':line_no,'category':category,'text':line})
owned=[r for r in records if not r['path'].startswith(VENDOR)]
(O/'remaining-references.json').write_text(json.dumps({'scanner':'UTF-8 text only; known binary/font/art payloads excluded; exact original text retained','counts':dict(collections.Counter(r['category']for r in records)),'owned_counts':dict(collections.Counter(r['category']for r in owned)),'owned':owned,'third_party': [r for r in records if r['path'].startswith(VENDOR)],'binary_path_exceptions':binary},indent=2,ensure_ascii=False)+'\n')
if a.baseline:
 baseline=json.loads(a.baseline.read_text());baseline={k:v for k,v in baseline.items()if not k.startswith(VENDOR)and'.generated.'not in k and k not in GENERATED and not any(x in EXCLUDE for x in Path(k).parts)}
 changed={k:{'before':baseline[k],'after':v}for k,v in files.items()if k in baseline and baseline[k]['sha256']!=v['sha256']}
 new={k:v for k,v in files.items()if k not in baseline};deleted={k:v for k,v in baseline.items()if k not in files}
 carry={k:v for k,v in files.items()if k in baseline and baseline[k]['sha256']==v['sha256']}
 manifest={'baseline':'2a477dea03a6db5ed4b5ccd33772c04cf19f8767','excluded':'Git administration/dependency trees/ignored generated headers/node_modules/evidence; source docs/crosscub ledgers are self-excluded','counts':{'changed':len(changed),'new':len(new),'deleted':len(deleted),'carried':len(carry)},'changed':changed,'new':new,'deleted':deleted,'carried_sha256':hashlib.sha256(json.dumps(carry,sort_keys=True,separators=(',',':')).encode()).hexdigest(),'symlinks_preserved':symlinks}
 (O/'source-delta.json').write_text(json.dumps(manifest,indent=2)+'\n')
 (O/'source-carry.json').write_text(json.dumps(carry,sort_keys=True,indent=2)+'\n')
catalog=json.loads((R/'apps/catalog.json').read_text())
apps=[]
for app in catalog['apps']:
 runtime={str(f.relative_to(R)):{'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'bytes':f.stat().st_size}for f in sorted((R/'apps'/app['id']).rglob('*'))if f.is_file()and f.suffix!='.md'}
 apps.append({'id':app['id'],'name':app['name'],'version':app['version'],'entry':json.loads((R/'apps'/app['id']/'manifest.json').read_text()).get('entry','main.lua'),'runtime_files':runtime,'viewports':[[480,800],[800,480]],'scope':'see per-title measured test ledger; inventory alone is not runtime/visual acceptance'})
native=[]
for f in sorted((R/'src/activities').rglob('*')):
 if f.suffix not in ['.cpp','.h']:continue
 text=f.read_text()
 if re.search(r'\brender\s*\(|\bdraw\w*\s*\(',text):
  native.append({'path':str(f.relative_to(R)),'family':f.relative_to(R/'src/activities').parts[0],'sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'viewports':[[480,800],[800,480]],'scope':'source inventory; shared native bounds and named host render tests are separate from full target/physical screen acceptance'})
(O/'ui-inventory.json').write_text(json.dumps({'catalog_count':len(apps),'source_vs_live':'selected baseline contains 37 source apps; live owner store separately contains 41 (flashcards/habits/pomodoro/weather are live-only). Keep live owner URL; never publish source37 over live41','apps':apps,'native_screens':native},indent=2)+'\n')
print(json.dumps({'owned_reference_lines':len(owned),'all_reference_lines':len(records),'native_render_files':len(native),'catalog_apps':len(apps),'owned_categories':dict(collections.Counter(r['category']for r in owned))},indent=2))
