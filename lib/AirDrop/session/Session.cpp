#include "Session.h"
#include "../vendor/ESP32Drop/src/airdrop/core/ad_bplist.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
namespace airdrop::session {
namespace {
uint64_t now(Workspace& w){return w.io->now(w.io->context);}
bool stopping(Workspace& w){return w.backend->cancelled(w.backend->context);}
bool more(Workspace& w){
  // Measure only a refill's wait for the peer, not staging, decode, SD or
  // display-lock work performed since the previous successful read. The
  // absolute request deadline still includes all of that local work.
  if(w.begin==w.end)w.lastProgress=now(w);
  while(w.begin==w.end){
    if(stopping(w)){w.readError="cancelled";return false;}
    if(now(w)>=w.deadline){w.readError="absolute deadline";return false;}
    if(now(w)-w.lastProgress>=15000){w.readError="15-second input stall";return false;}
    if(!w.io->alive(w.io->context)){w.readError="peer disconnected";return false;}
    w.begin=w.end=0;int n=w.io->read(w.io->context,w.input,sizeof w.input);
    if(n<0){w.readError="TLS read closed or failed";return false;}
    if(n>0){w.end=size_t(n);w.lastProgress=now(w);return true;}
    w.io->wait(w.io->context);
  }
  return !stopping(w)&&now(w)<w.deadline;
}
bool live(Workspace& w){return !stopping(w)&&now(w)<w.deadline&&w.io->alive(w.io->context);}
bool response(Workspace& w,int code,bool identity=false,bool close=false){
  const char* reason=code==200?"OK":code==403?"Forbidden":code==413?"Payload Too Large":code==507?"Insufficient Storage":code==408?"Request Timeout":"Bad Request";
  uint32_t size=0;
  if(identity){
    const AdBplistKV fields[]={{"ReceiverComputerName",AD_BP_STR,"CrossSmudge Sticky",0},{"ReceiverModelName",AD_BP_STR,"reTerminal Sticky",0}};
    size=ad_bplist_dict(w.replyBody,sizeof w.replyBody,fields,2);if(!size)return false;
  }
  char header[192]; // Fixed response headers only; payload lives in session-owned storage.
  int n=snprintf(header,sizeof header,"HTTP/1.1 %d %s\r\nContent-Length: %lu\r\nContent-Type: application/octet-stream\r\nConnection: %s\r\n\r\n",code,reason,(unsigned long)size,code==200&&!close?"keep-alive":"close");
  return n>0&&size_t(n)<sizeof header&&w.io->write(w.io->context,reinterpret_cast<const uint8_t*>(header),size_t(n),w.deadline)&&(!size||w.io->write(w.io->context,w.replyBody,size,w.deadline));
}
SinkResult metadata(void* context,const uint8_t* p,size_t n){auto& w=*static_cast<Workspace*>(context);if(n>MaxAskBytes-w.askBytes)return SinkResult::Error;if(w.parser.request().route==Route::Ask)memcpy(w.ask+w.askBytes,p,n);w.askBytes+=n;return SinkResult::Consumed;}
bool cancelled(void* context){auto& w=*static_cast<Workspace*>(context);return !live(w);}
stream::WriteResult append(void* context,const uint8_t* p,size_t n){auto& w=*static_cast<Workspace*>(context);if(!live(w))return stream::WriteResult::Error;return w.backend->append(w.backend->context,p,n);}
SinkResult archive(void* context,const uint8_t* p,size_t n){
  auto& w=*static_cast<Workspace*>(context);size_t at=0;
  while(at<n){if(!live(w))return SinkResult::Error;size_t consumed=0;auto result=w.decoder.feed(p+at,n-at,consumed);at+=consumed;
    if(result==stream::Status::Error)return SinkResult::Error;
    if(result==stream::Status::WouldBlock){w.io->wait(w.io->context);continue;}
    if(!consumed&&at<n)return SinkResult::Error;
  }
  // SD writes already yield. Waiting 10ms for every buffered span needlessly
  // delays draining the small TCP receive window during an AWDL radio burst.
  return SinkResult::Consumed;
}
bool body(Workspace& w,bool upload){
  BodyDecoder decoder(w.parser.request(),upload?stream::kMaxEncoded:MaxAskBytes,upload?42000000:MaxAskBytes+65536);
  w.askBytes=0;
  while(!decoder.complete()){
    if(!more(w)){
      if(upload){char detail[192];snprintf(detail,sizeof detail,"Upload input stopped: %s; wire=%llu body=%llu file=%u",w.readError?w.readError:"session expired",(unsigned long long)decoder.wireBytes(),(unsigned long long)decoder.payloadBytes(),unsigned(w.decoder.payloadBytes()));w.backend->failure(w.backend->context,detail);}
      return false;
    }size_t consumed=0;auto state=decoder.feed(w.input+w.begin,w.end-w.begin,consumed,upload?archive:metadata,&w,upload);w.begin+=consumed;
    if(upload)w.backend->progress(w.backend->context,decoder.wireBytes(),w.decoder.payloadBytes());
    if(state==BodyState::Error){if(upload)w.backend->failure(w.backend->context,"Upload HTTP body or archive rejected");return false;}
    // Native compatibility cutoff: the first complete consented archive at a
    // validated nonzero HTTP chunk DATA-END+CRLF. feed stops AT that boundary,
    // so later chunks/bytes are abandoned whether coalesced or still unread.
    // serve returns after a closing response; Receiver closes this TLS socket.
    // Content-Length never uses this exception and must consume its full body.
    if(upload&&state==BodyState::ChunkBoundary&&w.decoder.archiveComplete())return true;
    if(state==BodyState::WouldBlock)w.io->wait(w.io->context);
  }
  return true;
}
void error(Workspace& w,const char* text,int code=400){w.backend->failure(w.backend->context,text);(void)response(w,code);}
}
void serve(Workspace& w,Io& io,Backend& backend,const OfferIdentity& connection,uint64_t& offerSequence){
  w.io=&io;w.backend=&backend;w.begin=w.end=0;w.upload=false;
  for(unsigned requests=0;requests<8&&!stopping(w);++requests){
    w.requestStart=now(w);w.deadline=w.requestStart+40000;w.lastProgress=w.requestStart;w.readError=nullptr;
    // Reconstruct in caller-owned storage rather than creating a 1KiB temporary on stack.
    w.parser.~RequestParser();new (&w.parser) RequestParser;
    HttpState state=HttpState::NeedMore;
    while(state==HttpState::NeedMore){if(!more(w))goto closed;size_t consumed=0;state=w.parser.feed(w.input+w.begin,w.end-w.begin,consumed);w.begin+=consumed;}
    if(state!=HttpState::Ready){error(w,"invalid HTTP framing");goto closed;}
    if(w.parser.request().route==Route::Discover){
      if(w.parser.request().expectContinue){error(w,"unsupported metadata Expect");goto closed;}
      if(!body(w,false)){error(w,"incomplete discovery metadata");goto closed;}
      // macOS opens a separate Ask connection after discovery. Release this
      // idle channel so the single TLS owner can accept it immediately.
      (void)response(w,200,true,true);goto closed;
    }
    if(w.parser.request().route==Route::Ask){
      if(w.parser.request().expectContinue){error(w,"unsupported Ask Expect");goto closed;}
      if(!body(w,false)){error(w,"incomplete Ask metadata");goto closed;}
      ParseError parsed;
      if(!parseOffer(w.ask,w.askBytes,w.offer,w.graph,parsed)){
        char reason[80];
        snprintf(reason,sizeof reason,"unsupported or invalid offer (reason=%u, bytes=%u)",unsigned(parsed),unsigned(w.askBytes));
        error(w,reason,403);goto closed;
      }
      w.identity=connection;w.identity.offer=++offerSequence;
      if(!w.consent.begin(w.offer,w.identity,w.requestStart,now(w))){error(w,"offer expired or busy",403);goto closed;}
      backend.pending(backend.context,w.offer,w.identity,w.consent.deadlineMs());
      while(w.consent.state()==DecisionState::Pending){
        if(!live(w)){w.consent.cancel();goto closed;}
        int decision=backend.decision(backend.context,w.identity);
        if(decision)w.consent.decide(w.identity,decision>0,now(w));
        w.consent.tick(now(w));
        if(w.consent.state()==DecisionState::Pending)io.wait(io.context);
      }
      // Decision delivery can race a disconnect or a deadline while the UI
      // paints. Revalidate the live channel at the actual acceptance boundary.
      w.consent.tick(now(w));
      if(!live(w)){w.consent.cancel();goto closed;}
      if(w.consent.state()!=DecisionState::Accepted){(void)response(w,403);goto closed;}
      // Reserve the one-use URL while consent is live. Its bounded positive
      // acknowledgement may finish after the decision deadline; that must not
      // discard a URL which the peer has already been told was accepted.
      const bool url = w.offer.kind==OfferKind::Url;
      if(url&&!w.consent.consumeUrl(w.identity,now(w)))goto closed;
      if(!response(w,200,true))goto closed;
      if(url){backend.urlAccepted(backend.context,w.offer,w.identity);goto closed;}
      // Keep the exact consenting TLS channel live; no global "accepted recently" flag.
      continue;
    }
    if(w.parser.request().route==Route::Upload){
      w.identity=connection;w.identity.offer=w.consent.identity().offer;
      if(!w.consent.authorizeUpload(w.identity,w.parser.request().transferId,now(w))){error(w,"upload has no live approval",403);goto closed;}
      const auto& request=w.parser.request();
      // MIME metadata is not authoritative for AirDrop's DVZip envelope.
      // Validate the actual bounded archive below, including its exact manifest.
      w.deadline=w.requestStart+1800000;
      if(!backend.beginFile(backend.context,w.consent.offer())){error(w,"cannot stage file",507);goto closed;}
      w.upload=true;
      memcpy(w.manifest.name,w.consent.offer().filename,sizeof w.manifest.name);memcpy(w.manifest.archivePath,w.consent.offer().archivePath,sizeof w.manifest.archivePath);w.manifest.size=w.consent.offer().fileSize;
      if(!w.decoder.reset(w.manifest,{&w,append,cancelled})){error(w,"invalid archive manifest");goto closed;}
      if(request.expectContinue){static const uint8_t ok[]="HTTP/1.1 100 Continue\r\n\r\n";if(!io.write(io.context,ok,sizeof ok-1,w.deadline))goto closed;}
      if(!body(w,true)||w.decoder.finish()!=stream::Status::Complete){error(w,w.decoder.error()?w.decoder.error():"invalid or incomplete upload");goto closed;}
      if(!backend.finishFile(backend.context)){error(w,"SD publication failed or uncertain",507);goto closed;}
      w.upload=false;w.consent.completeUpload();
      // Publication is durable, but never report a fresh positive response on
      // a channel which died or exceeded the absolute receive budget in finish.
      // Keep the published book; failure here is not permission to roll it back.
      if(!live(w)||!response(w,200,false,true))backend.failure(backend.context,"file committed; final sender acknowledgement not delivered");
      return;
    }
    error(w,"unsupported request");goto closed;
  }
closed:
  w.consent.disconnect(connection.connection);
  if(w.upload){backend.cancelFile(backend.context);w.upload=false;}
}
}
