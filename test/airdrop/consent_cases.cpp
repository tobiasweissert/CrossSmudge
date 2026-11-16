#include "Offer.h"
#include "Consent.h"
#include "HttpRequest.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace airdrop;
namespace {
SinkResult sum(void* p,const uint8_t*,size_t n){ assert(n<=2048);*static_cast<uint64_t*>(p)+=n;return SinkResult::Consumed; }
SinkResult blocked(void*,const uint8_t*,size_t){return SinkResult::WouldBlock;}
HttpRequest request(std::string s,bool expect=true) {
  static RequestParser p;
  p=RequestParser{};
  HttpState result=HttpState::NeedMore;
  for(size_t i=0;i<s.size();++i){size_t consumed=99;result=p.feed(reinterpret_cast<const uint8_t*>(s.data()+i),1,consumed);assert(consumed==1);if(result!=HttpState::NeedMore){assert(i+1==s.size()||result==HttpState::Error);break;}}
  assert((result==HttpState::Ready)==expect);return p.request();
}
}
int main(){
  auto r=request("POST /Ask HTTP/1.1\r\nContent-Length: 2\r\nContent-Type: application/octet-stream\r\n\r\n");assert(r.route==Route::Ask&&r.length==2);
  const char* bad[]={
    "POST /AskExtra HTTP/1.1\r\nContent-Length: 0\r\n\r\n",
    "POST /Ask HTTP/1.1\r\nContent-Length: 0\r\nContent-Length: 0\r\n\r\n",
    "POST /Ask HTTP/1.1\r\nContent-Length: 0\r\nTransfer-Encoding: chunked\r\n\r\n",
    "POST /Ask HTTP/1.1\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n",
    "POST /Ask HTTP/1.1\r\nTransfer-Encoding: chunked\r\nTransfer-Encoding: chunked\r\n\r\n",
    "POST /Ask HTTP/1.1\r\nTransfer-Encoding: gzip, chunked\r\n\r\n",
    "POST /Ask HTTP/1.1\r\nContent-Length: -1\r\n\r\n",
    "POST /Ask HTTP/1.1\r\nContent-Length: +1\r\n\r\n",
    "POST /Ask HTTP/1.1\r\nContent-Length: 18446744073709551616\r\n\r\n",
    "POST /Ask HTTP/1.1\r\nContent-Length: 524289\r\n\r\n",
    "POST /Ask HTTP/1.1\r\n Content-Length: 0\r\n\r\n",
    "POST /Ask HTTP/1.1\nContent-Length: 0\n\n",
    "POST /Upload HTTP/1.1\r\nTransfer-Encoding: chunked\r\nTrailer: TransferID\r\n\r\n"};
  for(const char* s:bad)request(s,false);
  request("POST /Ask HTTP/1.1\r\nX-Long: "+std::string(1025,'a')+"\r\nContent-Length: 0\r\n\r\n",false);
  request("POST /Ask HTTP/1.1\r\nContent-Length: 524288\r\n\r\n");
  std::string many="POST /Ask HTTP/1.1\r\nContent-Length: 0\r\n";for(int i=0;i<64;++i)many+="X: y\r\n";many+="\r\n";request(many,false);
  std::string budget="POST /Ask HTTP/1.1\r\nContent-Length: 0\r\n";for(int i=0;i<20;++i)budget+="X: "+std::string(900,'a')+"\r\n";budget+="\r\n";request(budget,false);
  r=request("POST /Upload HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n");
  for(const std::string& b:std::vector<std::string>{"1z\r\na\r\n0\r\n\r\n","-1\r\na\r\n0\r\n\r\n","1;x=y\r\na\r\n0\r\n\r\n","0\r\nTransferID: x\r\n\r\n","10000000000000000\r\n","ffffffffffffffff\r\n"}) {
    BodyDecoder d(r);size_t n=0;uint64_t total=0;assert(d.feed(reinterpret_cast<const uint8_t*>(b.data()),b.size(),n,sum,&total)==BodyState::Error);assert(!d.complete());
  }
  const std::string chunk="3\r\nabc\r\n2\r\nde\r\n0\r\n\r\n";
  for(size_t split=0;split<=chunk.size();++split){BodyDecoder d(r);size_t n=0;uint64_t total=0;auto a=d.feed(reinterpret_cast<const uint8_t*>(chunk.data()),split,n,sum,&total);assert(a==BodyState::NeedMore||a==BodyState::Complete);assert(n==split);a=d.feed(reinterpret_cast<const uint8_t*>(chunk.data()+split),chunk.size()-split,n,sum,&total);assert(a==BodyState::Complete&&d.complete()&&total==5);}
  // Opt-in yields at each validated data+CRLF boundary, never an arbitrary feed end.
  for(size_t split=1;split<=chunk.size();++split){
    BodyDecoder d(r);size_t at=0;uint64_t total=0;unsigned boundaries=0;
    while(at<chunk.size()){
      size_t n=0;auto state=d.feed(reinterpret_cast<const uint8_t*>(chunk.data()+at),std::min(split,chunk.size()-at),n,sum,&total,true);at+=n;
      assert(n&&state!=BodyState::Error);
      if(state==BodyState::ChunkBoundary){++boundaries;assert(!d.complete());assert(at==(boundaries==1?8:15));}
    }
    assert(d.complete()&&boundaries==2&&total==5);
  }
  // Default callers require a full strict zero chunk, including its final LF.
  for(size_t end=8;end<chunk.size();++end){BodyDecoder d(r);size_t used=0;uint64_t total=0;assert(d.feed(reinterpret_cast<const uint8_t*>(chunk.data()),end,used,sum,&total)==BodyState::NeedMore);assert(!d.complete()&&used==end);}
  BodyDecoder backpressure(r);size_t n=0;uint64_t total=0;assert(backpressure.feed(reinterpret_cast<const uint8_t*>(chunk.data()),chunk.size(),n,blocked,nullptr)==BodyState::WouldBlock&&n==3);assert(backpressure.feed(reinterpret_cast<const uint8_t*>(chunk.data()+n),chunk.size()-n,n,sum,&total)==BodyState::Complete&&total==5);
  // A giant HTTP chunk is delivered through constant-sized slices, never staged whole.
  BodyDecoder huge(r,30000000,30001000);std::string header="1c9c380\r\n";assert(huge.feed(reinterpret_cast<const uint8_t*>(header.data()),header.size(),n,sum,&total)==BodyState::NeedMore);std::vector<uint8_t> block(8192,7);total=0;
  while(total<30000000){size_t amount=static_cast<size_t>(std::min<uint64_t>(block.size(),30000000-total));assert(huge.feed(block.data(),amount,n,sum,&total)==BodyState::NeedMore&&n==amount);}
  const char* end="\r\n0\r\n\r\n";assert(huge.feed(reinterpret_cast<const uint8_t*>(end),7,n,sum,&total)==BodyState::Complete&&total==30000000);
  BodyDecoder over(r,30000000);header="1c9c381\r\n";assert(over.feed(reinterpret_cast<const uint8_t*>(header.data()),header.size(),n,sum,&total)==BodyState::Error);
  static Offer o; o.kind=OfferKind::File;std::strcpy(o.transferId,"A3F34E01-1234-5678-ABCD-EF0123456789");
  OfferIdentity id{};id.generation=1;id.session=1;id.offer=1;id.connection=1;id.interfaceIndex=2;id.peer[15]=1;
  static Consent c;assert(!c.authorizeUpload(id,o.transferId,0));assert(c.begin(o,id,0,1));assert(c.state()==DecisionState::Pending);assert(!c.authorizeUpload(id,o.transferId,2));
  auto wrong=id;++wrong.offer;assert(!c.decide(wrong,true,3));assert(!c.begin(o,wrong,0,3));assert(c.decide(id,false,4));assert(!c.decide(id,true,5));
  assert(!c.begin(o,id,0,6)); // A resolved decision token can never be reused.
  ++id.offer;assert(c.begin(o,id,0,10));assert(!c.decide(id,true,30010));assert(c.state()==DecisionState::Expired);
  ++id.offer;assert(c.begin(o,id,39000,39999));assert(c.deadlineMs()==69999);c.disconnect(id.connection);assert(c.state()==DecisionState::Cancelled);assert(!c.decide(id,true,40000));
  ++id.offer;assert(c.begin(o,id,40000,40001));assert(c.decide(id,true,40002));wrong=id;++wrong.connection;assert(!c.authorizeUpload(wrong,o.transferId,40003));assert(!c.authorizeUpload(id,"wrong",40003));assert(c.authorizeUpload(id,o.transferId,40003));assert(!c.authorizeUpload(id,o.transferId,40004));c.completeUpload();assert(c.state()==DecisionState::Consumed);
  ++id.offer;o.transferId[0]=0;assert(c.begin(o,id,40004,40005));assert(c.decide(id,true,40006));wrong=id;++wrong.connection;assert(!c.authorizeUpload(wrong,"",40007,true));assert(c.authorizeUpload(id,"",40007));c.cancel();assert(c.state()==DecisionState::Cancelled);
  ++id.offer;o.kind=OfferKind::Url;assert(c.begin(o,id,50000,50001));assert(c.decide(id,true,50002));assert(!c.authorizeUpload(id,"",50003));assert(c.consumeUrl(id,50003));assert(!c.consumeUrl(id,50004));
  ++id.offer;assert(c.begin(o,id,50005,50006));c.cancel();assert(!c.consumeUrl(id,50007));
  std::puts("consent and HTTP production cases passed");
}
