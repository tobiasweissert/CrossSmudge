#!/usr/bin/env python3
"""Real immutable SDK OPDS2 parser; production reservation bodies over heap double."""
from pathlib import Path
import subprocess,sys,json,hashlib
R=Path(__file__).resolve().parents[3];T=Path(__file__).resolve().parent
O=Path('/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/integration/network/tests/opds');O.mkdir(parents=True,exist_ok=True)
SDK='/root/github/crosssmudge/freeink-sdk';SHA='634a3f8c067a3714a6a7c622015138af096ab5d2'
def blob(p):return subprocess.check_output(['git','-C',SDK,'show',SHA+':'+p],timeout=15)
for target,path in [('Opds2Parser.h','libs/network/Opds/include/Opds2Parser.h'),('Opds2Parser.cpp','libs/network/Opds/src/Opds2Parser.cpp'),('OpdsEntry.h','libs/network/Opds/include/OpdsEntry.h'),('OpdsLog.h','libs/network/Opds/include/OpdsLog.h'),('Print.h','libs/network/Opds/test/host/stubs/Print.h'),('StreamingJsonParser.h','libs/network/JsonSax/include/StreamingJsonParser.h'),('StreamingJsonParser.cpp','libs/network/JsonSax/src/StreamingJsonParser.cpp')]:
 (O/target).write_bytes(blob(path))
atom=blob('libs/network/Opds/src/OpdsParser.cpp').decode()
reserve=atom[atom.index('namespace {'):atom.index('OpdsParser::OpdsParser()')]
(O/'reserve.cpp').write_text('#include <OpdsEntry.h>\n#include <algorithm>\n#include <OpdsLog.h>\nstruct Heap {size_t getMaxAllocHeap(){return 100000;}} ESP;\nusing namespace OpdsLimits;\n'+reserve)
mode='sanitize' if '--sanitize' in sys.argv else 'normal'
cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-g','-I'+str(O),str(T/'opds.cpp'),str(O/'Opds2Parser.cpp'),str(O/'StreamingJsonParser.cpp'),str(O/'reserve.cpp'),'-o',str(O/mode)]
if mode=='sanitize':cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
p=subprocess.run(cmd,capture_output=True,text=True,timeout=45);log='$ '+' '.join(cmd)+'\n'+p.stdout+p.stderr;rc=p.returncode
if not rc:
 p=subprocess.run([str(O/mode)],capture_output=True,text=True,timeout=15);log+=p.stdout+p.stderr;rc=p.returncode
(O/(mode+'.log')).write_text(log);print(log)
inputs=[T/'opds.cpp',Path(__file__),*O.glob('*.h'),*O.glob('*.cpp')]
(O/(mode+'.json')).write_text(json.dumps({'returncode':rc,'sdk':SHA,'command':cmd,'inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}},indent=2))
sys.exit(rc)
