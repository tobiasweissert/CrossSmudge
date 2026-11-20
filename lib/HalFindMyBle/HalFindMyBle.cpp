#include "HalFindMyBle.h"
#if defined(CROSSCUB_FINDMY_EXPERIMENTAL) && CROSSCUB_FINDMY_EXPERIMENTAL && defined(ARDUINO_ARCH_ESP32)
#include "sdkconfig.h"
#endif
#if defined(CROSSCUB_FINDMY_EXPERIMENTAL) && CROSSCUB_FINDMY_EXPERIMENTAL && defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_IDF_TARGET_ESP32S3)
#include <atomic>
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_hs_id.h"
#include "esp_timer.h"
extern "C" void crosscub_findmy_bt_retention_anchor();
namespace hal_findmy_ble {
namespace {
Status current{State::Off, false, false, false, 0};
std::atomic<bool> synced{false};
std::atomic<int> resetReason{0};
int64_t syncDeadlineUs=0;
std::atomic<uint32_t> completed{0};
uint32_t generation=0;
uint8_t nativeAddress[6]{}, adv[31]{};
bool pending=false;
int64_t expiresUs=0;
int gapEvent(ble_gap_event* event, void* cookie) {
  if (event->type == BLE_GAP_EVENT_ADV_COMPLETE) {
    uint32_t tag=static_cast<uint32_t>(reinterpret_cast<uintptr_t>(cookie));
    uint32_t seen=completed.load();
    while (seen<tag && !completed.compare_exchange_weak(seen,tag)) {}
  }
  return 0;
}
void clearFrame() { pending=false; for (auto& b : nativeAddress) b=0; for (auto& b : adv) b=0; }
void onSync() { synced.store(true); }
void onReset(int reason) { resetReason.store(reason ? reason : -3); }
void fault(int error) {
  clearFrame();
  if (ble_gap_adv_active()) ble_gap_adv_stop();
  current={State::Fault,false,false,ble_gap_adv_active()!=0,error};
}
void hostTask(void*) { nimble_port_run(); nimble_port_freertos_deinit(); }
}
bool begin() {
  crosscub_findmy_bt_retention_anchor();
  if (current.state != State::Off) return current.state != State::Fault;
  int rc = nimble_port_init();
  if (rc) { current = {State::Fault, false, false, false, rc}; return false; }
  ble_hs_cfg.sync_cb = onSync;
  ble_hs_cfg.reset_cb = onReset;
  syncDeadlineUs=esp_timer_get_time()+5000000;
  current.state = State::WaitingSync;
  nimble_port_freertos_init(hostTask);
  return true;
}
bool setFrame(const uint8_t* address, size_t addressSize, const uint8_t* payload,
              size_t payloadSize, uint32_t ttlMs, bool trustedTime) {
  if (!stop()) return false;
  if (current.state == State::Off || current.state == State::Fault) return false;
  if (!address || !payload || addressSize!=6 || payloadSize!=31 || !trustedTime ||
      ttlMs==0 || ttlMs>MaxFrameTtlMs || (address[0]&0xc0)!=0xc0 ||
      payload[0]!=0x1e || payload[1]!=0xff || payload[2]!=0x4c ||
      payload[3]!=0 || payload[4]!=0x12 || payload[5]!=0x19 || payload[29]>3)
    return false;
  bool allZero=(address[0]&0x3f)==0, allOnes=(address[0]&0x3f)==0x3f;
  for (size_t i=1;i<6;++i) { allZero &= address[i]==0; allOnes &= address[i]==255; }
  if (allZero || allOnes) return false;
  for (size_t i=0;i<6;++i) nativeAddress[i]=address[5-i];
  for (size_t i=0;i<31;++i) adv[i]=payload[i];
  expiresUs=esp_timer_get_time()+static_cast<int64_t>(ttlMs)*1000;
  pending=true;
  return true;
}
bool stop() {
  clearFrame();
  if (current.state==State::Fault) return !current.uncertain;
  if (current.advertising) {
    int rc=ble_gap_adv_stop();
    if ((rc && rc!=BLE_HS_EALREADY) || ble_gap_adv_active()) {
      current={State::Fault,false,false,ble_gap_adv_active()!=0,rc ? rc : -2};
      return false;
    }
    current.advertising=false;
    current.state=State::Ready;
  }
  return true;
}
void poll() {
  int reset=resetReason.exchange(0);
  if (reset && current.state!=State::Off) fault(reset);
  if (current.state==State::Fault) {
    if (current.uncertain && !ble_gap_adv_active()) current.uncertain=false;
    synced.store(false); return;
  }
  if (current.state==State::WaitingSync && esp_timer_get_time()>=syncDeadlineUs) {
    fault(-1); return;
  }
  if (synced.exchange(false) && current.state == State::WaitingSync)
    current = {State::Ready, true, false, false, 0};
  if (completed.load() == generation && current.advertising) {
    clearFrame(); current.advertising=false; current.state=State::Ready;
  }
  if (pending && esp_timer_get_time()>=expiresUs) stop();
  if (pending && current.ready && !current.advertising) {
    int rc=ble_hs_id_set_rnd(nativeAddress);
    if (!rc) rc=ble_gap_adv_set_data(adv,31);
    if (!rc) rc=ble_gap_adv_rsp_set_data(nullptr,0);
    if (rc) { fault(rc); return; }
    ble_gap_adv_params params{};
    params.conn_mode=BLE_GAP_CONN_MODE_NON;
    params.disc_mode=BLE_GAP_DISC_MODE_NON;
    // Never reuse callback tags within a boot (late completions are monotonic).
    if (generation==UINT32_MAX) { fault(-4); return; }
    ++generation;
    int32_t duration=static_cast<int32_t>((expiresUs-esp_timer_get_time())/1000);
    if (duration<=0) { clearFrame(); return; }
    reset=resetReason.exchange(0);
    if (reset) { fault(reset); return; }
    rc=ble_gap_adv_start(BLE_OWN_ADDR_RANDOM,nullptr,duration,&params,gapEvent,
                         reinterpret_cast<void*>(static_cast<uintptr_t>(generation)));
    if (rc) { fault(rc); return; }
    reset=resetReason.exchange(0);
    if (reset) { fault(reset); return; }
    current.advertising=true; current.state=State::Advertising;
  }
}
Status status() { return current; }
}
#else
namespace hal_findmy_ble {
bool begin() { return false; }
bool setFrame(const uint8_t*, size_t, const uint8_t*, size_t, uint32_t, bool) { return false; }
bool stop() { return true; }
void poll() {}
Status status() { return {State::Unavailable, false, false, false, 0}; }
}
#endif
