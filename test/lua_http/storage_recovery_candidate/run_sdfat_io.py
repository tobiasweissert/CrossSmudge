"""Read/sync negative-ack qualification against pinned real FAT."""
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parent
SDK = Path('/root/github/crosssmudge-lua-http/.pio/libdeps/sticky/SdFat/src')
assert hashlib.sha256((SDK/'FatLib/FatFile.cpp').read_bytes()).hexdigest() == 'c1757a57ea134df2aab650d2db109b7d731f66cf309e5a070a9b50667c258948'
inputs = list(SDK.rglob('*.h')) + list(SDK.rglob('*.cpp')) + list(ROOT.glob('*.h')) + [ROOT/'sdfat_io_fault_test.cpp', Path(__file__).resolve()]
def hashes():
    return {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
with open(ROOT.parent/'candidate.lock','r+') as lock:
    fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    before=hashes()
    results=[]
    for old in json.loads((ROOT/'sdfat-receipt.json').read_text())['results']:
        mode=old['mode']; cmd=old['command'][:]
        cmd[cmd.index(str(ROOT/'sdfat_test.cpp'))]=str(ROOT/'sdfat_io_fault_test.cpp')
        exe=Path(os.environ['TMPDIR'])/('crosssmudge-sdfat-io-'+mode)
        cmd[-1]=str(exe)
        c=subprocess.run(cmd,capture_output=True,text=True,timeout=60)
        (ROOT/('io-'+mode+'-compile.log')).write_text(c.stdout+c.stderr)
        result=dict(mode=mode,command=cmd,compile_exit=c.returncode)
        if c.returncode==0:
            r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30)
            (ROOT/('io-'+mode+'-run.log')).write_text(r.stdout+r.stderr)
            result.update(run_exit=r.returncode,groups=[s for s in r.stdout.splitlines() if s.startswith('PASS ')])
        results.append(result)
    receipt=dict(observed_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),results=results,before=before,after=hashes(),hardware_enabled=False)
    stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    path=ROOT/('io-evidence-'+stamp+'.json')
    path.write_text(json.dumps(receipt,indent=2)+'\n')
    (ROOT/'io-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(json.dumps(dict(evidence=str(path),results=results),indent=2))
    assert before==receipt['after']
    assert all(r['compile_exit']==0 and r.get('run_exit')==0 and len(r.get('groups',[]))==5 for r in results)
