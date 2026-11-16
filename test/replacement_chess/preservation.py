#!/usr/bin/env python3
"""Compare delivered legacy bytes/callback traces against pinned git source, with separate 75 KiB observations."""
import hashlib,json,os,pathlib,subprocess,tempfile
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[1]
BASE='2c82f306ff504c886843421846e4ff3b0c8e315c'
APP=HERE.name.removeprefix('replacement_')
def verify(app,runner,work):
    baseline=work/'preservation-base';baseline.mkdir()
    paths=subprocess.check_output(['git','ls-tree','-r','--name-only',BASE,'apps/'+APP],cwd=ROOT,text=True).splitlines()
    hashes={}
    for path in paths:
        name=path.split('/',2)[2]
        if name=='manifest.json':continue
        data=subprocess.check_output(['git','show',BASE+':'+path],cwd=ROOT)
        target='legacy_main.lua' if name=='main.lua' else name
        assert (app/target).read_bytes()==data, 'legacy bytes changed: '+target
        (baseline/name).parent.mkdir(parents=True,exist_ok=True);(baseline/name).write_bytes(data)
        hashes[target]=hashlib.sha256(data).hexdigest()
    constrained=[]
    for width,height in [(480,800),(800,480)]:
        for device in ['x3','x4','x4pro','simulator','unknown','missing']:
            pair=[];traces=[]
            for directory in [baseline,app]:
                p=subprocess.run([str(runner),str(HERE/'preservation.lua'),'76800',str(width),str(height)],cwd=directory,env=dict(os.environ,PRESERVE_DEVICE=device),text=True,capture_output=True,timeout=60)
                lines=p.stdout.splitlines();metrics=json.loads(lines[-1]);assert metrics['after_close']==0
                traces.append(lines[:-1])
                pair.append(dict(candidate=directory==app,returncode=p.returncode,error=p.stderr,metrics=metrics,trace_lines=len(lines)-1,last_trace_line=lines[-2] if len(lines)>1 else None,trace_sha256=hashlib.sha256('\n'.join(lines[:-1]).encode()).hexdigest()))
            assert pair[0]['returncode']==pair[1]['returncode'], '75 KiB regression versus baseline: '+str(pair)
            assert traces[0]==traces[1], '75 KiB callback/failure progress differs: '+str(pair)
            if pair[0]['returncode']:
                assert 'not enough memory' in pair[0]['error'] and 'not enough memory' in pair[1]['error']
            constrained.append(dict(device=device,width=width,height=height,runs=pair))
    records=[]
    for width,height in [(480,800),(800,480)]:
        for device in ['x3','x4','x4pro','simulator','unknown','missing']:
            traces=[]
            for directory in [baseline,app]:
                env=dict(os.environ,PRESERVE_DEVICE=device)
                p=subprocess.run([str(runner),str(HERE/'preservation.lua'),'2097152',str(width),str(height)],cwd=directory,env=env,text=True,capture_output=True,timeout=60)
                assert p.returncode==0,str(directory)+': '+p.stderr+'\n'+p.stdout[-1500:]
                lines=p.stdout.splitlines();metrics=json.loads(lines[-1]);assert metrics['after_close']==0
                traces.append(lines[:-1])
            assert traces[0]==traces[1], (device,width,'legacy callbacks/save state differ')
            records.append(dict(device=device,width=width,height=height,trace_sha256=hashlib.sha256('\n'.join(traces[0]).encode()).hexdigest()))
    return dict(base=BASE,legacy_sha256=hashes,traces=records,trace_cap=2097152,constrained_observations=constrained)
if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--app-dir',type=pathlib.Path,default=ROOT/'apps'/APP);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
    with tempfile.TemporaryDirectory(prefix=APP+'-preserve-',dir=os.environ.get('TMPDIR','/root/.hermes/cache/scratch')) as tmp:
        work=pathlib.Path(tmp);runner=work/'runner'
        subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),str(HERE/'runner.c'),*map(str,sorted((ROOT/'lib/Lua/src').glob('*.c'))),'-lm','-o',str(runner)],check=True)
        a.output.write_text(json.dumps(verify(a.app_dir,runner,work),indent=2)+'\n')
