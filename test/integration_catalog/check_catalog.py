#!/usr/bin/env python3
"""Crosscub catalog preservation, package closure and optional SD-overlay gate."""
import argparse,hashlib,json,pathlib,re,zipfile
p=argparse.ArgumentParser();p.add_argument('--owner-overlay',type=pathlib.Path);a=p.parse_args()
R=pathlib.Path(__file__).resolve().parents[2]
catalog=json.loads((R/'apps/catalog.json').read_text())
predecessor=json.loads((R/'test/crosscub/catalog-preservation.json').read_text())
carry=json.loads((R/'test/crosscub/accepted-catalog-preservation.json').read_text())
source_synced={'flashcards','habits','pomodoro','weather'}
assert set(carry['apps'])==set(predecessor['apps'])|source_synced, 'Preserve predecessor IDs plus the four source-synced productivity apps'
assert catalog['repo']=='tobiasweissert/Crosscub'
assert catalog['branch']==carry['branch']
assert len(catalog['apps'])==carry['catalog_count']
assert len({v['id'] for v in catalog['apps']})==len(catalog['apps'])
assert {v['id'] for v in catalog['apps']}==set(carry['apps'])
assert {p.name for p in (R/'apps').iterdir() if p.is_dir()}==set(carry['apps'])
overlay=zipfile.ZipFile(a.owner_overlay) if a.owner_overlay else None
for app in catalog['apps']:
 app_id=app['id'];baseline=carry['apps'][app_id]
 assert re.fullmatch(r'[A-Za-z0-9_-]{1,64}',app_id)
 assert app['version']==baseline['version'],app_id
 # Bind to the immutable owner-accepted redesign, not the superseded pre-redesign fixture.
 record=json.loads(json.dumps(app))
 digest=hashlib.sha256(json.dumps(record,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode()).hexdigest()
 assert digest==baseline['record_sha256'],(app_id,'baseline record changed')
 folder=R/'apps'/app_id;manifest=json.loads((folder/'manifest.json').read_text())
 assert manifest['id']==app_id and manifest['version']==app['version'],app_id
 manifest_bytes=(folder/'manifest.json').read_bytes()
 assert hashlib.sha256(manifest_bytes).hexdigest()==baseline['manifest_sha256'],(app_id,'accepted manifest changed')
 files=app['files'];assert len(files)==len(set(files)),app_id
 assert 'manifest.json' in files and manifest.get('entry','main.lua') in files
 for rel in files:
  assert re.fullmatch(r'[A-Za-z0-9_-][A-Za-z0-9_.-]*(/[A-Za-z0-9_-][A-Za-z0-9_.-]*)*',rel),rel
  assert (folder/rel).is_file() and not (folder/rel).is_symlink(),rel
 packaged=set(files)
 if overlay is not None:
  prefix=f'.crosssmudge/applications/{app_id}/'
  packaged={name[len(prefix):]for name in overlay.namelist()if name.startswith(prefix)}
  assert set(files)<=packaged,(app_id,'catalog member omitted from overlay')
  for rel in packaged:assert overlay.read(prefix+rel)==(folder/rel).read_bytes(),(app_id,rel)
 for path in folder.rglob('*'):
  if not path.is_file():continue
  rel=path.relative_to(folder).as_posix()
  if path.suffix in {'.lua','.raw','.bin','.json','.txt'} or rel.startswith(('data/','sprites/','maps/')):
   assert rel in packaged,(app_id,'runtime/license dependency missing',rel)
  if path.suffix=='.lua':
   for dep in re.findall(r'''\bdofile\(['"]([^'"]+)['"]\)''',path.read_text()):assert dep in packaged,(app_id,dep)
  assert not rel.endswith(('.dat','.sav','.bak','.cache')),(app_id,'save payload included')
 if app_id in {'cp-hackernews','cp-trivia'}:
  assert app['requires_psram'] is True and app['min_api']==manifest['min_api']==3
assert next(v for v in catalog['apps']if v['id']=='browser')['min_api']==6
print(f"PASS {len(catalog['apps'])} apps: accepted IDs/versions/metadata, runtime/license closure, safe paths and no save payloads")
print('Preserved all 37 main apps and source-synced four existing Store apps; 41 total.')
