#!/usr/bin/env python3
"""Bounded real parser compilation. No dependency downloads or SDK init."""
import pathlib, subprocess, hashlib, json, sys
ROOT = pathlib.Path(__file__).resolve().parents[3]
OUT = pathlib.Path('/root/github/.host-data/crosssmudge/crosspoint-rebase-1655/integration/network/tests')
SDK = pathlib.Path('/root/github/crosssmudge/freeink-sdk')
SHA = '634a3f8c067a3714a6a7c622015138af096ab5d2'
OUT.mkdir(parents=True, exist_ok=True)
for target, source in [('StreamingJsonParser.h','include/StreamingJsonParser.h'),('StreamingJsonParser.cpp','src/StreamingJsonParser.cpp')]:
    data = subprocess.check_output(['git','-C',str(SDK),'show',SHA+':libs/network/JsonSax/'+source],timeout=15)
    (OUT/target).write_bytes(data)
mode = 'sanitize' if '--sanitize' in sys.argv else 'normal'
cmd = ['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-g','-I'+str(OUT),'-I'+str(ROOT/'lib/JsonParser'),str(ROOT/'test/crosspoint_migration/network/parser.cpp'),str(ROOT/'lib/JsonParser/ReleaseJsonParser.cpp'),str(OUT/'StreamingJsonParser.cpp'),'-o',str(OUT/('parser-'+mode))]
if mode=='sanitize': cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
run = subprocess.run(cmd,capture_output=True,text=True,timeout=45)
log = '$ '+ ' '.join(cmd)+'\n'+run.stdout+run.stderr
rc=run.returncode
if not rc:
    run = subprocess.run([str(OUT/('parser-'+mode))],capture_output=True,text=True,timeout=15)
    log += run.stdout+run.stderr;rc=run.returncode
(OUT/('parser-'+mode+'.log')).write_text(log)
print(log)
inputs=[ROOT/'test/crosspoint_migration/network/parser.cpp',ROOT/'test/crosspoint_migration/network/run.py',ROOT/'lib/JsonParser/ReleaseJsonParser.h',ROOT/'lib/JsonParser/ReleaseJsonParser.cpp',OUT/'StreamingJsonParser.h',OUT/'StreamingJsonParser.cpp']
(OUT/('parser-'+mode+'.json')).write_text(json.dumps({'returncode':rc,'command':cmd,'inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}},indent=2))
sys.exit(rc)
