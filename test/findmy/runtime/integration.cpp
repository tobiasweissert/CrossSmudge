#include <cassert>
#include <cstdio>
#if __has_include("Runtime.h")
#include "Runtime.h"
struct Store : findmy::Persistence {
 findmy::Checkpoint cp; bool has=false, fail=false; int writes=0;
 findmy::Load load(findmy::Checkpoint& out) override { if(has) out=cp; return has?findmy::Load::Ok:findmy::Load::Empty; }
 bool commit(const findmy::Checkpoint& in) override {++writes; if(fail)return false; cp=in; has=true; return true;}
};
struct Radio : crosscub::findmy::Radio {
 bool active=false, uncertain=false; int frames=0, begins=0; uint32_t ttl=0;
 bool begin() override {++begins; return true;}
 void poll() override {}
 bool ready() const override {return true;}
 bool advertising() const override {return active;}
 bool ownershipUncertain() const override {return uncertain;}
 bool set(const ::findmy::Frame&,uint32_t t) override {++frames;ttl=t;active=true;return true;}
 bool stop() override {active=false;return !uncertain;}
};
int rng(void*,unsigned char* p,size_t n) {while(n--)*p++=7;return 0;}
int main(){
 Store store; Radio radio; crosscub::findmy::Runtime run(store,radio,rng);
 run.poll({1800000000,0,true}); assert(radio.begins==0); assert(!run.standby());
 findmy::Provision p; p.master[27]=1; p.primarySeed.fill(5);p.epochSeconds=1800000000;
 assert(run.enable(p,{1800000000,0,true},true));
 run.poll({1800000000,0,true}); assert(run.standby());assert(radio.ttl==60000);
 run.poll({1800000030,30000,true}); assert(radio.frames==2);
 run.poll({1800000900,900000,true}); assert(!run.standby()); // missed TTL must fail closed, never renew expired lease
 assert(!radio.active);assert(!run.enable(p,{1800000900,900000,true},false));
 // A public frame may never outlive the key slot, including subsecond wall phase.
 Store edgeStore;Radio edgeRadio;crosscub::findmy::Runtime edge(edgeStore,edgeRadio,rng);
 assert(edge.enable(p,{1800000899,0,true},true));
 edge.poll({1800000899,0,true},750);assert(edgeRadio.ttl==250);
 Store rotationStore;Radio rotationRadio;crosscub::findmy::Runtime rotation(rotationStore,rotationRadio,rng);
 assert(rotation.enable(p,{1800000000,0,true},true));
 for(uint64_t ms=0;ms<=900050;ms+=50)rotation.poll({1800000000+ms/1000,ms,true},ms%1000);
 assert(rotation.standby());assert(rotationStore.cp.index==1);
 struct BadStopRadio : Radio {bool stop() override{return true;}};
 Store handoffStore;BadStopRadio badStop;crosscub::findmy::Runtime handoff(handoffStore,badStop,rng);
 assert(handoff.enable(p,{1800000000,0,true},true));handoff.poll({1800000000,0,true});
 assert(!handoff.stopForHandoff());assert(!handoff.safeToSleep());
 puts("PASS runtime explicit opt-in / lease / missed expiry latch / slot bound / continuous rotation / confirmed idle");
}
#else
int main(){assert(false && "production Runtime integration missing");}
#endif
