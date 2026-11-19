#!/usr/bin/env python3
"""Exact vendored Lua native adapter and independent CrossPlay source oracle.
No firmware, SDK, external Python modules or changes outside a temporary fixture.
"""
import argparse, hashlib, json, os, pathlib, shutil, subprocess, tempfile
ROOT = pathlib.Path(__file__).resolve().parents[2]
HERE = pathlib.Path(__file__).resolve().parent
APP = "wordle"
p = argparse.ArgumentParser()
p.add_argument('--source',type=pathlib.Path,default=HERE/'upstream')
p.add_argument('--output',type=pathlib.Path)
p.add_argument('--app-dir',type=pathlib.Path,help='Extracted candidate ZIP app root; defaults to repository app')
a = p.parse_args()
os.environ.setdefault('TMPDIR','/root/.hermes/cache/scratch')
records=[]
with tempfile.TemporaryDirectory(prefix=APP+'-replacement-',dir=os.environ['TMPDIR']) as temp:
    work=pathlib.Path(temp)
    runner=work/'runner'
    subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),str(HERE/'runner.c'),*map(str, sorted((ROOT/'lib/Lua/src').glob('*.c'))),'-lm','-o',str(runner)],check=True)
    src=a.source/'src/apps_local'/APP
    oracle=work/'oracle'
    subprocess.run(['c++','-O2','-std=c++17','-I'+str(src),str(HERE/'oracle.cpp'),str(src/'WordleCore.cpp'),'-o',str(oracle)],check=True)
    fixture=work/'app';shutil.copytree(a.app_dir or ROOT/'apps'/APP,fixture)
    (fixture/'storage').mkdir();(fixture/'import').mkdir()
    with (fixture/'oracle.txt').open('w') as f:subprocess.run([str(oracle)],stdout=f,check=True)
    if not (fixture/'cp_core.lua').exists():
        r=subprocess.run([str(runner),str(HERE/'legacy_regression.lua'),'76800'],cwd=fixture,text=True,capture_output=True,timeout=10)
        records.append({'test':'existing_invalid_word_regression','returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
    for cap in [76800,2097152]:
        r=subprocess.run([str(runner),str(HERE/'oracle.lua'),str(cap)],cwd=fixture,text=True,capture_output=True,timeout=120)
        record={'test':'source_oracle','cap':cap,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr}
        records.append(record)
    if (fixture/'cp_core.lua').exists():
        for script in ['launch.lua','capacity.lua','daily_capacity.lua','integration.lua','durability.lua','full_capacity.lua','full_load_capacity.lua','span_capacity.lua','span_limit_capacity.lua','journal.lua']:
            for cap in [76800,2097152]:
                if script in ['integration.lua','durability.lua','full_capacity.lua'] and cap==76800:continue
                for width,height in [(480,800),(800,480)]:
                    for file in (fixture/'storage').iterdir():file.unlink()
                    for name in ['wordle.ans','wordle.words']:(fixture/name).unlink(missing_ok=True)
                    if script in ['full_load_capacity.lua','span_capacity.lua','span_limit_capacity.lua']:
                        payload=(b'908\n'+bytes([130])*1024) if script=='span_limit_capacity.lua' else b'0\n'+(bytes([130])*8192 if script=='full_load_capacity.lua' else bytes([129]))
                        av,bv=1,0
                        for byte in b'1 '+payload:av=(av+byte)%65521;bv=(bv+av)%65521
                        (fixture/'storage'/'crossplay-results-2.dat').write_bytes(('CP1 1 '+str(len(payload))+' '+str(av)+':'+str(bv)+'\n').encode()+payload)
                    if script in ['daily_capacity.lua','span_capacity.lua','span_limit_capacity.lua']:
                        (fixture/'wordle.ans').write_bytes(b'.'*(1931*5)+b'APPLE')
                        (fixture/'wordle.words').write_bytes(b'APPLECRANE')
                    r=subprocess.run([str(runner),str(HERE/script),str(cap),str(width),str(height)],cwd=fixture,text=True,capture_output=True,timeout=120)
                    if script in ['full_load_capacity.lua','span_capacity.lua','span_limit_capacity.lua'] and (fixture/'storage'/'crossplay-results-2.dat').read_bytes()!=('CP1 1 '+str(len(payload))+' '+str(av)+':'+str(bv)+'\n').encode()+payload:
                        r.returncode=1;r.stderr+='History guard altered preserved results journal'
                    records.append({'test':script,'cap':cap,'width':width,'height':height,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
        for width,height in [(480,800),(800,480)]:
            for file in (fixture/'storage').iterdir():file.unlink()
            r=subprocess.run([str(runner),str(HERE/'render.lua'),'2097152',str(width),str(height)],cwd=fixture,text=True,capture_output=True,timeout=120)
            records.append({'test':'adapter_render','width':width,'height':height,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
            if a.output and r.returncode==0:shutil.copyfile(fixture/'render.svg',a.output.parent/(APP+'-adapter-'+str(width)+'x'+str(height)+'.svg'))
    for row in records:
        if row['returncode']==0:
            metrics=json.loads(row['stdout'].strip().splitlines()[-1]);row['metrics']=metrics
            if metrics.get('after_close')!=0:row['returncode']=1;row['stderr']+='VM allocation retained after close'
    result={'adapter':'native test adapter, not firmware rendering or hardware timing','lua_config_sha256':hashlib.sha256((ROOT/'lib/Lua/src/luaconf.h').read_bytes()).hexdigest(),'source_sha256':hashlib.sha256((src/'WordleCore.cpp').read_bytes()).hexdigest(),'tests':records}
    if a.output:a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    raise SystemExit(0 if all(r['returncode']==0 for r in records) else 1)
