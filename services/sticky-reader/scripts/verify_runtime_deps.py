#!/usr/bin/env python3
"""Verify actual binary dependency module ZIP contents and extracted files against committed go.sum.
This checks compiled dependencies; it does not claim full historical/test module-graph coverage.
"""
import argparse,base64,hashlib,json,pathlib,subprocess,zipfile
p=argparse.ArgumentParser();p.add_argument('--binary',required=True);p.add_argument('--cache',required=True,type=pathlib.Path);p.add_argument('--sum',required=True,type=pathlib.Path);p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args()
sums={tuple(parts[:2]):parts[2] for line in a.sum.read_text().splitlines() if len(parts:=line.split())==3}
info=subprocess.check_output(['go','version','-m',a.binary],text=True);rows=[]
for line in info.splitlines():
 parts=line.split()
 if not parts or parts[0]!='dep':continue
 _,module,version,expected=parts
 assert sums[(module,version)]==expected
 def escaped(s):return ''.join('!'+c.lower() if c.isupper() else c for c in s)
 archive=a.cache/'cache/download'/escaped(module)/'@v'/(escaped(version)+'.zip');root=a.cache/(escaped(module)+'@'+escaped(version));whole=hashlib.sha256();count=0
 with zipfile.ZipFile(archive) as z:
  names=z.namelist();assert len(names)==len(set(names));prefix=module+'@'+version+'/'
  for name in sorted(names):
   assert name.startswith(prefix) and '..' not in pathlib.PurePosixPath(name).parts
   data=z.read(name);filehash=hashlib.sha256(data).hexdigest();whole.update((filehash+'  '+name+'\n').encode());local=root/name[len(prefix):];assert local.read_bytes()==data;count+=1
 actual='h1:'+base64.b64encode(whole.digest()).decode();assert actual==expected
 rows.append({'module':module,'version':version,'sum':actual,'files_checked':count,'zip_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'extracted_files_match':True})
assert len(rows)>=3
a.output.write_text(json.dumps({'scope':'all external modules embedded in actual executable; not full historical/test graph','dependencies':rows},indent=2)+'\n');print('verified',len(rows),'compiled modules: ZIP h1, extracted contents, committed go.sum and executable metadata')
