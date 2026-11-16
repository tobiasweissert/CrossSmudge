#!/usr/bin/env python3
"""Create the versioned Browser ZIP and verify the local catalog; never publish."""
import hashlib,json,pathlib,subprocess,sys,zipfile
root=pathlib.Path(__file__).resolve().parents[1]
out=pathlib.Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
folder=root/'apps/browser';manifest=json.loads((folder/'manifest.json').read_text())
assert manifest['id']=='browser' and manifest['min_api']==6 and manifest['browser'] is True
assert manifest['requires_psram'] is True and manifest['network']=={'domains':[]}
catalog=json.loads((root/'apps/catalog.json').read_text());entry=next(a for a in catalog['apps']if a['id']=='browser')
files=sorted(p for p in folder.iterdir()if p.is_file())
assert entry['files']==[p.name for p in files]
assert entry['version']==manifest['version'] and entry['min_api']==6
old=json.loads(subprocess.check_output(['git','show','HEAD:apps/catalog.json'],cwd=root,text=True))
assert [a for a in old['apps']if a['id']!='browser']==[a for a in catalog['apps']if a['id']!='browser']
assert (folder/'icon.raw').read_bytes()==subprocess.check_output(['git','show','673206080abbe678cbb69918807a1e18560b05d5:apps/browser/icon.raw'],cwd=root)
archive=out/('browser-'+manifest['version']+'.zip');inventory=[]
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_STORED)as z:
 for path in files:
  assert path.suffix not in ('.cache','.pyc')
  data=path.read_bytes();name='browser/'+path.name
  info=zipfile.ZipInfo(name,date_time=(2026,11,15,0,0,0));info.external_attr=0o100644<<16;z.writestr(info,data)
  inventory.append({'path':path.name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
with zipfile.ZipFile(archive)as z:
 assert z.testzip() is None and z.namelist()==['browser/'+p.name for p in files]
 for path in files:assert z.read('browser/'+path.name)==path.read_bytes()
result={'id':'browser','version':manifest['version'],'zip':archive.name,'zip_bytes':archive.stat().st_size,'zip_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'files':inventory,'other_catalog_entries_preserved':True,'icon_preserved':True}
(out/'browser-inventory.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items()if k!='files'},indent=2))
