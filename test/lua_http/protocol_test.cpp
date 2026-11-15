#include "HttpCore.h"
#include <cassert>
#include <cstring>
#include <string>
#include <algorithm>
using namespace ink::http;
struct Peer {
 std::string response,sent;size_t at=0;bool closed=false;uint32_t now=0;
 static bool connect(void*,const char*,uint32_t,const std::atomic<bool>&){return true;}
 static int read(void* p,char* b,size_t n){auto& s=*static_cast<Peer*>(p);n=std::min({n,s.response.size()-s.at,size_t(3)});memcpy(b,s.response.data()+s.at,n);s.at+=n;return n;}
 static int write(void* p,const char* b,size_t n){auto& s=*static_cast<Peer*>(p);n=std::min(n,size_t(3));s.sent.append(b,n);return n;}
 static void close(void* p){static_cast<Peer*>(p)->closed=true;}
 static uint32_t millis(void* p){return static_cast<Peer*>(p)->now;}
};
int main(){
 Request q;assert(parseUrl("https://example.com/x",q.url));q.post=true;q.bodySize=3;memcpy(q.body,"a\0b",3);
 auto check=[&](std::string response,const char* error,const char* body){
  Peer p; p.response=response;Result r;Scratch scratch;std::atomic<bool> cancel{false};Io io{&p,Peer::connect,Peer::read,Peer::write,Peer::close,Peer::millis};
  exchange(q,r,scratch,io,cancel);assert(p.closed);
  assert((!error && !r.error) || (error && r.error && !strcmp(error,r.error)));
  if(!error)assert(std::string(r.body,r.size)==body);
  assert(p.sent==std::string("POST /x HTTP/1.1\r\nHost: example.com\r\nConnection: close\r\nAccept-Encoding: identity\r\nContent-Length: 3\r\n\r\na\0b",107));
 };
 check("HTTP/1.1 200 OK\r\n\r\nhello","protocol","");
 check("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello",nullptr,"hello");
 check("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2\r\nhe\r\n3\r\nllo\r\n0\r\n\r\n",nullptr,"hello");
 check("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nx","truncated","");
 check("HTTP/1.1 200 OK\r\nContent-Length: 9000\r\n\r\n","body_limit","");
 check("HTTP/1.1 200 OK\r\nContent-Length: 0\r\nTransfer-Encoding: chunked\r\n\r\n","protocol","");
 check("HTTP/1.1 302 Moved\r\nLocation: https://127.0.0.1/\r\nContent-Length: 0\r\n\r\n","redirect_denied","");
 check("HTTP/1.1 200 OK\r\nBig: "+std::string(600,'x')+"\r\n\r\n","header_limit","");
 std::string longest="https://"+std::string(63,'a')+"."+std::string(59,'b')+".com/"+std::string(376,'x');
 assert(longest.size()==512 && parseUrl(longest,q.url));
 Peer p;p.response="HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
 Result r;Scratch scratch;std::atomic<bool> cancel{false};Io io{&p,Peer::connect,Peer::read,Peer::write,Peer::close,Peer::millis};
 exchange(q,r,scratch,io,cancel);assert(!r.error);

}
