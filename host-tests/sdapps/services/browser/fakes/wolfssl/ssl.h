#pragma once
// Controlled boundary fake, NOT a TLS implementation or certificate test.
#include "Arduino.h"
#include <algorithm>
#include <cstring>
struct WOLFSSL{};struct WOLFSSL_CTX{};struct WOLFSSL_METHOD{};
constexpr int WOLFSSL_SUCCESS=1,WOLFSSL_VERIFY_PEER=1,WOLFSSL_FILETYPE_PEM=1,WOLFSSL_SNI_HOST_NAME=0,WOLFSSL_TLSV1_2=3;
constexpr int WOLFSSL_CBIO_ERR_WANT_WRITE=-2,WOLFSSL_CBIO_ERR_WANT_READ=-3,WOLFSSL_CBIO_ERR_CONN_CLOSE=-4;
constexpr int WOLFSSL_ERROR_WANT_READ=2,WOLFSSL_ERROR_WANT_WRITE=3,WOLFSSL_ERROR_ZERO_RETURN=6;
inline void wolfSSL_Init(){}
inline WOLFSSL_METHOD* wolfSSLv23_client_method(){static WOLFSSL_METHOD m;return &m;}
inline WOLFSSL_CTX* wolfSSL_CTX_new(WOLFSSL_METHOD*){return new WOLFSSL_CTX;}
inline void wolfSSL_CTX_free(WOLFSSL_CTX* p){delete p;}
inline void wolfSSL_free(WOLFSSL* p){++fixture.frees;delete p;}
inline void wolfSSL_CTX_set_verify(WOLFSSL_CTX*,int verify,void*){fixture.verify=verify;}
inline int wolfSSL_CTX_SetMinVersion(WOLFSSL_CTX*,int v){fixture.minimum=v;return 1;}
inline int wolfSSL_CTX_load_verify_buffer(WOLFSSL_CTX*,const unsigned char* cert,size_t n,int){fixture.trust=n>500&&std::strstr(reinterpret_cast<const char*>(cert),"BEGIN CERTIFICATE");return fixture.trust?1:0;}
using Callback=int(*)(WOLFSSL*,char*,int,void*);
inline void wolfSSL_SetIORecv(WOLFSSL_CTX*,Callback){}
inline void wolfSSL_SetIOSend(WOLFSSL_CTX*,Callback){}
inline WOLFSSL* wolfSSL_new(WOLFSSL_CTX*){return new WOLFSSL;}
inline void wolfSSL_SetIOReadCtx(WOLFSSL*,void*){}
inline void wolfSSL_SetIOWriteCtx(WOLFSSL*,void*){}
inline int wolfSSL_check_domain_name(WOLFSSL*,const char* name){fixture.domain=name;return 1;}
inline int wolfSSL_UseSNI(WOLFSSL*,int,const char* name,size_t n){fixture.sni.assign(name,n);return 1;}
inline int wolfSSL_connect(WOLFSSL*){
 ++fixture.handshakes;
 return fixture.minimum==WOLFSSL_TLSV1_2&&fixture.verify==WOLFSSL_VERIFY_PEER&&fixture.trust&&!fixture.badChain&&!fixture.badHost&&!fixture.domain.empty()&&fixture.domain==fixture.sni?1:-1;
}
inline int wolfSSL_get_error(WOLFSSL*,int){return 99;}
inline const char* wolfSSL_get_version(WOLFSSL*){return "TLSv1.3";}
inline int wolfSSL_write(WOLFSSL*,const char* p,size_t n){fixture.request.append(p,n);return n;}
inline int wolfSSL_read(WOLFSSL*,char* p,size_t n){n=std::min(n,fixture.response.size()-fixture.read);memcpy(p,fixture.response.data()+fixture.read,n);fixture.read+=n;return n?n:-1;}
