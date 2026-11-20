#pragma once
#include <FindMyCore.h>
#include <algorithm>
namespace crosscub::findmy {
struct Radio {
 virtual ~Radio()=default;
 virtual bool begin()=0;
 virtual void poll()=0;
 virtual bool ready() const=0;
 virtual bool advertising() const=0;
 virtual bool ownershipUncertain() const=0;
 virtual bool set(const ::findmy::Frame&,uint32_t ttl)=0;
 virtual bool stop()=0; // synchronous join/idle confirmation
};
// One application-task owner. No auto-retry following faults or radio handoff.
class Runtime {
 public:
 Runtime(::findmy::Persistence& store,Radio& radio,::findmy::Random rng,void* ctx=nullptr)
 : core_(rng,ctx),store_(store),radio_(radio) {}
 bool enable(const ::findmy::Provision& p,const ::findmy::Time& t,bool fresh) {
  if(attempted_ || blocked_)return false;
  attempted_=true;
  if(!t.trusted || core_.init(p,store_,t,fresh)!=::findmy::Status::Ok) { fail();return false; }
  if(!radio_.begin()){fail();return false;}
  enabled_=true; epoch_=p.epochSeconds; beginMs_=t.monotonicMs;return true;
 }
 void poll(const ::findmy::Time& t,uint16_t wallSubsecondMs=0) {
  if(!attempted_)return;
  radio_.poll();
  if(!enabled_)return;
  if(!t.trusted || wallSubsecondMs>999 || radio_.ownershipUncertain() ||
     core_.rotate(t)!=::findmy::Status::Ok) {fail();return;}
  const auto d=core_.diagnostics();
  const bool rotated=lease_ && d.index!=publishedIndex_;
  if(lease_ && t.monotonicMs>=expires_) {
   // Never renew an expired old key. At a continuously serviced slot boundary
   // stop/join the old lease, then admit only the newly committed key.
   if(!rotated || t.monotonicMs-lastPollMs_>1000 || !stopConfirmed()) {fail();return;}
  }
  lastPollMs_=t.monotonicMs;
  if(!radio_.ready()) {if(t.monotonicMs-beginMs_>=5000)fail();return;}
  if(!lease_ || rotated || t.monotonicMs>=renew_) {
   const uint64_t remaining=::findmy::Core::RotationSeconds-(t.unixSeconds-epoch_) % ::findmy::Core::RotationSeconds;
   const uint32_t ttl=static_cast<uint32_t>(std::min<uint64_t>(60000,remaining*1000-std::min<uint16_t>(999,wallSubsecondMs)));
   ::findmy::Frame frame;
   if(!d.ready || !core_.currentFrame(frame) || !radio_.set(frame,ttl)) {fail();return;}
   publishedIndex_=d.index;lease_=true;expires_=t.monotonicMs+ttl;renew_=t.monotonicMs+ttl/2;
   radio_.poll();
  }
 }
 bool standby() const {return enabled_ && lease_ && radio_.advertising() && !radio_.ownershipUncertain();}
 bool stopForHandoff() {enabled_=false;blocked_=true;lease_=false;return stopConfirmed();}
 bool safeToSleep() {return stopForHandoff();}
 bool failed() const {return attempted_&&!enabled_;}
 private:
 bool stopConfirmed(){return radio_.stop()&&!radio_.ownershipUncertain()&&!radio_.advertising();}
 void fail(){enabled_=false;blocked_=true;lease_=false;radio_.stop();}
 ::findmy::Core core_; ::findmy::Persistence& store_; Radio& radio_;
 bool attempted_=false,enabled_=false,blocked_=false,lease_=false;
 uint64_t expires_=0,renew_=0,beginMs_=0,epoch_=0,lastPollMs_=0,publishedIndex_=0;
};
}
