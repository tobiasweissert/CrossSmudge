"""Package the reviewed app and an allowlisted standalone patch; no publish/staging."""
from pathlib import Path
import hashlib,json,re,subprocess,sys,zipfile
out=Path(sys.argv[1]);root=Path.cwd()
assert (out/'REVIEW-PASS.txt').exists(), 'independent frozen review must pass first'
base='cc0e2248da546c768122ed1ba71d60474208d1a8'
old=subprocess.check_output(['git','show',base+':apps/catalog.json']).decode()
new=Path('apps/catalog.json').read_text()
pattern=r'(?ms)^    \{\n      "id": "2048".*?^    \}'
a=re.search(pattern,old);b=re.search(pattern,new)
assert a and b and old[:a.start()]+b.group()+old[a.end():]==new,'other catalog bytes changed'
entry=json.loads(b.group());manifest=json.loads(Path('apps/2048/manifest.json').read_text())
assert entry['files']==manifest['files'] and entry['version']==manifest['version']=='1.1.0'
assert Path('apps/2048/legacy_main.lua').read_bytes()==subprocess.check_output(['git','show',base+':apps/2048/main.lua'])
files=sorted([*Path('apps/2048').glob('*'),*Path('test/2048_design').glob('*'),Path('docs/2048-reference-design.md')])
files=[p for p in files if p.is_file()]
tracked=set(subprocess.check_output(['git','ls-files']).decode().splitlines())
changed=subprocess.check_output(['git','diff','--name-only']).decode().splitlines()
assert all(p.startswith('apps/2048/') or p=='apps/catalog.json' for p in changed),changed
patch=subprocess.check_output(['git','diff','--binary','--','apps/2048','apps/catalog.json'])
for p in files:
 if str(p) not in tracked:
  result=subprocess.run(['git','diff','--no-index','--binary','--','/dev/null',str(p)],capture_output=True)
  assert result.returncode==1
  patch+=result.stdout
(out/'2048-reference-design.patch').write_bytes(patch)
(out/'catalog-entry-2048.json').write_text(json.dumps(entry,indent=2)+'\n')
(out/'copy-allowlist.txt').write_text('\n'.join(map(str,files))+'\n# apps/catalog.json: replace record id=2048 only, never copy whole file\n')
sha=lambda data: hashlib.sha256(data).hexdigest()
(out/'source-sha256.json').write_text(json.dumps({str(p):sha(p.read_bytes()) for p in files},indent=2)+'\n')
with zipfile.ZipFile(out/'2048-1.1.0-test.zip','w',compression=zipfile.ZIP_DEFLATED) as z:
 for name in manifest['files']:
  assert '/' not in name and not name.endswith('.dat')
  info=zipfile.ZipInfo('2048/'+name,(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
  z.writestr(info,(Path('apps/2048')/name).read_bytes())
(out/'artifacts-sha256.json').write_text(json.dumps({p.name:sha(p.read_bytes()) for p in out.iterdir() if p.suffix in ('.png','.trace','.zip','.patch')},indent=2)+'\n')
print('Package, catalog-only delta, allowlist, source/artifact hashes, and standalone patch written.')
