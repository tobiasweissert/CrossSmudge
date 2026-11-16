#!/usr/bin/env python3
"""Production parser/security tests; plistlib generates independent binary-plist inputs."""
import copy
import json
import os
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
temporary = None
if os.environ.get('AIRDROP_TEST_OUTPUT'):
    OUT = Path(os.environ['AIRDROP_TEST_OUTPUT'])
else:
    temporary = tempfile.TemporaryDirectory(prefix='airdrop-offer-')
    OUT = Path(temporary.name)
OUT.mkdir(parents=True, exist_ok=True)
FIX = OUT / 'fixtures'
FIX.mkdir(exist_ok=True)
uid = {'id': 'A3F34E01-1234-5678-ABCD-EF0123456789'}
url = {'SenderComputerName': 'Sender', 'Items': ['https://example.com/a#fragment'], 'Files': [], 'TransferID': uid, 'TransferType': {'links': {}}}
file = {'SenderComputerName': 'Sender', 'Items': [], 'Files': [{'FileName': 'book.epub', 'FileBomPath': './book.epub', 'FileType': 'org.idpf.epub-container', 'FileSize': 123, 'FileIsDirectory': False, 'ShouldConvertMediaFormats': False}], 'TransferID': uid, 'TransferType': {'files': {}}}
def put(name, value):
    data = value if isinstance(value, bytes) else plistlib.dumps(value, fmt=plistlib.FMT_BINARY, sort_keys=True)
    (FIX / name).write_bytes(data)
    return data
put('url', url)
a=copy.deepcopy(url); a.pop('TransferID'); put('url_noid',a)
a=copy.deepcopy(url); a['SenderComputerName']='Téléphone 📚';put('url_utf16_sender',a)
put('file', file)
a=copy.deepcopy(file); a['Files'][0]['FileSize']=30000000; put('file_limit',a)
a=copy.deepcopy(file); a['Files'][0]['FileBomPath']='./wrapper/book.epub'; put('file_wrapper',a)
a=copy.deepcopy(file); a.pop('TransferID'); a.pop('TransferType'); put('file_noid',a)
a=copy.deepcopy(url); a['FileIcon']=bytes(220000); put('large_icon',a)
# Keep the complete wire plist exactly at the bounded random-access Ask cap.
a=copy.deepcopy(url); a['FileIcon']=bytes(523900)
for _ in range(4):
    encoded=plistlib.dumps(a,fmt=plistlib.FMT_BINARY)
    if len(encoded)==524288: break
    a['FileIcon']=bytes(len(a['FileIcon'])+524288-len(encoded))
assert len(encoded)==524288
put('ask_limit',encoded)
a['FileIcon']+=b'x';put('ask_over',a)
a=copy.deepcopy(file);a['Items']=url['Items'];put('mixed',a)
a=copy.deepcopy(url);a['Items']*=2;put('batch',a)
a=copy.deepcopy(url);a['Items']=[];put('empty',a)
for name,s in [('http','http://example.com/'),('bare','example.com'),('userinfo','https://user@example.com/'),('private_literal','https://127.0.0.1/'),('encoded_slash','https://example.com/%5c'),('control_url','https://example.com/\n'),('long_url','https://example.com/'+('a'*512))]:
    a=copy.deepcopy(url);a['Items']=[s];put(name,a)
for name,key,value in [('file_over','FileSize',30000001),('traversal','FileName','../book.epub'),('unsafe_fat','FileName','bad:book.epub'),('device_name','FileName','CON.txt'),('directory','FileIsDirectory',True),('bad_size','FileSize',-1),('executable','FileName','run.lua'),('bad_bom','FileBomPath','./x/../book.epub')]:
    a=copy.deepcopy(file);a['Files'][0][key]=value;put(name,a)
a=copy.deepcopy(url);a['SenderComputerName']='é';b=plistlib.dumps(a,fmt=plistlib.FMT_BINARY);assert b'\x61\x00\xe9' in b;put('bad_utf16',b.replace(b'\x61\x00\xe9',b'\x61\xd8\x00'))
b=bytearray(put('_seed',url));w=b[-26];rw=b[-25];num=int.from_bytes(b[-24:-16],'big');tab=int.from_bytes(b[-8:],'big')
# Corrupt offset table, references and trailer independently.
c=bytearray(b);c[tab:tab+w]=bytes(w);put('bad_offset',bytes(c))
root=int.from_bytes(b[-16:-8],'big');off=int.from_bytes(b[tab+root*w:tab+(root+1)*w],'big');count=b[off]&15
assert count<15 and b[off]>>4==13
c=bytearray(b);c[off+1+count*rw:off+1+(count+1)*rw]=root.to_bytes(rw,'big');put('cycle',bytes(c))
c=bytearray(b);c[off+1+rw:off+1+2*rw]=c[off+1:off+1+rw];put('duplicate_key',bytes(c))
c=bytearray(b);c[off+1:off+1+rw]=(num+1).to_bytes(rw,'big');put('bad_ref',bytes(c))
c=bytearray(b);c[off]=0x80;put('bad_type',bytes(c))
c=bytearray(b);c[-32]=1;put('bad_trailer',bytes(c))
c=bytearray(b);c[-24:-16]=(4097).to_bytes(8,'big');put('huge_object_count',bytes(c))
a=copy.deepcopy(url);nested=[]
for _ in range(18): nested=[nested]
a['Extra']=nested;put('too_deep',a)

mode='sanitized' if '--sanitize' in sys.argv else 'normal'
env=dict(os.environ)
if mode=='sanitized':
    # Sandbox ptrace restrictions make LeakSanitizer unavailable; keep ASan+UBSan active.
    env['ASAN_OPTIONS']='detect_leaks=0'
exe=OUT/('offer-cases-'+mode)
cmd=['c++','-std=c++17','-g','-O1','-Wall','-Wextra','-Werror','-Ilib/AirDrop/core','test/airdrop/offer_cases.cpp','lib/AirDrop/core/Offer.cpp','lib/AirDrop/core/HttpRequest.cpp','lib/AirDrop/core/Consent.cpp','-o',str(exe)]
if mode=='sanitized': cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
with (OUT/(mode+'.log')).open('w') as log:
    print('COMMAND',json.dumps(cmd),file=log,flush=True)
    compile_result=subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,env=env)
    result=compile_result.returncode
    if result==0:
        print('COMMAND',json.dumps([str(exe),str(FIX)]),file=log,flush=True)
        result=subprocess.run([str(exe),str(FIX)],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,env=env).returncode
if result:
    print((OUT/(mode+'.log')).read_text(),file=sys.stderr)
print(mode,'exit',result)
sys.exit(result)
