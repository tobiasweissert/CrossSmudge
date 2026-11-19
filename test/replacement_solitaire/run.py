#!/usr/bin/env python3
"""Exact vendored Lua native adapter and independent CrossPlay source oracle.
No firmware, SDK, external Python modules or changes outside a temporary fixture.
"""
import argparse, hashlib, json, os, pathlib, shutil, subprocess, tempfile
ROOT = pathlib.Path(__file__).resolve().parents[2]
HERE = pathlib.Path(__file__).resolve().parent
APP = "solitaire"
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
    subprocess.run(['c++','-O2','-std=c++17','-I'+str(src),str(HERE/'oracle.cpp'),str(src/'SolitaireCore.cpp'),'-o',str(oracle)],check=True)
    fixture=work/'app';shutil.copytree(a.app_dir or ROOT/'apps'/APP,fixture)
    (fixture/'storage').mkdir();(fixture/'import').mkdir()
    with (fixture/'oracle.txt').open('w') as f:subprocess.run([str(oracle)],stdout=f,check=True)
    if not (fixture/'cp_core.lua').exists():
        r=subprocess.run([str(runner),str(HERE/'legacy_regression.lua'),'76800'],cwd=fixture,text=True,capture_output=True,timeout=10)
        records.append({'test':'existing_stock_touch_regression','returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
    for cap in [76800,2097152]:
        r=subprocess.run([str(runner),str(HERE/'oracle.lua'),str(cap)],cwd=fixture,text=True,capture_output=True,timeout=120)
        record={'test':'source_oracle','cap':cap,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr}
        records.append(record)
    if (fixture/'cp_core.lua').exists():
        for cap in [76800,2097152]:
            r=subprocess.run([str(runner),str(HERE/'rules.lua'),str(cap)],cwd=fixture,text=True,capture_output=True,timeout=120)
            records.append({'test':'rules','cap':cap,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
    if (fixture/'cp_core.lua').exists():
        for script in ['launch.lua','capacity.lua','integration.lua','journal.lua','touch.lua']:
            for cap in [76800,2097152]:
                if script in ['integration.lua','touch.lua'] and cap==76800:continue # Full test bytecode/second core load exceed hardware budget; lean capacity.lua tests real UI.
                for width,height in [(480,800),(800,480)]:
                    for file in (fixture/'storage').iterdir():file.unlink()
                    r=subprocess.run([str(runner),str(HERE/script),str(cap),str(width),str(height)],cwd=fixture,text=True,capture_output=True,timeout=120)
                    records.append({'test':script,'cap':cap,'width':width,'height':height,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
    if (fixture/'cp_core.lua').exists():
        for width,height in [(480,800),(800,480)]:
            for file in (fixture/'storage').iterdir():file.unlink()
            r=subprocess.run([str(runner),str(HERE/'render.lua'),'2097152',str(width),str(height)],cwd=fixture,text=True,capture_output=True,timeout=120)
            records.append({'test':'adapter_render','width':width,'height':height,'returncode':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
            if a.output and r.returncode==0:shutil.copyfile(fixture/'render.svg',a.output.parent/(APP+'-adapter-'+str(width)+'x'+str(height)+'.svg'))
    for row in records:
        if row['returncode']==0:
            metrics=json.loads(row['stdout'].strip().splitlines()[-1]);row['metrics']=metrics
            if metrics.get('after_close')!=0:row['returncode']=1;row['stderr']+='VM allocation retained after close'
    result={'adapter':'native test adapter, not firmware rendering or hardware timing','lua_config_sha256':hashlib.sha256((ROOT/'lib/Lua/src/luaconf.h').read_bytes()).hexdigest(),'source_sha256':hashlib.sha256((src/'SolitaireCore.cpp').read_bytes()).hexdigest(),'tests':records}
    if a.output:a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
    raise SystemExit(0 if all(r['returncode']==0 for r in records) else 1)
