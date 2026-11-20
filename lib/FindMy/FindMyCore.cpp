#include "FindMyCore.h"
#include <cstring>
#include <limits>
namespace findmy {
Status Provision::parse(const uint8_t* master, size_t masterSize, const uint8_t* seed,
                        size_t seedSize, uint64_t index, uint64_t epoch, Provision& out) {
  out=Provision{};
  if(!cryptoAvailable()) return Status::CryptoUnavailable;
  if(!master || !seed || masterSize!=28 || seedSize!=32 || !epoch) return Status::InvalidKey;
  MasterKey d{}; std::memcpy(d.data(),master,28);
  bool valid=validMaster(d); secureErase(d.data(),d.size());
  if(!valid) return Status::InvalidKey;
  std::memcpy(out.master.data(),master,28); std::memcpy(out.primarySeed.data(),seed,32);
  out.initialIndex=index; out.epochSeconds=epoch; return Status::Ok;
}
Core::~Core() = default;
Status Core::fail(Status s) {
  ready_=false; frame_=Frame{}; public_.fill(0); status_=s; return s;
}
bool Core::currentFrame(Frame& out) const {
  out=ready_?frame_:Frame{}; return ready_;
}
Status Core::publish(Checkpoint& next, const Time& t) {
  PublicKey x{};
  if(!derivePublic(provision_.master,next.seed,x,rng_,rngContext_)) return fail(Status::CryptoFailure);
  next.lastWallSeconds=t.unixSeconds; next.publicKey=x;
  if(!persistence_->commit(next)) { provisioned_=false; return fail(Status::PersistenceError); }
  state_=next; public_=x; frame_=encodeFrame(x); ready_=true; status_=Status::Ok; lastTime_=t; anchorTime_=t;
  return status_;
}
Status Core::init(const Provision& p, Persistence& store, const Time& t, bool allowFresh) {
  provisioned_=false; fail(Status::NotProvisioned); persistence_=&store;
  if(!cryptoAvailable()) return fail(Status::CryptoUnavailable);
  if(!validMaster(p.master) || !p.epochSeconds) return fail(Status::InvalidKey);
  if(!t.trusted || t.unixSeconds<p.epochSeconds) return fail(Status::UntrustedTime);
  provision_=p;
  Checkpoint next;
  Load loaded=store.load(next);
  if(loaded!=Load::Ok && loaded!=Load::Empty) return fail(Status::PersistenceError);
  uint64_t delta=(t.unixSeconds-p.epochSeconds)/RotationSeconds;
  if(delta>std::numeric_limits<uint64_t>::max()-p.initialIndex) return fail(Status::IndexOverflow);
  const uint64_t target=p.initialIndex+delta;
  PublicKey identity{};
  if(!derivePublic(p.master,p.primarySeed,identity,rng_,rngContext_)) return fail(Status::CryptoFailure);
  if(loaded==Load::Ok) {
    if(next.version!=1 || next.identity!=identity || next.initialIndex!=p.initialIndex ||
       next.epochSeconds!=p.epochSeconds || next.index<p.initialIndex || next.lastWallSeconds<p.epochSeconds)
      return fail(Status::InvalidState);
    if(t.unixSeconds<next.lastWallSeconds || target<next.index) return fail(Status::ClockRollback);
    if(next.index-p.initialIndex!=(next.lastWallSeconds-p.epochSeconds)/RotationSeconds)
      return fail(Status::InvalidState);
    PublicKey persistedPublic{};
    if(!derivePublic(p.master,next.seed,persistedPublic,rng_,rngContext_)) return fail(Status::CryptoFailure);
    if(persistedPublic!=next.publicKey) return fail(Status::InvalidState);
    if(target==next.index) return fail(Status::ReusedIndex);
  } else {
    if(!allowFresh) return fail(Status::MissingState);
    next=Checkpoint{}; next.seed=p.primarySeed; next.initialIndex=p.initialIndex;
    next.epochSeconds=p.epochSeconds; next.index=p.initialIndex; next.identity=identity;
  }
  if(target-next.index>MaxCatchup) return fail(Status::CatchupLimit);
  while(next.index<target) {
    if(!updateSeed(next.seed,next.seed)) return fail(Status::CryptoFailure);
    ++next.index;
  }
  state_=next; lastTime_=t; provisioned_=true;
  return publish(next,t);
}
Status Core::rotate(const Time& t) {
  if(!provisioned_) return fail(Status::NotProvisioned);
  if(!t.trusted || t.unixSeconds<provision_.epochSeconds) return fail(Status::UntrustedTime);
  if(t.unixSeconds<lastTime_.unixSeconds || t.monotonicMs<lastTime_.monotonicMs) return fail(Status::ClockRollback);
  const uint64_t elapsed=(t.monotonicMs-anchorTime_.monotonicMs)/1000;
  const uint64_t wallElapsed=t.unixSeconds-anchorTime_.unixSeconds;
  if(elapsed>wallElapsed && elapsed-wallElapsed>2) return fail(Status::UntrustedTime);
  uint64_t delta=(t.unixSeconds-provision_.epochSeconds)/RotationSeconds;
  if(delta>std::numeric_limits<uint64_t>::max()-provision_.initialIndex) return fail(Status::IndexOverflow);
  uint64_t target=provision_.initialIndex+delta;
  if(target<state_.index) return fail(Status::ClockRollback);
  if(target==state_.index) {
    if(!ready_) return fail(Status::ReusedIndex);
    lastTime_=t; status_=Status::Ok; return status_;
  }
  if(target-state_.index>MaxCatchup) return fail(Status::CatchupLimit);
  Checkpoint next=state_;
  while(next.index<target) {
    if(!updateSeed(next.seed,next.seed)) return fail(Status::CryptoFailure);
    ++next.index;
  }
  return publish(next,t);
}
}
