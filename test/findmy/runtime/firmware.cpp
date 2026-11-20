#include <cassert>
#include <cstdio>
#if __has_include("Firmware.h")
#include "Firmware.h"
#include <HalFindMyBle.h>
#include <HalStorage.h>
#include <HalClock.h>
#include <esp_timer.h>
#include <nvs.h>
#include <ctime>
#include <sys/time.h>
extern "C" int gettimeofday(timeval* tv,void*) noexcept {tv->tv_sec=946684800+fakeUs/1000000;tv->tv_usec=fakeUs%1000000;return 0;}
extern "C" time_t time(time_t* out) noexcept {time_t t=946684800+fakeUs/1000000;if(out)*out=t;return t;}
namespace hal_findmy_ble {
int beginCount=0,frameCount=0;uint32_t lastTtl=0;int64_t frameExpiresUs=0;bool active=false,uncertain=false;
bool begin(){++beginCount;return true;}
bool setFrame(const uint8_t*,size_t,const uint8_t*,size_t,uint32_t ttl,bool trust){assert(trust&&ttl>0&&ttl<=60000);++frameCount;lastTtl=ttl;frameExpiresUs=fakeUs+int64_t(ttl)*1000;active=true;return true;}
bool stop(){if(uncertain)return false;active=false;return true;}
void poll(){}
Status status(){return {active?State::Advertising:State::Ready,true,active,uncertain,0};}
}
int main(int argc,char** argv){
 namespace fw=crosscub::findmy::firmware;
 importJson="{\"schema\":\"crosscub-findmy-v1\",\"master_private_key_hex\":\""+std::string(55,'0')+"1\",\"primary_ratchet_seed_hex\":\""+std::string(64,'5')+"\",\"paired_at\":\"2000-01-01T00:00:00Z\"}";
 fw::poll();assert(hal_findmy_ble::beginCount==0);assert(!fw::standby());
 const std::string mode=argc>1?argv[1]:"normal";
 if(mode=="handoff-before-enable"){
  assert(fw::beforeAirDrop());assert(!fw::ownerEnableFresh());assert(nv.empty());
  assert(hal_findmy_ble::beginCount==0);puts("PASS pre-enable handoff cannot burn a fresh provision intent");return 0;
 }
 if(mode=="size-race")changingSize=true;
 if(mode=="no-time")halClock.synced=false;
 if(mode=="nvs-fail")failCommit=true;
 if(mode=="checkpoint-fail")failCommitAt=2;
 if(mode=="checkpoint-readback-fail")corruptCheckpointRead=true;
 if(mode=="old-intent")nv["intent"]={1};
 if(mode=="corrupt-state")nv["checkpoint"]={0,1,2};
 if(mode=="bad-import")importJson=std::string(513,'x');
 if(mode!="normal"&&mode!="slowcommit"&&mode!="uncertain"&&mode!="lost-time"&&mode!="size-race"){
  const bool enabled=mode=="existing-missing"?fw::ownerEnableExisting():fw::ownerEnableFresh();
  assert(!enabled);fw::poll();assert(!fw::standby());assert(hal_findmy_ble::beginCount==0);
  assert(!fw::ownerEnableFresh());assert(!fw::ownerEnableExisting());
  if(mode=="nvs-fail"||mode=="old-intent")assert(nv.count("intent")==1);
  if(mode=="checkpoint-fail"||mode=="checkpoint-readback-fail")assert(nv.count("intent")==1&&nv.count("checkpoint")==1);
  printf("PASS firmware fail-closed %s; no RF admission or auto retry\n",mode.c_str());return 0;
 }
 assert(fw::ownerEnableFresh());fw::poll();assert(fw::standby());
 assert(nv.count("intent")==1&&nv.count("checkpoint")==1);
 if(mode=="size-race"){assert(sizeCalls==1);puts("PASS import uses one checked bounded length");return 0;}
 if(mode=="uncertain"){
  hal_findmy_ble::uncertain=true;assert(!fw::beforeAirDrop());assert(!fw::beforeSleep());
  const int frames=hal_findmy_ble::frameCount;fw::poll();
  hal_findmy_ble::uncertain=false;fw::poll();assert(!fw::standby());
  assert(hal_findmy_ble::frameCount==frames);assert(!fw::ownerEnableExisting());assert(fw::beforeSleep());
  puts("PASS uncertain radio blocks AWDL and sleep; no auto restart after resolution");return 0;
 }
 if(mode=="lost-time"){
  halClock.synced=false;fw::poll();assert(!fw::standby());assert(!hal_findmy_ble::active);
  halClock.synced=true;fw::poll();assert(!fw::standby());assert(!fw::ownerEnableExisting());
  puts("PASS lost time stops radio; regained time cannot auto restart");return 0;
 }
 fakeUs=30000000;fw::poll();assert(hal_findmy_ble::frameCount==2);
 for(fakeUs=30050000;fakeUs<=899950000;fakeUs+=50000)fw::poll();
 assert(fw::standby());assert(hal_findmy_ble::frameExpiresUs<=900000000);
 if(argc>1){
  delayCommitUs=900000000;fakeUs=900000000;fw::poll();
  assert(!fw::standby());assert(!hal_findmy_ble::active);
  puts("PASS slow durable commit cannot publish an expired-slot public key");return 0;
 }
 assert(fw::beforeAirDrop());assert(!fw::standby());fw::poll();assert(!hal_findmy_ble::active);
 assert(!fw::ownerEnableFresh());assert(!fw::ownerEnableExisting());assert(fw::beforeSleep());
 puts("PASS actual firmware opt-in/import/NVS/Core/radio/handoff integration");
}
#else
int main(){assert(false && "firmware adapter missing");}
#endif
