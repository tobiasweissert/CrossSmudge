// Harness for the actual methods extracted from awdl_port_esp32.cpp by awdl_run.py.
// Hardware fault seams model API returns; this does not emulate RF or prove driver timing.
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include "awdl_owned_gate.h"
using esp_err_t = int;
using TickType_t = uint32_t;
using TaskHandle_t = void*;
constexpr int ESP_OK=0, ESP_ERR_INVALID_STATE=1, ESP_ERR_TIMEOUT=2, ESP_ERR_NO_MEM=3, ERR_OK=0;
#define pdMS_TO_TICKS(n) (n)
static awdl_owned::Gate g_resource_gate,g_tap_gate,g_announce_gate;
static std::atomic<bool> g_awdl_stop{false},g_proc_stopped{true},g_cad_stopped{true};
enum class LinkPhase : uint8_t { Stopped,Starting,Ready,Stopping };
static std::atomic<LinkPhase> g_link_phase{LinkPhase::Stopped};
static std::atomic<TaskHandle_t> g_proc_task{nullptr},g_cadence_task{nullptr};
static bool g_wifi_owned=false,g_wifi_started=false,g_rx_registered=false,g_tx_registered=false,g_promiscuous=false;
static std::atomic<bool> g_drain_posted{false},g_drain_done{false};
static void tcpip_drained(void*) {g_drain_done.store(true,std::memory_order_release);}
static void *g_awdl_netif=nullptr,*g_netif_txq=nullptr,*g_retry_frame=nullptr;
static void *g_rx_tap=nullptr,*g_rx_tap_ctx=nullptr,*g_announce_cb=nullptr,*g_announce_ctx=nullptr;
static int fail_at=0,calls=0,destroys=0,deletes=0,frees=0;
static TickType_t ticks=0;
static void (*queued)(void*)=nullptr;
static int operation(){return ++calls==fail_at?99:ESP_OK;}
static int esp_wifi_set_promiscuous(bool){return operation();}
static int esp_wifi_set_promiscuous_rx_cb(void*){return operation();}
static int esp_wifi_register_80211_tx_cb(void*){return operation();}
static int esp_wifi_stop(){return operation();}
static int esp_wifi_deinit(){return operation();}
static void esp_netif_action_disconnected(void*,void*,int,void*){}
static void esp_netif_action_stop(void*,void*,int,void*){}
static int tcpip_try_callback(void(*fn)(void*),void*){
  if(operation()!=ESP_OK)return -1;
  queued=fn;return ERR_OK;
}
static void esp_netif_destroy(void*){++destroys;}
static void vQueueDelete(void*){++deletes;}
static void fake_free(void*p){++frees;std::free(p);}
static TickType_t xTaskGetTickCount(){return ticks;}
static void vTaskDelay(int n){ticks+=n;}
#define free fake_free
#include "awdl_lifecycle_extracted.inc"
#undef free
static void activate(){
  assert(awdl_is_stopped());
  assert(g_resource_gate.open());
  assert(g_tap_gate.open());assert(g_announce_gate.open());
  g_awdl_stop=false;g_link_phase=LinkPhase::Ready;
  g_proc_stopped=true;g_cad_stopped=true;
  g_wifi_owned=g_wifi_started=g_rx_registered=g_tx_registered=g_promiscuous=true;
  g_awdl_netif=reinterpret_cast<void*>(1);g_netif_txq=reinterpret_cast<void*>(2);
  g_retry_frame=std::malloc(1);assert(g_retry_frame);
  g_drain_posted=false;g_drain_done=false;queued=nullptr;
  calls=destroys=deletes=frees=0;fail_at=0;ticks=0;
}
static void complete(){
  assert(awdl_poll_stop()==ESP_ERR_TIMEOUT);assert(queued);queued(nullptr);queued=nullptr;
  assert(awdl_poll_stop()==ESP_OK);assert(awdl_is_stopped());
  assert(destroys==1&&deletes==1&&frees==1);
  assert(awdl_end()==ESP_OK);assert(destroys==1&&deletes==1&&frees==1);
}
int main(){
  assert(awdl_end()==ESP_OK);
  // Every driver cleanup failure retains all netif/queue/retry ownership and can retry.
  for(int failure=1;failure<=5;++failure){
    activate();awdl_request_stop();fail_at=failure;
    assert(awdl_poll_stop()==99);assert(!awdl_is_stopped());
    assert(!destroys&&!deletes&&!frees);assert(g_awdl_stop);
    fail_at=0;complete();
  }
  // Partial acquisition rollback works without a netif, including queue-only failures.
  for(int stage=0;stage<=7;++stage){
    activate();
    g_wifi_owned=stage>=1;g_wifi_started=stage>=2;
    g_tx_registered=stage>=3;g_rx_registered=stage>=4;g_promiscuous=stage>=5;
    if(stage<6){std::free(g_retry_frame);g_retry_frame=nullptr;}
    if(stage<5)g_netif_txq=nullptr;
    if(stage<7)g_awdl_netif=nullptr;
    awdl_request_stop();const int result=awdl_poll_stop();
    if(stage==7){assert(result==ESP_ERR_TIMEOUT);assert(queued);queued(nullptr);
      assert(awdl_poll_stop()==ESP_OK);
    }else assert(result==ESP_OK);
    assert(awdl_is_stopped()&&g_awdl_stop);
    assert(destroys==(stage>=7)&&deletes==(stage>=5)&&frees==(stage>=6));
  }
  // Stop cannot free a live task's resources, or re-enable a half-stopped session.
  activate();g_proc_stopped=false;awdl_request_stop();assert(!awdl_is_ready());
  assert(awdl_end()==ESP_ERR_TIMEOUT);assert(ticks==5000);assert(!calls&&!destroys);
  assert(g_awdl_stop&&!awdl_is_ready());g_proc_stopped=true;complete();
  // Admitted callbacks retain ownership. New callbacks fail immediately after close.
  activate();assert(g_resource_gate.enter());assert(g_tap_gate.enter());
  awdl_request_stop();assert(!g_resource_gate.enter());assert(!g_tap_gate.enter());
  assert(awdl_poll_stop()==ESP_ERR_TIMEOUT);assert(!calls);
  g_resource_gate.leave();assert(awdl_poll_stop()==ESP_ERR_TIMEOUT);assert(!calls);
  g_tap_gate.leave();complete();
  // FIFO queue admission failure and delayed completion cannot destroy the netif.
  activate();awdl_request_stop();fail_at=6;
  assert(awdl_poll_stop()==ESP_ERR_NO_MEM);assert(!g_drain_posted);assert(!destroys);
  fail_at=0;assert(awdl_poll_stop()==ESP_ERR_TIMEOUT);assert(g_drain_posted);
  for(int i=0;i<100;++i)assert(awdl_poll_stop()==ESP_ERR_TIMEOUT);
  assert(!destroys&&!deletes&&!frees);queued(nullptr);queued=nullptr;
  assert(awdl_poll_stop()==ESP_OK);assert(awdl_is_stopped());
  // Real gate used by production callbacks: close/reopen requires previous users gone.
  awdl_owned::Gate gate;assert(gate.open());assert(gate.enter());gate.close();
  assert(!gate.open()&&!gate.enter()&&!gate.drained());gate.leave();
  assert(gate.drained()&&gate.open());gate.close();
  for(int cycle=0;cycle<100;++cycle){
    assert(gate.open());std::atomic<bool> start{false};std::atomic<int> entered{0};
    std::vector<std::thread> workers;workers.reserve(4);
    for(int i=0;i<4;++i)workers.emplace_back([&]{while(!start.load()){};for(int n=0;n<1000;++n){
      awdl_owned::Use use(gate);if(use)++entered;
    }});
    start=true;gate.close();for(auto& t:workers)t.join();
    assert(gate.drained()&&!gate.enter());
  }
  std::puts("PASS actual AWDL stop methods: task/callback retention, five cleanup faults, FIFO barrier, timeout retry, idempotence, 100 concurrent gate cycles");
}
