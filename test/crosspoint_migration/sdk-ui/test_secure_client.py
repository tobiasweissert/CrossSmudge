#!/usr/bin/env python3
"""Compile complete overlaid SDK SecureClient TU against explicit TLS/TCP doubles.
This validates policy wiring/cleanup, NOT real cryptography or ESP32 execution.
"""
import os
from pathlib import Path
import subprocess
import tempfile
from test_overlay import load,SDK,REV
ROOT=Path(__file__).resolve().parents[3]
def main():
    m=load()
    with tempfile.TemporaryDirectory(prefix='sdk-ui-secure-',dir=os.environ['TMPDIR']) as td:
        d=Path(td)
        for p in m['SDK_SECURENET_PREIMAGES']:
            f=d/p;f.parent.mkdir(parents=True,exist_ok=True)
            f.write_bytes(subprocess.check_output(['git','--git-dir='+str(SDK),'show',REV+':'+p]))
        out=m['sdk_secure_net_outputs'](d)
        (d/'SecureClient.cpp').write_bytes(out['libs/network/SecureNet/src/SecureClient.cpp'])
        (d/'SecureClient.h').write_bytes(out['libs/network/SecureNet/include/SecureClient.h'])
        files={
        'Arduino.h':r'''#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
struct IPAddress { uint8_t operator[](int)const{return 1;} };
inline uint32_t millis(){return 10;} inline void delay(int){}
struct SerialType {explicit operator bool()const{return false;}template<typename... T>void printf(const char*,T...){}void println(const char*){}};inline SerialType Serial;
struct Esp {unsigned getFreeHeap(){return 50000;}unsigned getMaxAllocHeap(){return 20000;}};inline Esp ESP;
''',
        'Client.h':r'''#pragma once
#include "Arduino.h"
class Client {public:virtual ~Client()=default;virtual int connect(IPAddress,uint16_t)=0;virtual int connect(const char*,uint16_t)=0;virtual size_t write(uint8_t)=0;virtual size_t write(const uint8_t*,size_t)=0;virtual int available()=0;virtual int read()=0;virtual int read(uint8_t*,size_t)=0;virtual int peek()=0;virtual void flush()=0;virtual void stop()=0;virtual uint8_t connected()=0;virtual operator bool()=0;uint32_t getTimeout(){return 1000;} };
''',
        'WiFiClient.h':r'''#pragma once
#include "Arduino.h"
inline int numericConnects=0;
struct WiFiClient {void setConnectionTimeout(uint32_t){}int connect(IPAddress,uint16_t){++numericConnects;return 1;}int connect(const char*,uint16_t){return 1;}int connected(){return 1;}size_t write(const uint8_t*,size_t n){return n;}int read(uint8_t*,size_t){return 0;}int available(){return 0;}void stop(){} };
''',
        'Network.h':r'''#pragma once
#include "Arduino.h"
inline int dnsQueries=0;
struct Net {int hostByName(const char*,IPAddress&){++dnsQueries;return 1;}};inline Net Network;
''',
        'wolfssl/ssl.h':r'''#pragma once
#include <cstddef>
struct WOLFSSL{};struct WOLFSSL_CTX{};struct WOLFSSL_METHOD{};
constexpr int WOLFSSL_SUCCESS=1,WOLFSSL_VERIFY_NONE=0,WOLFSSL_VERIFY_PEER=1,WOLFSSL_FILETYPE_PEM=1,WOLFSSL_TLSV1_2=3,WOLFSSL_SNI_HOST_NAME=1;
constexpr int WOLFSSL_CBIO_ERR_CONN_CLOSE=-1,WOLFSSL_CBIO_ERR_WANT_WRITE=-2,WOLFSSL_CBIO_ERR_WANT_READ=-3,WOLFSSL_ERROR_WANT_READ=2,WOLFSSL_ERROR_WANT_WRITE=3,WOLFSSL_ERROR_ZERO_RETURN=4;
inline int rootOk=1,domainOk=1,floorOk=1,verify=-1,handshakes=0,contexts=0,sessions=0,attempts=0,failHandshake=0;
inline WOLFSSL_METHOD method;inline WOLFSSL_CTX ctx;inline WOLFSSL ssl;
inline void* wolfSSLv23_client_method(){return &method;}inline void* wolfTLSv1_2_client_method(){return &method;}
inline WOLFSSL_CTX* wolfSSL_CTX_new(WOLFSSL_METHOD*){++contexts;++attempts;return &ctx;}
inline void wolfSSL_CTX_free(WOLFSSL_CTX*){--contexts;}
inline int wolfSSL_CTX_SetMinVersion(WOLFSSL_CTX*,int version){return version==WOLFSSL_TLSV1_2?floorOk:0;}
inline void wolfSSL_CTX_set_verify(WOLFSSL_CTX*,int v,void*){verify=v;}
inline int wolfSSL_CTX_load_verify_buffer(WOLFSSL_CTX*,const unsigned char*,size_t,int){return rootOk;}
inline void wolfSSL_SetIORecv(WOLFSSL_CTX*,int(*)(WOLFSSL*,char*,int,void*)){}
inline void wolfSSL_SetIOSend(WOLFSSL_CTX*,int(*)(WOLFSSL*,char*,int,void*)){}
inline WOLFSSL* wolfSSL_new(WOLFSSL_CTX*){++sessions;return &ssl;}
inline void wolfSSL_free(WOLFSSL*){--sessions;}
inline void wolfSSL_SetIOReadCtx(WOLFSSL*,void*){}inline void wolfSSL_SetIOWriteCtx(WOLFSSL*,void*){}
inline int wolfSSL_UseSNI(WOLFSSL*,int,const char*,size_t){return 1;}
inline int wolfSSL_check_domain_name(WOLFSSL*,const char*){return domainOk;}
inline int wolfSSL_connect(WOLFSSL*){++handshakes;return failHandshake?0:1;}
inline int wolfSSL_get_error(WOLFSSL*,int){return -100;}
inline const char* wolfSSL_get_version(WOLFSSL*){return "fixture";}inline const char* wolfSSL_get_cipher(WOLFSSL*){return "fixture";}
inline int wolfSSL_write(WOLFSSL*,const void*,size_t n){return n;}inline int wolfSSL_read(WOLFSSL*,void*,size_t){return 0;}inline int wolfSSL_pending(WOLFSSL*){return 0;}
''',
        'main.cpp':r'''
#include "SecureClient.h"
#include <wolfssl/ssl.h>
#include <Network.h>
#include <cassert>
using freeink::SecureClient;
int main(){
 SecureClient c;
 assert(!c.connect("example.test",443)&&handshakes==0&&contexts==0&&sessions==0);
 c.setCACert("fixture CA");assert(c.connect("example.test",443)&&verify==WOLFSSL_VERIFY_PEER);assert(contexts==1&&sessions==1);
 c.setInsecure();assert(contexts==0&&sessions==0);assert(c.connect("example.test",443)&&verify==WOLFSSL_VERIFY_NONE);
 c.setCACert(nullptr);assert(!c.connected());handshakes=0;assert(!c.connect("example.test",443)&&handshakes==0);
 c.setCACert("fixture CA");rootOk=0;assert(!c.connect("example.test",443)&&contexts==0&&sessions==0&&handshakes==0);
 rootOk=1;domainOk=0;assert(!c.connect("example.test",443)&&handshakes==0&&contexts==0&&sessions==0);
 domainOk=1;floorOk=0;assert(!c.connect("example.test",443)&&handshakes==0&&contexts==0&&sessions==0);
 floorOk=1;failHandshake=1;attempts=0;assert(!c.connect("example.test",443)&&attempts==2&&contexts==0&&sessions==0);
 failHandshake=0;assert(c.connect("example.test",443));c.stop();assert(contexts==0&&sessions==0);
 dnsQueries=0;numericConnects=0;assert(c.connectPinned("example.test",IPAddress{},443));assert(numericConnects==1&&dnsQueries==0);c.stop();
 assert(c.connect("example.test",443));assert(dnsQueries==1);c.stop();
 c.setAbortCallback([]{return true;});handshakes=0;assert(!c.connect("example.test",443)&&handshakes==0&&c.aborted());
}
'''}
        for p,s in files.items():(d/p).parent.mkdir(parents=True,exist_ok=True);(d/p).write_text(s)
        cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-DFREEINK_NET_WOLFSSL=1','-I'+td,str(d/'SecureClient.cpp'),str(d/'main.cpp'),'-o',str(d/'probe')]
        if os.environ.get('SANITIZE')=='1':cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        subprocess.run(cmd,check=True);subprocess.run([str(d/'probe')],check=True)
        print('PASS: complete SDK SecureClient TU; peer policy, trust revocation, CA/domain/floor failures, TLS12 retry, cleanup, cancellation (doubles only)')
if __name__=='__main__':main()
