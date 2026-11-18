#include <cstdint>
#include <cstddef>
#include <cstring>
#include <deque>
#include <cassert>
#include <cstdio>
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
#define pdTRUE 1
#define ESP_OK 0
#define WIFI_SEND_SUCCESS 0
#define WIFI_IF_AP 0
#define AWDL_NETIF_INFLIGHT_MAX 16
#define AWDL_NETIF_MTU 1280
#define AWDL_TX_HEADROOM 40
using esp_err_t=int;
namespace awdl_owned { struct Use { Use(int){} explicit operator bool()const{return true;} }; }
int g_resource_gate=0,g_netif_tx_mux=0,g_cadence_task=1;
struct Diag { uint32_t netif_inflight=0; uint32_t netif_inflight_max=0; uint32_t netif_retry_drop=0; uint32_t netif_retry_exhausted=0; uint32_t netif_retry_queued=0; uint32_t netif_tx_err=0; uint32_t netif_tx_err_icmp=0; uint32_t netif_tx_err_len=0; uint32_t netif_tx_err_mcast=0; uint32_t netif_tx_err_nh=0; uint32_t netif_tx_err_ucast=0; uint32_t netif_tx_last_err=0; uint32_t netif_tx_sent=0; uint32_t netif_txq_drop=0; uint32_t txdone_action_fail=0; uint32_t txdone_action_ok=0; uint32_t txdone_mcast_fail=0; uint32_t txdone_mcast_ok=0; uint32_t txdone_ucast_fail=0; uint32_t txdone_ucast_ok=0; uint8_t netif_tx_err_a1[6]{}; } g_diag;
static const uint8_t NETIF_TX_RETRY_MAX=7, NETIF_TX_RETRY_SLOTS=64;
volatile uint8_t g_netif_inflight=0,g_netif_inflight_used[64]{},g_retry_n[64]{};
volatile uint16_t g_netif_inflight_seq[64]{},g_retry_seq[64]{};
struct NetifTxFrame { uint16_t len; uint8_t buf[1320]; } retry;
NetifTxFrame *g_retry_frame=&retry;
std::deque<NetifTxFrame> queue;
void *g_netif_txq=&queue;
int notify=0, injectionResult=0;
void xTaskNotifyGive(int){++notify;}
int xQueueSendToFront(void*,const NetifTxFrame *f,int){queue.push_front(*f);return 1;}
int xQueueReceive(void*,NetifTxFrame *f,int){if(queue.empty())return 0;*f=queue.front();queue.pop_front();return 1;}
struct esp_80211_tx_info_t { const uint8_t *data; int tx_status; };
static void raw_tx_done_cb(const esp_80211_tx_info_t*);
int esp_wifi_80211_tx(int,const uint8_t *f,size_t,bool){if(!injectionResult){esp_80211_tx_info_t ti{f,0};raw_tx_done_cb(&ti);}return injectionResult;}
int64_t esp_timer_get_time(){return 0;}
bool in_ch6_window_now(){return true;}
// Production methods are extracted by fixes_run.py here.
NetifTxFrame frame(uint16_t seq,bool multicast){NetifTxFrame f{};f.len=80;f.buf[0]=8;f.buf[4]=multicast?1:2;f.buf[32]=3;f.buf[33]=4;f.buf[34]=seq&255;f.buf[35]=seq>>8;f.buf[40]=0x60;return f;}
int main(){
 // Actual flush admission and actual completion: more than the hard credit cap.
 for(uint16_t i=1;i<=20;++i){queue.push_back(frame(i,true));awdl_netif_flush_tx(1,1);}
 if(g_netif_inflight!=0 || !queue.empty()){puts("FAIL multicast successful completions leaked credits");return 1;}
 // Failed multicast completion also frees credit, but never retries without ACKs.
 auto f=frame(30,true);assert(netif_inflight_begin(30));esp_80211_tx_info_t ti{f.buf,1};raw_tx_done_cb(&ti);
 assert(g_netif_inflight==0 && queue.empty());
 // Immediate tap and action traffic do not own a queued credit.
 f=frame(31,true);ti={f.buf,0};raw_tx_done_cb(&ti);assert(g_netif_inflight==0);
 f.buf[0]=0xd0;raw_tx_done_cb(&ti);assert(g_diag.txdone_action_ok==1);
 // Unicast failure keeps its bounded retry accounting and releases matched credit.
 f=frame(32,false);g_retry_seq[32]=32;assert(netif_inflight_begin(32));ti={f.buf,1};raw_tx_done_cb(&ti);
 assert(g_netif_inflight==0 && queue.size()==1 && g_retry_n[32]==1);
 awdl_netif_flush_tx(1,1);assert(queue.empty() && g_netif_inflight==0);
 // Full-sequence mismatch cannot release a recycled slot.
 assert(netif_inflight_begin(33));f=frame(97,true);ti={f.buf,0};raw_tx_done_cb(&ti);assert(g_netif_inflight==1);assert(netif_inflight_done(33));
 // Rejected injection is released at the real flush site.
 injectionResult=1;queue.push_back(frame(34,true));awdl_netif_flush_tx(1,1);assert(g_netif_inflight==0);
 puts("PASS multicast cap+success+failure+unicast retry+tap/action+sequence+injection controls");
}
