#!/usr/bin/env python3
"""Compile and execute actual Session/HTTP/Offer/Consent/miniz decoder with scripted I/O."""
import json
import hashlib
import os
from pathlib import Path
import plistlib
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT=Path(__file__).resolve().parents[2]
temporary=None
if os.environ.get('AIRDROP_SESSION_TEST_OUTPUT'):
    OUT=Path(os.environ['AIRDROP_SESSION_TEST_OUTPUT'])
else:
    temporary=tempfile.TemporaryDirectory(prefix='airdrop-session-');OUT=Path(temporary.name)
mode='sanitized' if '--sanitize' in sys.argv else 'normal'
OUT=OUT/mode;OUT.mkdir(parents=True,exist_ok=True)
uid={'id':'A3F34E01-1234-5678-ABCD-EF0123456789'}
url={'SenderComputerName':'Sender','Files':[],'Items':['https://example.com/a#fragment'],'TransferType':{'links':{}},'TransferID':uid}
file={'SenderComputerName':'Sender','Items':[],'Files':[{'FileName':'book.txt','FileBomPath':'./book.txt','FileType':'public.plain-text','FileSize':3,'FileIsDirectory':False,'ShouldConvertMediaFormats':False}],'TransferType':{'files':{}},'TransferID':uid}
for name,value in [('url',url),('file',file),('file-noid',{k:v for k,v in file.items()if k!='TransferID'})]:
    (OUT/(name+'.plist')).write_bytes(plistlib.dumps(value,fmt=plistlib.FMT_BINARY))
def member(name,data,mode=0o100644):
    h=('070707'+''.join(f'{v:06o}'for v in [0,1,mode,0,0,1,0])+f'{0:011o}'+f'{len(name)+1:06o}'+f'{len(data):011o}').encode()
    assert len(h)==76
    return h+name.encode()+b'\0'+data
archive=member('./book.txt',b'abc')+member('TRAILER!!!',b'',0)
for compressed,name in [(False,'stored'),(True,'zlib')]:
    body=zlib.compress(archive)if compressed else archive
    word=len(body)|(0 if compressed else 0x80000000)
    (OUT/(name+'.dvzip')).write_bytes(struct.pack('>I',word)+body+b'\0\0\0\0')
sources=['lib/AirDrop/session/Session.h','lib/AirDrop/session/Session.cpp','lib/AirDrop/core/Offer.h','lib/AirDrop/core/Offer.cpp','lib/AirDrop/core/HttpRequest.h','lib/AirDrop/core/HttpRequest.cpp','lib/AirDrop/core/Consent.h','lib/AirDrop/core/Consent.cpp','lib/AirDrop/stream/StreamDecoder.h','lib/AirDrop/stream/StreamDecoder.cpp','lib/miniz/src/miniz_impl.c']
sourceHashes={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest()for p in sources}
(OUT/'source.json').write_text(json.dumps(sourceHashes,indent=2)+'\n')
flags=['-O1','-g','-fno-omit-frame-pointer']
if mode=='sanitized':flags+=['-fsanitize=address,undefined']
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',SMUDGE_X3_CONSTRAINTS='1')
commands=[['cc','-std=c11',*flags,'-Ilib/miniz/src','-c','lib/miniz/src/miniz_impl.c','-o',str(OUT/'miniz.o')],['c++','-std=c++20',*flags,'-Ilib/AirDrop','-Ilib/miniz/src','test/airdrop/session_cases.cpp','lib/AirDrop/session/Session.cpp','lib/AirDrop/core/Offer.cpp','lib/AirDrop/core/HttpRequest.cpp','lib/AirDrop/core/Consent.cpp','lib/AirDrop/stream/StreamDecoder.cpp',str(OUT/'miniz.o'),'-o',str(OUT/'session')],[str(OUT/'session'),str(OUT)]]
receipts=[]
with(OUT/'commands.log').open('w')as log:
    for command in commands:
        print('COMMAND',json.dumps(command),file=log,flush=True)
        result=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
        receipts.append({'command':command,'exit':result.returncode});(OUT/'results.json').write_text(json.dumps(receipts,indent=2)+'\n')
        if result.returncode:
            log.flush();print((OUT/'commands.log').read_text(),file=sys.stderr);raise SystemExit(result.returncode)
assert sourceHashes=={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest()for p in sources},'Production sources changed during test; rerun on frozen source'
print('PASS',mode,'production Session scripted-I/O integration')
