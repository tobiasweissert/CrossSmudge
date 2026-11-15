#include "DownloadTransfer.h"
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <algorithm>
#include <cstdio>
using namespace ink::http;
#include "download_fakes.h"
static std::string response(const std::string& body) {return "HTTP/1.1 200 OK\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;}
static void transfer(Files& f,Wire& w,Job& j) {assert(parseUrl("https://example.com/archive",j.request.url));j.download=true;j.maxBytes=MaxDownload;auto io=w.io();exchangeDownload(j,io,f);}
static std::string read(Files& f) {
  DownloadStore s(f);char buf[1024];size_t n=0,total=0;std::string all;
  do {assert(!s.readAt(all.size(),buf,sizeof(buf),n,total));all.append(buf,n);}while(n);
  assert(total==all.size());return all;
}
int main() {
  Files base;Wire first;first.response=response("old saved archive");Job initial;transfer(base,first,initial);
  assert(initial.committed && !initial.result.error && !base.data.count("part"));
  for(int fault=0;fault<12;++fault) {
    Files f=base;Wire w;Job j;w.response=response(std::string(9000,'n'));
    if(fault==0)f.shortWrite=true;
    if(fault==1)f.syncFail=true;
    if(fault==2)f.closeFail=true;
    if(fault==3)f.renameFail=true;
    if(fault==4)w.stop=2000;
    if(fault==5){w.cancellation=&j.cancel;w.trigger=2000;}
    if(fault==6){w.removal=&f;w.trigger=2000;}
    if(fault==7)w.trusted=false;
    if(fault==8)j.request.timeoutMs=10;
    if(fault==9)f.cancelOnSync=&j.cancel;
    if(fault==10)w.response="HTTP/1.1 404 Missing\r\nContent-Length: 3\r\n\r\nbad";
    if(fault==11)f.clockOnSync=&w.now;
    transfer(f,w,j);assert(j.result.error && !j.committed);
    if(fault==7)assert(w.sent.empty());
    f.removed=f.closeFail=false;f.cancelOnSync=nullptr;
    assert(read(f)=="old saved archive");
    if(fault!=6)assert(!f.data.count("part"));
    // Removal can prevent cleanup; next begin removes only the stale private part.
    f.shortWrite=f.syncFail=f.renameFail=false;
    DownloadStore retry(f);assert(retry.begin());retry.abort();assert(!f.data.count("part"));
  }
  Files large=base;Wire w;Job j;w.response=response(std::string(MaxDownload,'x'));transfer(large,w,j);
  assert(j.committed && j.result.size==MaxDownload && large.largest<=DownloadChunk);
  assert(read(large)==std::string(MaxDownload,'x'));
  Wire over;Job bad;over.response=response(std::string(MaxDownload+1,'z'));transfer(large,over,bad);
  assert(bad.result.error && !strcmp(bad.result.error,"body_limit"));assert(read(large).size()==MaxDownload);
  Files chunks=base;Wire c;Job cj;c.response="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n400\r\n"+std::string(1024,'b')+"\r\n3\r\nxyz\r\n0\r\n\r\n";
  transfer(chunks,c,cj);assert(cj.committed && read(chunks)==std::string(1024,'b')+"xyz");
  for(const char* framing:{"0\r\nTrailer: no\r\n\r\n","1;ext=bad\r\nx\r\n0\r\n\r\n","5\r\nxy"}) {
    Files f=base;Wire wire;Job job;wire.response="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"+std::string(framing);
    transfer(f,wire,job);assert(job.result.error && read(f)=="old saved archive");
  }
  // Existing malformed generation must never be mistaken for absent and overwritten.
  Files corrupt=base;corrupt.data["1"]="bad";auto original=corrupt.data;DownloadStore s(corrupt);assert(!s.begin());assert(original==corrupt.data);
  Files uncertain=base;uncertain.metadataFail=true;auto saved=uncertain.data;DownloadStore uncertainStore(uncertain);assert(!uncertainStore.begin());assert(uncertain.data==saved);
  puts("PASS download: 2MiB bounded transfer, chunk framing, TLS-before-bytes, 12 failure/cancel cases, old-file preservation, corrupt-generation refusal");
}
