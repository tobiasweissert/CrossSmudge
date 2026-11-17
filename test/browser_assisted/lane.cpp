#include "HttpCore.h"
#include <cassert>
#include <cstring>
#include <string>
using namespace ink::http;
struct Wire {std::string response, sent;size_t at=0;int connects=0;};
static void check(int status,const char* type,size_t count,bool success,bool sameOrigin=false){
 Wire w{"HTTP/1.1 "+std::to_string(status)+" Test\r\nContent-Type: "+type+"\r\nContent-Length: "+std::to_string(count)+"\r\n"+(status>=300&&status<400?(sameOrigin?"Location: https://sticky.tobiasweissert.ch/v1/poll\r\n":"Location: https://example.org/leak\r\n"):"")+"\r\n"+std::string(count,'x')};
 Io io{&w,[](void* p,const char* host,uint32_t,const std::atomic<bool>&){assert(!strcmp(host,"sticky.tobiasweissert.ch"));((Wire*)p)->connects++;return true;},[](void* p,char* out,size_t n){auto& w=*(Wire*)p;n=std::min(n,w.response.size()-w.at);memcpy(out,w.response.data()+w.at,n);w.at+=n;return int(n);},[](void* p,const char* b,size_t n){((Wire*)p)->sent.append(b,n);return int(n);},[](void*){},[](void*){return 0u;}};
 Request q;assert(prepareBackend(q,"/v1/open","{}"));Result r;r.capacity=49152;Scratch scratch;std::atomic<bool> cancel{false};exchange(q,r,scratch,io,cancel);
 assert(w.connects==1);assert(w.sent.find("POST /v1/open HTTP/1.1")!=w.sent.npos);assert(w.sent.find("Authorization:")==w.sent.npos);assert(bool(!r.error)==success);
 if(status>=300 && status<400)assert(r.error && !strcmp(r.error,"redirect_denied"));
}
int main(){assert(!safeHeader("Authorization","Bearer synthetic"));Request q;assert(!prepareBackend(q,"/v1/open?x=1","{}"));assert(!prepareBackend(q,"/v1/open",""));for(int s=300;s<400;++s){check(s,"application/json",0,false);check(s,"application/json",0,false,true);}check(200,"application/json",49152,true);check(400,"application/json",2049,false);check(200,"text/html",1,false);check(202,"application/json; charset=utf-8",81,true);check(200,"application/json; charset=iso-8859-1",1,false);check(200,"application/jsonx",1,false);check(200,"application/json; charset=utf-8; other=yes",1,false);}
