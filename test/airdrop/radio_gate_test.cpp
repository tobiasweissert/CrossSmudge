// Executes the real Gate conditional branches. Host mutex seam models cross-core
// exclusion/acquire/release only; no claim about ESP32 task/ISR/driver timing.
#include <freertos/FreeRTOS.h>
#include "awdl_owned_gate.h"
#include <array>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>
using awdl_owned::Gate;
using awdl_owned::Use;
static void await(const std::atomic<unsigned>& counter,unsigned value){while(counter.load(std::memory_order_acquire)!=value)std::this_thread::yield();}
int main(){
 Gate gate;
 assert(gate.drained()&&!gate.enter());
 {Use refused(gate);assert(!refused);}assert(gate.drained());
 assert(gate.open());assert(!gate.open());assert(gate.enter());
 gate.close();assert(!gate.enter()&&!gate.open()&&!gate.drained());gate.leave();
 assert(gate.drained());
 constexpr unsigned Workers=8;
 // Exact accounting: every admitted user holds until individually released.
 // Reopening before the last release would expose old context to a new owner.
 for(unsigned cycle=0;cycle<100;++cycle){
  assert(gate.open());
  std::atomic<unsigned> admitted{0},finished{0};
  std::array<std::atomic<bool>,Workers> release{};
  std::vector<std::thread> workers;workers.reserve(Workers);
  for(unsigned i=0;i<Workers;++i)workers.emplace_back([&,i]{
   {Use use(gate);assert(use);admitted.fetch_add(1,std::memory_order_release);
    while(!release[i].load(std::memory_order_acquire))std::this_thread::yield();}
   finished.fetch_add(1,std::memory_order_release);
  });
  await(admitted,Workers);gate.close();
  assert(!gate.drained()&&!gate.enter()&&!gate.open());
  for(unsigned i=0;i<Workers;++i){
   release[i].store(true,std::memory_order_release);await(finished,i+1);
   if(i+1<Workers)assert(!gate.drained()&&!gate.open());
  }
  for(auto& worker:workers)worker.join();
  assert(gate.drained()&&!gate.enter());
 }
 // Race close against repeated admissions; free the modeled payload only after
 // drain. Any callback body touching it after drain is a lifetime violation.
 for(unsigned cycle=0;cycle<100;++cycle){
  assert(gate.open());
  std::atomic<bool> start{false},payloadLive{true};std::atomic<unsigned> callbacks{0};
  std::vector<std::thread> workers;workers.reserve(Workers);
  for(unsigned i=0;i<Workers;++i)workers.emplace_back([&]{
   while(!start.load(std::memory_order_acquire))std::this_thread::yield();
   for(unsigned attempt=0;attempt<1000;++attempt){Use use(gate);if(use){
     assert(payloadLive.load(std::memory_order_acquire));callbacks.fetch_add(1,std::memory_order_relaxed);
     std::this_thread::yield();assert(payloadLive.load(std::memory_order_acquire));
   }}
  });
  start.store(true,std::memory_order_release);
  while(callbacks.load(std::memory_order_relaxed)<Workers)std::this_thread::yield();
  gate.close();assert(!gate.enter());
  while(!gate.drained())std::this_thread::yield();
  payloadLive.store(false,std::memory_order_release);
  for(auto& worker:workers)worker.join();
  assert(gate.drained()&&!gate.enter());
 }
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
 assert(radioCriticalCalls.load()>1000);
 std::puts("PASS actual ESP32 portMUX Gate branch: exact8-user drain, no early reopen, repeated close/admission races, RAII, 200 cycles; host mutex seam only");
#else
 assert(radioCriticalCalls.load()==0);
 std::puts("PASS actual native atomic Gate branch: exact8-user drain, no early reopen, repeated close/admission races, RAII, 200 cycles");
#endif
}
