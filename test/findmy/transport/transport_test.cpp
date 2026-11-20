#include "HalFindMyBle.h"
#include <cassert>
#include <cstring>
#include <iostream>
#ifdef CROSSCUB_FINDMY_EXPERIMENTAL
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_hs_id.h"
#include "esp_timer.h"
static int initCalls, taskCalls, starts, stops, active, initError;
static int addressError, dataError, scanError, startError, stopError;
static int64_t nowUs;
static uint8_t capturedAddress[6], capturedPayload[31];
static int duration, scanLen=-1;
static ble_gap_event_fn* gapCallback;
static void* gapCookie;
static uint8_t address[6]={0xc1,2,3,4,5,6};
static uint8_t payload[31]={0x1e,0xff,0x4c,0,0x12,0x19};
static bool frame(uint32_t ttl=1000, bool trusted=true) {
 return hal_findmy_ble::setFrame(address,6,payload,31,ttl,trusted);
}
ble_hs_cfg_t ble_hs_cfg{};
extern "C" int nimble_port_init() { ++initCalls; return initError; }
extern "C" void nimble_port_run() {}
extern "C" void nimble_port_freertos_init(void (*)(void*)) { ++taskCalls; }
extern "C" void nimble_port_freertos_deinit() {}
extern "C" int64_t esp_timer_get_time() { return nowUs; }
extern "C" int ble_hs_id_set_rnd(const uint8_t* p) { std::memcpy(capturedAddress,p,6); return addressError; }
extern "C" int ble_gap_adv_set_data(const uint8_t* p, int len) { assert(len==31); std::memcpy(capturedPayload,p,31); return dataError; }
extern "C" int ble_gap_adv_rsp_set_data(const uint8_t*, int len) { scanLen=len; return scanError; }
extern "C" int ble_gap_adv_start(uint8_t type, const ble_addr_t* peer, int32_t ms,
 const ble_gap_adv_params* params, ble_gap_event_fn* cb, void* cookie) {
 assert(type==BLE_OWN_ADDR_RANDOM && peer==nullptr);
 assert(params->conn_mode==BLE_GAP_CONN_MODE_NON && params->disc_mode==BLE_GAP_DISC_MODE_NON);
 duration=ms; gapCallback=cb; gapCookie=cookie; ++starts; active=1; return startError;
}
extern "C" int ble_gap_adv_stop() { ++stops; if (!stopError) active=0; return stopError; }
extern "C" int ble_gap_adv_active() { return active; }
extern "C" bool btInUse();
#ifdef CROSSCUB_FINDMY_TEST_WEAK_BT_BOOT
extern "C" __attribute__((weak)) bool btInUse() { return false; }
#endif
#endif
int main(int argc, char** argv) {
#ifdef CROSSCUB_FINDMY_EXPERIMENTAL
  assert(argc == 2);
  using namespace hal_findmy_ble;
  if (std::strcmp(argv[1], "init-failure") == 0) {
    initError=42; assert(!begin()); assert(!begin() && initCalls==1 && taskCalls==0);
    assert(status().state==State::Fault && status().error==42 && !frame());
    std::cout << "PASS init failure terminal\n"; return 0;
  }
  if (std::strcmp(argv[1], "sync-timeout") == 0) {
    assert(begin()); assert(frame()); nowUs=5000000; poll();
    assert(status().state==State::Fault && !status().ready && !frame());
    ble_hs_cfg.sync_cb(); poll(); assert(starts==0 && !status().ready);
    std::cout << "PASS bounded sync timeout\n"; return 0;
  }
  if (std::strncmp(argv[1], "reset-", 6) == 0) {
    assert(begin()); assert(frame());
    if (!std::strcmp(argv[1]+6,"active")) { ble_hs_cfg.sync_cb(); poll(); assert(starts==1); }
    assert(ble_hs_cfg.reset_cb); ble_hs_cfg.reset_cb(77);
    ble_hs_cfg.sync_cb(); poll();
    assert(status().state==State::Fault && !status().ready && !active);
    assert(status().error==77 && !frame() && !begin());
    int n=starts; poll(); assert(starts==n);
    std::cout << "PASS " << argv[1] << " clears frame without resync restart\n"; return 0;
  }
  if (std::strncmp(argv[1], "failure-", 8) == 0) {
    assert(begin()); ble_hs_cfg.sync_cb(); poll();
    const char* kind=argv[1]+8;
    if (!std::strcmp(kind,"address")) addressError=55;
    if (!std::strcmp(kind,"data")) dataError=55;
    if (!std::strcmp(kind,"scan")) scanError=55;
    if (!std::strcmp(kind,"start")) startError=55;
    assert(frame()); poll();
    if (!std::strcmp(kind,"stop")) { stopError=55; assert(!stop()); }
    assert(status().state==State::Fault && !status().ready && status().error==55);
    int n=starts; assert(!frame()); ble_hs_cfg.sync_cb(); poll(); assert(starts==n);
    if (!std::strcmp(kind,"stop")) {
      assert(status().uncertain && !status().advertising);
      nowUs=2000000; poll(); assert(status().uncertain); // Never infer idle just from deadline.
      active=0; poll(); assert(!status().uncertain && status().state==State::Fault);
    } else assert(!active);
    std::cout << "PASS " << argv[1] << " terminal fail-closed\n"; return 0;
  }
  if (std::strcmp(argv[1], "late-completion") == 0) {
    assert(begin()); ble_hs_cfg.sync_cb(); poll(); assert(frame()); poll();
    auto oldCb=gapCallback; auto oldCookie=gapCookie;
    assert(frame()); assert(starts==1); poll(); assert(starts==2);
    ble_gap_event e{BLE_GAP_EVENT_ADV_COMPLETE}; oldCb(&e,oldCookie); poll();
    assert(status().advertising && starts==2);
    active=0; gapCallback(&e,gapCookie); oldCb(&e,oldCookie); poll();
    assert(!status().advertising); // Current completion must survive delayed old callback.
    assert(stop()); ble_hs_cfg.sync_cb(); poll(); assert(starts==2);
    std::cout << "PASS late completion cannot clear new frame, optout cannot restart\n"; return 0;
  }
  if (std::strncmp(argv[1], "invalid-", 8) == 0) {
    assert(begin()); ble_hs_cfg.sync_cb(); poll();
    assert(frame()); poll(); assert(starts==1);
    const uint8_t* a=address; const uint8_t* p=payload;
    size_t an=6,pn=31; uint32_t ttl=1000; bool trust=true;
    const char* kind=argv[1]+8;
    if (!std::strcmp(kind,"address-size")) an=5;
    if (!std::strcmp(kind,"payload-size")) pn=30;
    if (!std::strcmp(kind,"null-address")) a=nullptr;
    if (!std::strcmp(kind,"null-payload")) p=nullptr;
    if (!std::strcmp(kind,"untrusted")) trust=false;
    if (!std::strcmp(kind,"zero-ttl")) ttl=0;
    if (!std::strcmp(kind,"long-ttl")) ttl=MaxFrameTtlMs+1;
    if (!std::strcmp(kind,"prefix")) payload[4]=0x13;
    if (!std::strcmp(kind,"random-type")) address[0]=0x81;
    if (!std::strcmp(kind,"zero-address")) { std::memset(address,0,6); address[0]=0xc0; }
    if (!std::strcmp(kind,"ones-address")) std::memset(address,255,6);
    if (!std::strcmp(kind,"key-bits")) payload[29]=4;
    assert(!setFrame(a,an,p,pn,ttl,trust));
    poll(); poll(); assert(!active && !status().advertising && starts==1);
    std::cout << "PASS " << argv[1] << " clears old frame\n"; return 0;
  }
  if (std::strcmp(argv[1], "expiry") == 0) {
    assert(begin()); assert(frame(10)); nowUs=11000;
    ble_hs_cfg.sync_cb(); poll(); assert(starts==0);
    assert(frame(10)); poll(); assert(starts==1 && duration==10);
    nowUs=21000; poll(); assert(!active && !status().advertising);
    poll(); assert(starts==1);
    std::cout << "PASS queued and advertising TTL expiration\n"; return 0;
  }
  if (std::strcmp(argv[1], "frame") == 0) {
    assert(!frame()); // No implicit begin.
    assert(begin());
    assert(frame()); // Queued while waiting for sync.
    poll(); assert(starts==0);
    nowUs=100000;
    ble_hs_cfg.sync_cb(); poll();
    assert(status().ready && status().advertising);
    const uint8_t reversed[6]={6,5,4,3,2,0xc1};
    assert(std::memcmp(capturedAddress,reversed,6)==0);
    assert(std::memcmp(capturedPayload,payload,31)==0);
    assert(duration==900 && scanLen==0 && starts==1);
    assert(stop()); poll();
    assert(!status().advertising && !active && starts==1);
    assert(frame()); poll(); assert(starts==2);
    active=0; ble_gap_event event{BLE_GAP_EVENT_ADV_COMPLETE}; gapCallback(&event,gapCookie);
    poll(); poll(); assert(!status().advertising && starts==2);
    std::cout << "PASS frame capture, endian reversal, finite duration, stop/completion no restart\n";
    return 0;
  }
  if (std::strcmp(argv[1], "sync") == 0) {
    assert(btInUse());
    assert(initCalls == 0 && starts == 0);
    assert(begin());
    assert(initCalls == 1 && taskCalls == 1 && starts == 0);
    assert(status().state == State::WaitingSync && !status().ready);
    assert(begin() && initCalls == 1);
    assert(ble_hs_cfg.sync_cb);
    ble_hs_cfg.sync_cb(); poll();
    assert(status().ready && !status().advertising && starts == 0);
    assert(stop());
    std::cout << "PASS async sync, explicit begin, no startup RF\n";
    return 0;
  }
#endif
  assert(argc == 2);
  assert(std::strcmp(argv[1], "inert") == 0);
  using namespace hal_findmy_ble;
  assert(!begin());
  assert(!status().ready && !status().advertising);
  assert(status().state == State::Unavailable);
  stop(); poll();
  std::cout << "PASS inert default-off\n";
}
