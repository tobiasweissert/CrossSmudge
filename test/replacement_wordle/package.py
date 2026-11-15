#!/usr/bin/env python3
"""Build one deterministic candidate ZIP and verify its exact bytes with this PR's tests."""
import argparse, hashlib, json, os, pathlib, subprocess, tempfile, zipfile
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[1]
APP=HERE.name.removeprefix('replacement_')
p=argparse.ArgumentParser();p.add_argument('--output-dir',type=pathlib.Path,required=True);p.add_argument('--test',action='store_true');args=p.parse_args()
args.output_dir.mkdir(parents=True,exist_ok=True)
catalog=json.loads((ROOT/'apps/catalog.json').read_text());entries=catalog['apps'] if isinstance(catalog,dict) else catalog
matches=[e for e in entries if e['id']==APP];assert len(matches)==1
entry=matches[0];base=ROOT/'apps'/APP;manifest=json.loads((base/'manifest.json').read_text())
assert entry['version']==manifest['version'] and manifest['id']==APP
actual=sorted(str(f.relative_to(base)) for f in base.rglob('*') if f.is_file())
assert sorted(entry['files'])==actual and len(set(entry['files']))==len(actual)
for f in actual:assert not pathlib.PurePosixPath(f).is_absolute() and '..' not in pathlib.PurePosixPath(f).parts and not (base/f).is_symlink()
archive=args.output_dir/(APP+'-'+manifest['version']+'.zip')
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for f in actual:
  info=zipfile.ZipInfo(f,(2026,11,15,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16;z.writestr(info,(base/f).read_bytes())
with zipfile.ZipFile(archive) as z:
 assert z.testzip() is None and sorted(z.namelist())==actual
 for f in actual:assert z.read(f)==(base/f).read_bytes()
 records=[{'path':f,'bytes':len(z.read(f)),'sha256':hashlib.sha256(z.read(f)).hexdigest()} for f in actual]
report={'id':APP,'version':manifest['version'],'zip':archive.name,'bytes':archive.stat().st_size,'sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'files':records,'catalog_entry':entry,'tested_extracted_bytes':False}
if args.test:
 scratch=pathlib.Path(os.environ.get('TMPDIR','/root/.hermes/cache/scratch'));scratch.mkdir(parents=True,exist_ok=True)
 with tempfile.TemporaryDirectory(prefix=APP+'-zip-',dir=scratch) as temp:
  extracted=pathlib.Path(temp)/APP;extracted.mkdir()
  with zipfile.ZipFile(archive) as z:z.extractall(extracted)
  env=dict(os.environ,TMPDIR=str(scratch))
  log=args.output_dir/(APP+'-package-test.log')
  if APP=='chess':
   env['CHESS_APP_DIR']=str(extracted);env['REPLACEMENT_EVIDENCE']=str(args.output_dir);cmd=['python3',str(HERE/'run.py')]
  else:cmd=['python3',str(HERE/'run.py'),'--app-dir',str(extracted),'--output',str(args.output_dir/(APP+'-package-tests.json'))]
  with log.open('w') as out:subprocess.run(cmd,cwd=ROOT,env=env,stdout=out,stderr=subprocess.STDOUT,check=True)
  subprocess.run(['python3',str(HERE/'preservation.py'),'--app-dir',str(extracted),'--output',str(args.output_dir/(APP+'-preservation.json'))],cwd=ROOT,env=env,check=True)
 report['tested_extracted_bytes']=True
(args.output_dir/(APP+'-package.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ['files','catalog_entry']},indent=2))
