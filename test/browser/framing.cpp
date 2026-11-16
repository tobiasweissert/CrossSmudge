#include "HttpCore.h"
#include <zlib.h>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
using namespace ink::http;
struct Peer{std::string response;size_t at=0;static bool connect(void*,const char*,uint32_t,const std::atomic<bool>&){return true;}static int read(void* p,char* b,size_t n){auto& s=*(Peer*)p;n=std::min({n,s.response.size()-s.at,size_t(3)});memcpy(b,s.response.data()+s.at,n);s.at+=n;return n;}static int write(void*,const char*,size_t n){return n;}static void close(void*){}static uint32_t millis(void*){return 0;}};
static std::string zipped(const std::string& input){z_stream z{};assert(deflateInit2(&z,6,Z_DEFLATED,31,8,Z_DEFAULT_STRATEGY)==Z_OK);std::string out(compressBound(input.size())+32,0);z.next_in=(Bytef*)input.data();z.avail_in=input.size();z.next_out=(Bytef*)out.data();z.avail_out=out.size();assert(deflate(&z,Z_FINISH)==Z_STREAM_END);out.resize(z.total_out);deflateEnd(&z);return out;}
int main(){
 Request q;q.browser=true;assert(parseUrl("https://example.org/a/b?keep=1",q.url));Scratch s;Result r;std::atomic<bool> cancel{false};std::string output;
 r.capacity=BrowserBody;r.sinkContext=&output;r.sink=[](void* p,const char* b,size_t n){((std::string*)p)->append(b,n);return true;};
 auto run=[&](const std::string& response){output.clear();Peer p{response};Io io{&p,Peer::connect,Peer::read,Peer::write,Peer::close,Peer::millis};exchange(q,r,s,io,cancel);};
 auto content=[&](std::string body,bool gzip){return std::string("HTTP/1.1 200 OK\r\nContent-Length: ")+std::to_string(body.size())+"\r\n"+(gzip?"Content-Encoding: gzip\r\n":"")+"\r\n"+body;};
 std::string chunks="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";for(int i=0;i<2048;++i)chunks+="1\r\nx\r\n";chunks+="0\r\nX-Test: yes\r\n\r\n";run(chunks);assert(!r.error&&output==std::string(2048,'x'));
 auto compressed=zipped(std::string(65536,'a'));run(content(compressed,true));assert(!r.error&&output==std::string(65536,'a'));
 std::string split="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Encoding: gzip\r\n\r\n";for(char c:compressed){split+="1;ok=yes\r\n";split+=c;split+="\r\n";}split+="0\r\n\r\n";run(split);assert(!r.error&&output.size()==65536);
 auto withCrc=compressed;withCrc[3]=2;unsigned headerCrc=crc32(0,(const Bytef*)withCrc.data(),10)&65535;withCrc.insert(10,std::string{char(headerCrc&255),char(headerCrc>>8)});run(content(withCrc,true));assert(!r.error && output.size()==65536);withCrc[10]^=1;run(content(withCrc,true));assert(r.error && std::string(r.error)=="gzip_header_crc");
 auto named=compressed;named[3]=8;named.insert(10,std::string(1025,'a')+'\0');run(content(named,true));assert(r.error && std::string(r.error)=="gzip_header");
 auto corrupt=compressed;corrupt.back()^=1;run(content(corrupt,true));assert(r.error&&std::string(r.error)=="gzip_checksum");
 run(content(compressed+compressed,true));assert(r.error&&std::string(r.error)=="gzip_trailing");
 run(content(compressed.substr(0,compressed.size()-3),true));assert(r.error);
 corrupt=compressed;corrupt[3]=char(0xe0);run(content(corrupt,true));assert(r.error&&std::string(r.error)=="gzip_header");
 q.limit=r.capacity=24576;run(content(compressed,true));assert(r.error&&std::string(r.error)=="body_limit");
 q.limit=r.capacity=BrowserBody;
 Url resolved;assert(resolveRedirect(q.url,"//news.example.org/done",resolved)&&std::string(resolved.host)=="news.example.org");assert(resolveRedirect(q.url,"../done",resolved)&&std::string(resolved.path)=="/done");assert(resolveRedirect(q.url,"#anchor",resolved)&&std::string(resolved.path)=="/a/b?keep=1");assert(resolveRedirect(q.url,"/a//c/./d/../e",resolved)&&std::string(resolved.path)=="/a//c/e");assert(!resolveRedirect(q.url,"https://127.0.0.1/",resolved));
 r.sink=[](void*,const char*,size_t){return false;};run(content("fail",false));assert(r.error&&std::string(r.error)=="storage_write");
 puts("PASS real framing/gzip CRC/ISIZE/truncation/trailing/bomb/resource bounds/redirect references");
}
