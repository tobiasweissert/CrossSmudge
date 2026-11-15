#include "BrowserFetch.h"
#if defined(FREEINK_DEVICE_STICKY)
#include "BrowserPolicy.h"
#include "BrowserTrust.h"
#include <Arduino.h>
#include <WiFiClient.h>
#include <TrustedTime.h>
#include <lwip/netdb.h>
#include <wolfssl/ssl.h>
#if defined(NO_ASN_TIME)
#error Browser TLS requires certificate validity-date verification
#endif
#include <cstring>
#include <ctime>
namespace stickyweb {
namespace {
// Original pinned transport adapter. The SDK SecureClient cannot bind an
// independently vetted DNS answer and does not enable hostname checking.
struct Session {
  WiFiClient tcp;
  WOLFSSL_CTX* ctx=nullptr;
  WOLFSSL* ssl=nullptr;
  ~Session() { if(ssl) wolfSSL_free(ssl); if(ctx) wolfSSL_CTX_free(ctx); tcp.stop(); }
};
int sendTls(WOLFSSL*,char* data,int size,void* user) {
  auto& tcp=*static_cast<WiFiClient*>(user);
  int n=tcp.write(reinterpret_cast<const uint8_t*>(data),size);
  return n>0 ? n : (tcp.connected() ? WOLFSSL_CBIO_ERR_WANT_WRITE : WOLFSSL_CBIO_ERR_CONN_CLOSE);
}
int receiveTls(WOLFSSL*,char* data,int size,void* user) {
  auto& tcp=*static_cast<WiFiClient*>(user);
  if(!tcp.available()) return tcp.connected() ? WOLFSSL_CBIO_ERR_WANT_READ : WOLFSSL_CBIO_ERR_CONN_CLOSE;
  int n=tcp.read(reinterpret_cast<uint8_t*>(data),size);
  return n>0 ? n : WOLFSSL_CBIO_ERR_WANT_READ;
}
bool wantIo(int error) { return error==WOLFSSL_ERROR_WANT_READ || error==WOLFSSL_ERROR_WANT_WRITE; }
}
HttpResponse fetchOnce(const std::string& url,const std::function<bool()>& cancelled,uint32_t budgetMs) {
  HttpResponse failure;
  auto fail=[&](const char* message) { failure.error=message; return failure; };
  Destination dest;
  if(!parseDestination(url,dest)) return fail("Only public HTTPS DNS names on port 443 are allowed");
  const int64_t trusted=trustedtime::trustedNow();
  if(!trusted || static_cast<int64_t>(std::time(nullptr))<trusted)
    return fail("Clock untrusted or rolled back: synchronize time in Settings");
  const uint32_t started=millis();
  bool userCancelled=false;
  auto stop=[&] { userCancelled=cancelled(); return userCancelled || millis()-started>=budgetMs; };
  if(stop()) return fail("Cancelled");
  // Resolve IPv4 only; inspect every answer returned, then connect by numeric
  // IP. The TLS SNI and certificate identity remain the original DNS name.
  addrinfo hints{}; hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;
  addrinfo* answers=nullptr;
  if(getaddrinfo(dest.host.c_str(),"443",&hints,&answers)!=0 || !answers) return fail("DNS lookup failed");
  uint32_t chosen=0; bool allowed=true;
  for(auto* a=answers;a;a=a->ai_next) {
    if(a->ai_family!=AF_INET || a->ai_addrlen<sizeof(sockaddr_in)) { allowed=false; break; }
    uint32_t ip=ntohl(reinterpret_cast<sockaddr_in*>(a->ai_addr)->sin_addr.s_addr);
    if(!publicIpv4(ip)) { allowed=false; break; }
    if(!chosen) chosen=ip;
  }
  freeaddrinfo(answers);
  if(!allowed || !chosen) return fail("Destination is not a public IPv4 address");
  if(stop()) return fail(userCancelled ? "Cancelled" : "Request timed out");
  Session s;
  s.tcp.setConnectionTimeout(1000);
  s.tcp.setTimeout(100);
  if(!s.tcp.connect(IPAddress(chosen>>24,(chosen>>16)&255,(chosen>>8)&255,chosen&255),443)) return fail("TCP connection failed");
  if(stop()) return fail(userCancelled ? "Cancelled" : "Request timed out");
  wolfSSL_Init();
  s.ctx=wolfSSL_CTX_new(wolfSSLv23_client_method());
  if(!s.ctx) return fail("TLS allocation failed");
  if(wolfSSL_CTX_SetMinVersion(s.ctx,WOLFSSL_TLSV1_2)!=WOLFSSL_SUCCESS) return fail("TLS minimum version configuration failed");
  wolfSSL_CTX_set_verify(s.ctx,WOLFSSL_VERIFY_PEER,nullptr);
  for(const char* root:kBrowserRoots)
    if(wolfSSL_CTX_load_verify_buffer(s.ctx,reinterpret_cast<const unsigned char*>(root),strlen(root),WOLFSSL_FILETYPE_PEM)!=WOLFSSL_SUCCESS) return fail("Trust anchor unavailable");
  wolfSSL_SetIORecv(s.ctx,receiveTls); wolfSSL_SetIOSend(s.ctx,sendTls);
  s.ssl=wolfSSL_new(s.ctx);
  if(!s.ssl) return fail("TLS allocation failed");
  wolfSSL_SetIOReadCtx(s.ssl,&s.tcp); wolfSSL_SetIOWriteCtx(s.ssl,&s.tcp);
  if(wolfSSL_check_domain_name(s.ssl,dest.host.c_str())!=WOLFSSL_SUCCESS || wolfSSL_UseSNI(s.ssl,WOLFSSL_SNI_HOST_NAME,dest.host.c_str(),dest.host.size())!=WOLFSSL_SUCCESS) return fail("TLS hostname configuration failed");
  int n;
  while((n=wolfSSL_connect(s.ssl))!=WOLFSSL_SUCCESS) {
    if(!wantIo(wolfSSL_get_error(s.ssl,n))) return fail("TLS verification or handshake failed (ISRG X1 / GTS roots; check trusted clock)");
    if(stop()) return fail(userCancelled ? "Cancelled" : "Request timed out");
    delay(5);
  }
  // Explicit minimum: do not allow the legacy versions v23 can theoretically negotiate.
  const char* version=wolfSSL_get_version(s.ssl);
  if(strcmp(version,"TLSv1.2")!=0 && strcmp(version,"TLSv1.3")!=0) return fail("TLS version below 1.2 rejected");
  std::string request="GET "+dest.target+" HTTP/1.0\r\nHost: "+dest.host+"\r\nUser-Agent: StickyWeb/0.1\r\nAccept: text/html, application/xhtml+xml\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n";
  size_t sent=0;
  while(sent<request.size()) {
    if(stop()) return fail(userCancelled ? "Cancelled" : "Request timed out");
    n=wolfSSL_write(s.ssl,request.data()+sent,request.size()-sent);
    if(n>0) sent+=n;
    else if(!wantIo(wolfSSL_get_error(s.ssl,n))) return fail("Request write failed");
    else delay(5);
  }
  std::string raw; raw.reserve(4096);
  bool cleanClose=false;
  char buffer[192];
  while(true) {
    if(stop()) return fail(userCancelled ? "Cancelled" : "Request timed out");
    n=wolfSSL_read(s.ssl,buffer,sizeof(buffer));
    if(n>0) {
      if(raw.size()+n>kMaxBody+8196) return fail("Response exceeds bounded download limit");
      raw.append(buffer,n);
      // Framed responses need not wait for a server to close its socket.
      auto decoded=decodeResponse(raw,false);
      if(decoded.ok) {
        if(stop()) return fail(userCancelled ? "Cancelled" : "Request timed out");
        return decoded;
      }
    } else {
      int error=wolfSSL_get_error(s.ssl,n);
      if(error==WOLFSSL_ERROR_ZERO_RETURN) { cleanClose=true; break; }
      if(!wantIo(error)) break;
      delay(5);
    }
  }
  return decodeResponse(raw,cleanClose);
}
}
#endif
