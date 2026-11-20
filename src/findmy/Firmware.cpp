#include "Firmware.h"
#if defined(CROSSCUB_FINDMY_EXPERIMENTAL) && CROSSCUB_FINDMY_EXPERIMENTAL
#include <Runtime.h>
#include <PrivateState.h>
#include <HalFindMyBle.h>
#include <HalStorage.h>
#include <HalClock.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <nvs.h>
#include <ctime>
#include <sys/time.h>
namespace crosscub::findmy::firmware {
namespace {
class Store final : public ::findmy::Persistence {
 public:
 bool open(){if(open_)return true;open_=nvs_open("findmy-private",NVS_READWRITE,&handle_)==ESP_OK;return open_;}
 ::findmy::Load load(::findmy::Checkpoint& cp) override {
  if(!open())return ::findmy::Load::Error;
  StateBytes bytes{};size_t n=bytes.size();const auto rc=nvs_get_blob(handle_,"checkpoint",bytes.data(),&n);
  if(rc==ESP_ERR_NVS_NOT_FOUND)return ::findmy::Load::Empty;
  const bool ok=rc==ESP_OK&&n==bytes.size()&&decodeState(bytes,cp);
  ::findmy::secureErase(bytes.data(),bytes.size());return ok ? ::findmy::Load::Ok : ::findmy::Load::Error;
 }
 bool writeChecked(const char* key,const void* bytes,size_t n){
  if(!open()||n>StateBytes{}.size()||nvs_set_blob(handle_,key,bytes,n)!=ESP_OK||nvs_commit(handle_)!=ESP_OK)return false;
  StateBytes check{};size_t size=check.size();const bool ok=nvs_get_blob(handle_,key,check.data(),&size)==ESP_OK&&size==n&&!std::memcmp(bytes,check.data(),n);
  ::findmy::secureErase(check.data(),check.size());return ok;
 }
 bool commit(const ::findmy::Checkpoint& cp) override {
  StateBytes bytes{};encodeState(cp,bytes);const bool ok=writeChecked("checkpoint",bytes.data(),bytes.size());
  ::findmy::secureErase(bytes.data(),bytes.size());return ok;
 }
 bool reserveFresh(){
  if(!open())return false;
  uint8_t intent=0;size_t n=1;
  // Any prior intent, malformed state or uncertain transaction is permanent denial.
  if(nvs_get_blob(handle_,"intent",&intent,&n)!=ESP_ERR_NVS_NOT_FOUND)return false;
  ::findmy::Checkpoint cp;if(load(cp)!=::findmy::Load::Empty)return false;
  intent=1;return writeChecked("intent",&intent,1);
 }
 private: nvs_handle_t handle_{};bool open_=false;
};
uint64_t provisionEpoch=0,admittedWall=0;
class Native final : public Radio {
 bool begin() override{return hal_findmy_ble::begin();} // links strong early BT retention anchor
 void poll() override{hal_findmy_ble::poll();}
 bool ready() const override{return hal_findmy_ble::status().ready;}
 bool advertising() const override{return hal_findmy_ble::status().advertising;}
 bool ownershipUncertain() const override{return hal_findmy_ble::status().uncertain;}
 bool set(const ::findmy::Frame& f,uint32_t ttl) override{
  // Derivation/durable commit may block. Recheck the exact slot and remaining
  // wall budget immediately at the native publication boundary.
  timeval tv{};
  if(!halClock.hasSynchronizedSystemTime()||gettimeofday(&tv,nullptr)!=0||tv.tv_sec<0||
     static_cast<uint64_t>(tv.tv_sec)<admittedWall||admittedWall<provisionEpoch||tv.tv_usec<0||tv.tv_usec>=1000000)return false;
  const uint64_t wall=tv.tv_sec;
  if((wall-provisionEpoch)/900!=(admittedWall-provisionEpoch)/900)return false;
  const uint64_t remainingUs=(900-(wall-provisionEpoch)%900)*1000000-tv.tv_usec;
  ttl=static_cast<uint32_t>(std::min<uint64_t>(ttl,remainingUs/1000));
  return ttl>0&&hal_findmy_ble::setFrame(f.address.data(),f.address.size(),f.payload.data(),f.payload.size(),ttl,true);
 }
 bool stop() override{return hal_findmy_ble::stop();}
};
int rng(void*,unsigned char* p,size_t n){esp_fill_random(p,n);return 0;}
Store store;Native radio;Runtime runtime(store,radio,rng);
bool ownerAttempted=false;
const char* message="Off. Owner action required each boot.";
uint16_t wallSubsecondMs=0;
::findmy::Time now(){
 // Only successful HAL NTP provenance. The minute-only cached RTC API cannot
 // establish second precision; do not turn settings/env flags into trust.
 timeval tv{};const bool readOk=gettimeofday(&tv,nullptr)==0;
 const time_t wall=tv.tv_sec;const int64_t us=esp_timer_get_time();
 wallSubsecondMs=static_cast<uint16_t>(tv.tv_usec/1000);
 return {wall>0?static_cast<uint64_t>(wall):0,us>0?static_cast<uint64_t>(us/1000):0,
         readOk&&halClock.hasSynchronizedSystemTime()&&wall>=946684800&&wall<=4102444799LL&&tv.tv_usec>=0&&tv.tv_usec<1000000};
}
bool enable(bool fresh){
 if(ownerAttempted)return false;
 ownerAttempted=true;
 const auto t=now();if(!t.trusted){message="Stopped: reboot, sync NTP, then enable.";return false;}
 if(!Storage.ready()){message="Stopped: SD unavailable; reboot required.";return false;}
 auto file=Storage.open("/crosscub-findmy.json");
 if(!file||file.isDirectory()){message="Stopped: invalid private import; reboot required.";return false;}
 const size_t n=file.size();
 if(n==0||n>MaxImportBytes){message="Stopped: invalid private import; reboot required.";return false;}
 char bytes[MaxImportBytes]{};const bool readOk=file.read(bytes,n)==static_cast<int>(n);const bool closeOk=file.close();
 ::findmy::Provision p;const bool parsed=readOk&&closeOk&&parseImport(bytes,n,p);::findmy::secureErase(bytes,sizeof bytes);
 if(!parsed){message="Stopped: invalid private import; reboot required.";return false;}
 if(fresh&&!store.reserveFresh()){message="Stopped: prior/uncertain provision. Never erase state.";return false;}
 provisionEpoch=p.epochSeconds;admittedWall=t.unixSeconds;
 const bool ok=runtime.enable(p,t,fresh);
 message=ok?"Starting beacon (not proof of account pairing).":"Stopped: core/storage/radio refused. Reboot required.";
 return ok;
}
}
bool ownerEnableFresh(){return enable(true);}
bool ownerEnableExisting(){return enable(false);}
void poll(){const auto t=now();admittedWall=t.unixSeconds;runtime.poll(t,wallSubsecondMs);if(runtime.failed())message="Stopped: core/time/radio fault. Reboot required.";else if(runtime.standby())message="Tracking lock ready. Power use unmeasured.";}
bool standby(){return runtime.standby();}
bool beforeAirDrop(){ownerAttempted=true;const bool ok=runtime.stopForHandoff();message=ok?"Stopped for radio handoff. Reboot to re-enable.":"Radio ownership uncertain. Reboot required; sleep blocked.";return ok;}
bool beforeSleep(){return runtime.safeToSleep();}
const char* statusText(){return message;}
}
#endif
