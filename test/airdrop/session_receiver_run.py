#!/usr/bin/env python3
"""Narrow extracted production Receiver methods; fake SDK boundaries, no radio/device proof."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

parser=argparse.ArgumentParser();parser.add_argument('--sanitize',action='store_true');parser.add_argument('--sdk',required=True,type=Path)
args=parser.parse_args();ROOT=Path(__file__).resolve().parents[2]
temporary=None
if os.environ.get('AIRDROP_SESSION_TEST_OUTPUT'):OUT=Path(os.environ['AIRDROP_SESSION_TEST_OUTPUT'])
else:temporary=tempfile.TemporaryDirectory(prefix='airdrop-receiver-');OUT=Path(temporary.name)
mode='sanitized' if args.sanitize else 'normal';OUT=OUT/('receiver-'+mode);OUT.mkdir(parents=True,exist_ok=True)
source=ROOT/'lib/AirDrop/port/ReceiverEsp32.cpp';text=source.read_text()
def function(marker):
    start=text.index(marker);opening=text.index('{',start);depth=0;quote=None;escape=False;comment=None;at=opening
    while at<len(text):
        c=text[at];pair=text[at:at+2]
        if comment=='line':
            if c=='\n':comment=None
        elif comment=='block':
            if pair=='*/':comment=None;at+=1
        elif quote:
            if escape:escape=False
            elif c=='\\':escape=True
            elif c==quote:quote=None
        elif pair=='//':comment='line';at+=1
        elif pair=='/*':comment='block';at+=1
        elif c in ['"',"'"]:quote=c
        elif c=='{':depth+=1
        elif c=='}':
            depth-=1
            if depth==0:return text[start:at+1]
        at+=1
    raise ValueError('unclosed production function '+marker)

sockets=args.sdk/'components/lwip/lwip/src/include/lwip/sockets.h';netif=args.sdk/'components/lwip/lwip/src/include/lwip/netif.h'
s=sockets.read_text();n=netif.read_text();match=re.search(r'struct ifreq\s*\{.*?\};',s,re.S);assert match
nameSize=re.search(r'#define NETIF_NAMESIZE\s+(\d+)',n).group(1)
constants='\n'.join(re.search(r'^#define\s+'+name+r'\s+[^\n]+',s,re.M).group(0)for name in ['SOL_SOCKET','SO_BINDTODEVICE','SO_REUSEADDR'])
layout='#define NETIF_NAMESIZE '+nameSize+'\n#define IFNAMSIZ NETIF_NAMESIZE\n'+match.group(0)+'\n'+constants+'\n'
prelude=(ROOT/'test/airdrop/session_receiver_prelude.inc').read_text()
# SDK structure/constants precede fake setsockopt; all behavior functions remain exact production text.
prelude=prelude.replace('static unsigned socketCalls=0;',layout+'\nstatic unsigned socketCalls=0;')
parts=[prelude,text[text.index('int decisionValue=0;'):text.index('uint64_t clockMs()')]]
for name in ['bool equal(','void state(','void failure(','void protocolFailure(','void closeFd(','bool finishFile(','bool preparePsa(','bool initTls(','bool bindInterface(','bool listen(','void freeTls(','void pending(','int decision(']:parts.append(function(name))
cleanup_line=next(line for line in text.splitlines() if 'status.ownershipUncertain=false;status.state=' in line)
parts.append('void completeStatus(Context& c){'+cleanup_line+'}')
parts+=['}\n',function('Receiver& Receiver::instance()'),function('bool Receiver::decide('),'}\n']
# Suffix closes the namespace itself; strip its two initial closes since production/public pieces did so.
suffix=(ROOT/'test/airdrop/session_receiver_cases.inc').read_text().split('int main',1)[1]
parts.append('int main'+suffix)
cpp=OUT/'extracted.cpp';cpp.write_text('\n'.join(parts))
slotSource=args.sdk/'components/mbedtls/mbedtls/library/psa_crypto_slot_management.c'
cryptoSource=args.sdk/'components/mbedtls/mbedtls/library/psa_crypto.c'
cryptoConfig=args.sdk/'components/mbedtls/mbedtls/include/mbedtls/mbedtls_config.h'
portConfig=args.sdk/'components/mbedtls/port/include/mbedtls/esp_config.h'
assert re.search(r'#define KEY_SLOT_VOLATILE_SLICE_BASE_LENGTH\s+16u',slotSource.read_text()),'PSA first-slice assumption changed; requalify warmup'
manifest={'production':{'path':str(source.relative_to(ROOT)),'sha256':hashlib.sha256(text.encode()).hexdigest()},'sdk':{str(p):hashlib.sha256(p.read_bytes()).hexdigest()for p in [sockets,netif,slotSource,cryptoSource,cryptoConfig,portConfig]},'classification':'verbatim extracted methods with fake crypto/socket/thread-critical-section boundaries; no full worker/device proof'}
(OUT/'source.json').write_text(json.dumps(manifest,indent=2)+'\n')
flags=['-O1','-g','-fno-omit-frame-pointer']
if args.sanitize:flags+=['-fsanitize=address,undefined']
commands=[['c++','-std=c++20',*flags,'-Ilib/AirDrop','-Ilib/LuaHttp',str(cpp),'-o',str(OUT/'receiver')]]
commands +=[[str(OUT/'receiver'),case]for case in ['empty-file','request-failure','queue','bind','listen','listen-interface-fail','listen-bind-fail','cold','psa-fail','import-fail','destroy-fail','tls:mbedtls_ctr_drbg_seed','tls:mbedtls_x509_crt_parse_der','tls:mbedtls_pk_parse_key','tls:mbedtls_ssl_config_defaults','tls:mbedtls_ssl_conf_own_cert','tls:mbedtls_ssl_setup']]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
receipts=[]
with(OUT/'commands.log').open('w')as log:
    for command in commands:
        print('COMMAND',json.dumps(command),file=log,flush=True);result=subprocess.run(command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
        receipts.append({'command':command,'exit':result.returncode});(OUT/'results.json').write_text(json.dumps(receipts,indent=2)+'\n')
        if result.returncode:log.flush();print((OUT/'commands.log').read_text());raise SystemExit(result.returncode)
assert source.read_text()==text,'Receiver source changed during extracted test; rerun on frozen source'
print('PASS',mode,'extracted Receiver queue/bind/PSA ownership/TLS cleanup fault cases')
