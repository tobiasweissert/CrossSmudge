#!/usr/bin/env python3
"""Exact Discovery extraction + AWDL production codec with independent DNS decoding."""
import argparse
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile

parser=argparse.ArgumentParser();parser.add_argument('--sanitize',action='store_true');args=parser.parse_args()
ROOT=Path(__file__).resolve().parents[2]
temporary=None
if os.environ.get('AIRDROP_DISCOVERY_TEST_OUTPUT'):BASE=Path(os.environ['AIRDROP_DISCOVERY_TEST_OUTPUT'])
else:temporary=tempfile.TemporaryDirectory(prefix='airdrop-discovery-');BASE=Path(temporary.name)
mode='sanitized' if args.sanitize else 'normal';BASE=BASE/mode;BASE.mkdir(parents=True,exist_ok=True)
source=ROOT/'lib/AirDrop/port/Discovery.cpp';original=source.read_text()
port=ROOT/'lib/AirDrop/vendor/ESP32Drop/src/awdl/port/awdl_port_esp32.h';p=port.read_text()
identity=re.search(r'struct AwdlIdentity\s*\{.*?\};',p,re.S).group(0)
headroom=re.search(r'#define AWDL_TX_HEADROOM\s+[^\n]+',p).group(0)
body=original[original.index('namespace airdrop::discovery {'):original.rindex('#endif')]
# Source guard complements the callback allocation counter; raw-driver internals remain a device gate.
callbackCode=re.sub(r'//[^\n]*|/\*.*?\*/','',body,flags=re.S)
for forbidden in [r'\bmalloc\s*\(',r'\bcalloc\s*\(',r'\brealloc\s*\(',r'\bnew\s+',r'\bdelete\s',r'\bLOG_\w*\s*\(',r'\bSerial\s*\.',r'\bvTaskDelay\s*\(',r'\bxSemaphoreTake\s*\(',r'\bdelay\s*\(',r'std::(?:vector|string)\b']:
    assert not re.search(forbidden,callbackCode),'Direct allocation/log/block in discovery callback source: '+forbidden
prelude=r'''
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <cassert>
#include "vendor/ESP32Drop/src/airdrop/core/ad_dnssd.h"
#include "vendor/ESP32Drop/src/awdl/core/awdl_frame.h"
static bool insideCallback=false;
static size_t allocations=0;
void* operator new(size_t n){if(insideCallback)++allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,size_t)noexcept{std::free(p);}void operator delete[](void* p,size_t)noexcept{std::free(p);}
'''
prelude+='\n'+identity+'\n'+headroom+r'''
static AwdlIdentity fixtureIdentity{{0x2a,0x84,0x85,0x43,0xb3,0x0c},{0xfe,0x80,0,0,0,0,0,0,0x28,0x84,0x85,0xff,0xfe,0x43,0xb3,0x0c},"2a848543b30c",{}};
using Tap=void(*)(void*,const uint8_t[6],const uint8_t*,size_t);
static Tap tap=nullptr;static unsigned sends=0;static uint8_t sent[1280];static size_t sentLength=0;
void awdl_identity(AwdlIdentity* out){*out=fixtureIdentity;}
int awdl_rx_tap_set(Tap value,void*){tap=value;return 0;}
int awdl_tx_ip6_now(const uint8_t peer[6],uint8_t* bytes,size_t n){assert(peer[0]==1&&n<=1280);std::memcpy(sent,bytes+AWDL_TX_HEADROOM,n);sentLength=n;++sends;return 0;}
'''
cpp=BASE/'extracted.cpp';cpp.write_text(prelude+'\n'+body+'\n'+(ROOT/'test/airdrop/discovery_cases.inc').read_text())
golden=(ROOT/'test/airdrop/discovery_mif_golden.inc').read_bytes()
assert hashlib.sha1(b'blob '+str(len(golden)).encode()+b'\0'+golden).hexdigest()=='7fdfff0e774de38c7483609adee2ca3b616252ec'
name=lambda n:b''.join(bytes([len(x)])+x.encode()for x in n.split('.'))+b'\0'
instance='2a848543b30c._airdrop._tcp.local';host='2a848543b30c.local'
src=ipaddress.IPv6Address('fe80::1234').packed;dst=ipaddress.IPv6Address('ff02::fb').packed
own=bytes.fromhex('fe80000000000000288485fffe43b30c')
def checksum(data):
    if len(data)%2:data+=b'\0'
    value=sum(struct.unpack('!'+str(len(data)//2)+'H',data))
    while value>>16:value=(value&65535)+(value>>16)
    return (~value)&65535

def question(n,t=12,c=1):return name(n)+struct.pack('!HH',t,c)
def query(questions,flags=0,answers=b'',an=0):return struct.pack('!HHHHHH',0,flags,len(questions),an,0,0)+b''.join(questions)+answers
def packet(dns,source=src,destination=dst,hop=255):
    udp=struct.pack('!HHHH',5353,5353,len(dns)+8,0)+dns
    pseudo=source+destination+struct.pack('!I3xB',len(udp),17)
    check=checksum(pseudo+udp) or 65535
    udp=udp[:6]+struct.pack('!H',check)+udp[8:]
    return struct.pack('!IHBB',6<<28,len(udp),17,hop)+source+destination+udp
cases={}
def put(k,dns,emit=0,**kw):cases[k]=(packet(dns,**kw),emit)
good=query([question('_airdrop._tcp.local')]);put('service-ptr',good,1)
put('service-any',query([question('_airdrop._tcp.local',255)]),1)
put('own-srv',query([question(instance,33)]),1);put('own-txt',query([question(instance,16)]),1)
put('host-aaaa',query([question(host,28)]),1);put('host-a-negative',query([question(host,1)]),1)
put('own-unicast',good,1,destination=own)
put('unrelated',query([question('_ssh._tcp.local')]))
put('pairing',query([question('_applicationServicePairing._tcp.local')]))
put('prepair',query([question('_appSvcPrePair._tcp.local')]))
put('bad-opcode',query([question('_airdrop._tcp.local')],flags=0x0800))
put('response-not-query',query([question('_airdrop._tcp.local')],flags=0x8000))
put('wrong-class',query([question('_airdrop._tcp.local',12,3)]))
put('zero-questions',query([]))
put('truncated-question',struct.pack('!HHHHHH',0,0,1,0,0,0)+b'\x07_air')
put('forward-pointer',struct.pack('!HHHHHH',0,0,1,0,0,0)+b'\xc0\x0c'+struct.pack('!HH',12,1))
put('reserved-label',struct.pack('!HHHHHH',0,0,1,0,0,0)+b'\x80'+b'x'*4)
put('malformed-after-relevant',struct.pack('!HHHHHH',0,0,2,0,0,0)+question('_airdrop._tcp.local')+b'\x05no')
put('extra-trailing',good+b'junk')
put('many-questions',query([question('_airdrop._tcp.local')]*17))
put('global-source',good,source=ipaddress.IPv6Address('2001:db8::1').packed)
put('wrong-destination',good,destination=ipaddress.IPv6Address('ff02::1').packed)
put('wrong-hop',good,hop=64)
base=packet(good)
for key,mutate in [('bad-checksum',lambda x:x.__setitem__(47,x[47]^1)),('zero-checksum',lambda x:x.__setitem__(slice(46,48),b'\0\0')),('wrong-payload-length',lambda x:x.__setitem__(slice(4,6),b'\0\0')),('bad-version',lambda x:x.__setitem__(0,0x40)),('wrong-port',lambda x:x.__setitem__(slice(42,44),b'\0\x50'))]:
    b=bytearray(base);mutate(b);cases[key]=(bytes(b),0)
for cut in range(len(base)):cases['truncated-'+str(cut)]=(base[:cut],0)
# Valid compressed question pointers are admitted; malformed answer spans fail as a whole.
put('compressed-question',query([question('_airdrop._tcp.local'),b'\xc0\x0c'+struct.pack('!HH',255,1)]),1)
put('truncated-answer',query([question('_airdrop._tcp.local')],answers=b'\x00\x00',an=1))
# A DNS wire name is at most255 octets INCLUDING the final root byte.
overlong=b''.join(bytes([n])+b'a'*n for n in [63,63,63,62])+b'\0'+struct.pack('!HH',255,1)
put('overlong-name-after-relevant',struct.pack('!HHHHHH',0,0,2,0,0,0)+question('_airdrop._tcp.local')+overlong)
# PTR/SRV/NSEC carry encoded names inside RDATA too; they must not escape graph checks.
bad_rr=name('_airdrop._tcp.local')+struct.pack('!HHIH',12,1,120,2)+b'\xc0\xff'
put('bad-rdata-pointer',query([question('_airdrop._tcp.local')],answers=bad_rr,an=1))
def rr(owner,kind,data):return name(owner)+struct.pack('!HHIH',kind,1,120,len(data))+data
for key,kind,data in [
    ('bad-srv-pointer',33,struct.pack('!HHH',0,0,22)+b'\xc0\xff'),
    ('short-srv',33,b'\0'*5),
    ('bad-nsec-pointer',47,b'\xc0\xff'+b'\0\1\x40'),
    ('empty-nsec-bitmap',47,name('extra.local')+b'\0\0'),
    ('duplicate-nsec-window',47,name('extra.local')+b'\0\1\x40\0\1\x40'),
    ('nsec-trailing-zero',47,name('extra.local')+b'\0\2\x40\0'),
    ('bad-txt-string',16,b'\x03ab'),('bad-a-size',1,b'\0'*3),('bad-aaaa-size',28,b'\0'*15)]:
    put(key,query([question('_airdrop._tcp.local')],answers=rr('extra.local',kind,data),an=1))
for key,kind,data in [
    ('valid-extra-ptr',12,name('extra.local')),
    ('valid-extra-srv',33,struct.pack('!HHH',0,0,22)+name('extra.local')),
    ('valid-extra-nsec',47,name('extra.local')+b'\0\1\x40'),
    ('valid-extra-txt',16,b'\x03abc'),('valid-opaque-rdata',65000,b'\xc0\xff')]:
    put(key,query([question('_airdrop._tcp.local')],answers=rr('extra.local',kind,data),an=1),1)
put('maximum-questions',query([question('_airdrop._tcp.local')]*16),1)
# Independent decoder: validates DNS byte spans/types and checksums; never calls production codec.
def dns_name(data,offset):
    labels=[];end=None;seen=set()
    while True:
        assert offset<len(data)and offset not in seen;seen.add(offset);n=data[offset];offset+=1
        if n&0xc0==0xc0:
            assert offset<len(data);target=((n&63)<<8)|data[offset];end=end or offset+1;offset=target;continue
        assert n<64 and offset+n<=len(data)
        if not n:return '.'.join(labels),end or offset
        labels.append(data[offset:offset+n].decode());offset+=n

def validate_dns(response):
    assert len(response)<=1280 and response[0]>>4==6 and response[6:8]==bytes([17,255])
    udp=response[40:];assert len(udp)==int.from_bytes(response[4:6],'big')==int.from_bytes(udp[4:6],'big')
    assert response[8:24]==own and response[24:40]==dst
    assert int.from_bytes(udp[6:8],'big')and checksum(response[8:40]+struct.pack('!I3xB',len(udp),17)+udp)==0
    dns=udp[8:];header=struct.unpack('!6H',dns[:12]);assert header==(0,0x8400,0,6,0,0)
    at=12;records=[]
    for _ in range(6):
        owner,at=dns_name(dns,at);kind,cls,ttl,n=struct.unpack('!HHIH',dns[at:at+10]);at+=10;end=at+n;assert end<=len(dns);records.append((owner,kind,cls,ttl,at,end));at=end
    assert at==len(dns)and[k for _,k,*_ in records]==[12,33,16,28,47,47]
    for owner,kind,cls,ttl,start,end in records:
        assert ttl==120 and cls==(1 if kind==12 else 0x8001)
        if kind==12:assert owner=='_airdrop._tcp.local'and dns_name(dns,start)==(instance,end)
        if kind==33:assert owner==instance and struct.unpack('!HHH',dns[start:start+6])==(0,0,8770)and dns_name(dns,start+6)==(host,end)
        if kind==16:
            assert owner==instance and dns[start:end]==b'\x0aflags=0131';flags=int(dns[start+7:end].decode(),10);assert flags==131 and flags&~0x83==0
        if kind==28:assert owner==host and dns[start:end]==own and ipaddress.IPv6Address(dns[start:end]).is_link_local
        if kind==47:
            target,pos=dns_name(dns,start);assert target==owner;window,size=dns[pos:pos+2];bitmap=dns[pos+2:end];assert window==0 and len(bitmap)==size
            types={i*8+b for i,byte in enumerate(bitmap)for b in range(8)if byte&(128>>b)}
            assert types==({28}if owner==host else {16,33})
    assert not any('_app' in owner or 'pair' in owner.lower()for owner,*_ in records)

def validate_mif(data,sticky):
    at=40;services=[]
    while at<len(data):
        tag=data[at];n=int.from_bytes(data[at+1:at+3],'little');start=at+3;end=start+n;assert end<=len(data)
        if tag==2:
            name_len=int.from_bytes(data[start:start+2],'little');rtype=data[start+2+name_len-1];p=start+2+name_len;rdlen=int.from_bytes(data[p:p+2],'little');assert p+4+rdlen==end;services.append((rtype,n,rdlen,data[p+4:end]))
        at=end
    assert at==len(data)and[t for t,*_ in services]==[12,33,16]
    txt=services[2];assert txt[1:3]==(33,11)and txt[3]==(b'\x0aflags=0131'if sticky else b'\x0aflags=1019')

flags=['-O1','-g','-fno-omit-frame-pointer']
if args.sanitize:flags+=['-fsanitize=address,undefined']
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
dependencies=[ROOT/'lib/AirDrop/vendor/ESP32Drop/src/awdl/core/awdl_frame.h',ROOT/'lib/AirDrop/vendor/ESP32Drop/src/awdl/core/awdl_window.h',ROOT/'lib/AirDrop/vendor/ESP32Drop/src/airdrop/core/ad_dnssd.h']
dependencyHashes={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in dependencies}
receipts=[];failed=False
for sticky in [False,True]:
    out=BASE/('sticky'if sticky else 'default');out.mkdir(exist_ok=True)
    for key,(value,expected)in cases.items():(out/(key+'.ip6')).write_bytes(value)
    (out/'cases.tsv').write_text(''.join(f'{key} {expected}\n'for key,(_,expected)in cases.items()))
    commands=[['c++','-std=c++20',*flags,*(['-DFREEINK_DEVICE_STICKY=1']if sticky else []),'-Ilib/AirDrop','-Itest/airdrop',str(cpp),'-o',str(out/'discovery')],[str(out/'discovery'),str(out)]]
    with(out/'commands.log').open('w')as log:
        for command in commands:
            print('COMMAND',json.dumps(command),file=log,flush=True);result=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT);receipts.append({'command':command,'exit':result.returncode})
            if result.returncode:failed=True;log.flush();print((out/'commands.log').read_text());break
    if(out/'response.bin').exists():validate_dns((out/'response.bin').read_bytes())
    if(out/'mif.bin').exists():validate_mif((out/'mif.bin').read_bytes(),sticky)
(BASE/'results.json').write_text(json.dumps(receipts,indent=2)+'\n')
(BASE/'source.json').write_text(json.dumps({'production':str(source.relative_to(ROOT)),'sha256':hashlib.sha256(original.encode()).hexdigest(),'golden_pin':'92bdc1221924ca522f3ccbebbd9353612ce5b2d7','golden_path':'testdata/mif_golden.inc','golden_git_blob':'7fdfff0e774de38c7483609adee2ca3b616252ec','golden_sha256':hashlib.sha256((ROOT/'test/airdrop/discovery_mif_golden.inc').read_bytes()).hexdigest(),'dependencies':dependencyHashes},indent=2)+'\n')
assert source.read_text()==original,'Discovery changed during test; rerun frozen source'
assert dependencyHashes=={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()for p in dependencies},'Discovery codec changed during test; rerun frozen source'
if failed:raise SystemExit(1)
print('PASS',mode,'Discovery exact extraction/DNS checksum+6RR/strict malformed query/AWDL default1019 Sticky0131')
