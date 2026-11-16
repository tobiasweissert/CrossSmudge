#pragma once
#include "../core/Consent.h"
#include "../core/HttpRequest.h"
#include "../stream/StreamDecoder.h"
namespace airdrop::session {
struct Io {
  void* context;
  int (*read)(void*,uint8_t*,size_t); // 0 would-block, negative closed/error
  bool (*write)(void*,const uint8_t*,size_t,uint64_t deadline);
  bool (*alive)(void*);
  uint64_t (*now)(void*);
  void (*wait)(void*); // bounded cooperative wait; also rejects additional senders
};
struct Backend {
  void* context;
  bool (*cancelled)(void*);
  void (*pending)(void*,const Offer&,const OfferIdentity&,uint64_t);
  int (*decision)(void*,const OfferIdentity&); // 0 none, 1 accept, -1 decline
  bool (*beginFile)(void*,const Offer&);
  stream::WriteResult (*append)(void*,const uint8_t*,size_t);
  bool (*finishFile)(void*);
  void (*cancelFile)(void*);
  void (*progress)(void*,uint64_t,uint64_t);
  void (*urlAccepted)(void*,const Offer&,const OfferIdentity&);
  void (*failure)(void*,const char*);
};
// Allocated once by the admitted S3 owner, never on a TLS task stack.
struct Workspace {
  uint8_t* ask = nullptr; // MaxAskBytes caller-owned PSRAM; never archive storage
  OfferScratch graph;
  Offer offer;
  OfferIdentity identity;
  Consent consent;
  RequestParser parser;
  stream::Decoder decoder;
  stream::Manifest manifest;
  uint8_t input[2048]{};
  uint8_t replyBody[320]{};
  size_t begin=0,end=0,askBytes=0;
  uint64_t requestStart=0,deadline=0,lastProgress=0;
  Io* io=nullptr;
  Backend* backend=nullptr;
  const char* readError=nullptr;
  bool upload=false;
};
// One live TLS connection per invocation. Files require this same consenting TLS
// channel. Reconnects are deliberately not authorized from timing or peer address.
// Actual peer reconnect shapes require a separately qualified TransferID contract.
void serve(Workspace& w,Io& io,Backend& backend,const OfferIdentity& connection,uint64_t& offerSequence);
}
