#!/usr/bin/env python3
"""Run unchanged accepted repair assertions with a bounded asset-read adapter."""
from pathlib import Path
import subprocess,json,argparse,shutil,hashlib
R=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--work-dir',type=Path,required=True);p.add_argument('--runner',type=Path,required=True);p.add_argument('--app-dir',type=Path,default=R/'apps/cp-hackernews');a=p.parse_args()
a.work_dir.mkdir(parents=True,exist_ok=True)
root=a.work_dir/'root';app=root/'apps/cp-hackernews';shutil.rmtree(root,ignore_errors=True);app.parent.mkdir(parents=True);shutil.copytree(a.app_dir,app)
results=[]
for source in [R/'test/network_apps/test.lua',R/'test/repair_round1/hn_memory.lua',R/'test/repair_round1/hn_cursor.lua']:
 text=source.read_text()
 old="local f=assert(io.open(app..path,'rb'));local data=f:read(n);f:close();return data"
 assert old in text
 text="local nativeAssetRead=smudge.read_file\n"+text.replace(old,"return nativeAssetRead(app..path,n)")
 script=a.work_dir/source.name;script.write_text(text)
 t=subprocess.run([str(a.runner),str(script),'2097152','480','800'],cwd=root,capture_output=True,text=True,timeout=180)
 row=dict(test=str(source.relative_to(R)),original_test_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),adaptation='asset read only replaces fixture io.open; assertions unchanged; missing identity uses exact legacy presentation',returncode=t.returncode,stdout=t.stdout,stderr=t.stderr)
 results.append(row);print(source.name,t.returncode,t.stdout,t.stderr,flush=True)
(a.work_dir/'results.json').write_text(json.dumps(results,indent=2)+'\n')
assert all(t['returncode']==0 for t in results)
