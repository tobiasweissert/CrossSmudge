#!/usr/bin/env python3
"""Exercise the production TLS wrappers with fake task/heap boundaries."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "lib/hal/HalLuaHttp.cpp").read_text()
wrappers = source[source.index('extern "C" void* __wrap_esp_mbedtls_mem_calloc'):source.index('\nnamespace HalAirDropRadio {')]
prelude = r'''
#include <AllocationBudget.h>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
using TaskHandle_t = int;
static int current=1, internalCalls=0, psramCalls=0;
static bool psramFail=false;
static std::atomic<TaskHandle_t> budgetTask{0}, receiverBudgetTask{0};
static ink::http::AllocationBudget *activeBudget=nullptr, *receiverBudget=nullptr;
constexpr int MALLOC_CAP_SPIRAM=1, MALLOC_CAP_8BIT=2;
static std::unordered_map<void*,size_t> sizes;
int xTaskGetCurrentTaskHandle(){return current;}
void* alloc(size_t n,size_t s){void* p=std::calloc(n,s);if(p)sizes[p]=n*s;return p;}
void* heap_caps_calloc(size_t n,size_t s,int caps){assert(caps==3);++psramCalls;return psramFail?nullptr:alloc(n,s);}
size_t heap_caps_get_allocated_size(void* p){return sizes.at(p);}
extern "C" void* __real_esp_mbedtls_mem_calloc(size_t n,size_t s){++internalCalls;return alloc(n,s);}
extern "C" void __real_esp_mbedtls_mem_free(void* p){sizes.erase(p);std::free(p);}
'''
checks = r'''
int main(){
 ink::http::AllocationBudget receiver,lua;
 receiverBudget=&receiver;receiverBudgetTask.store(1);
 void* p=__wrap_esp_mbedtls_mem_calloc(2,1024);
 assert(p&&psramCalls==1&&internalCalls==0&&receiver.used()==2048);
 __wrap_esp_mbedtls_mem_free(p);assert(receiver.used()==0&&sizes.empty());
 psramFail=true;p=__wrap_esp_mbedtls_mem_calloc(1,1024);
 assert(!p&&internalCalls==0&&receiver.used()==0);psramFail=false;
 int attempts=psramCalls;
 assert(!__wrap_esp_mbedtls_mem_calloc(1,receiver.Limit+1));
 assert(!__wrap_esp_mbedtls_mem_calloc(SIZE_MAX,2));
 assert(psramCalls==attempts);
 current=2;budgetTask.store(2);activeBudget=&lua;
 p=__wrap_esp_mbedtls_mem_calloc(1,512);
 assert(p&&lua.used()==512&&receiver.used()==0&&internalCalls==1);
 __wrap_esp_mbedtls_mem_free(p);assert(lua.used()==0);
 current=3;p=__wrap_esp_mbedtls_mem_calloc(1,256);
 assert(p&&lua.used()==0&&receiver.used()==0&&internalCalls==2);
 __wrap_esp_mbedtls_mem_free(p);assert(sizes.empty());
}
'''
with tempfile.TemporaryDirectory(prefix="airdrop-tls-allocator-") as directory:
    path = Path(directory)
    (path / "check.cpp").write_text(prelude + wrappers + checks)
    subprocess.run(["c++", "-std=c++20", "-fsanitize=address,undefined",
                    "-I" + str(root / "lib/LuaHttp"), str(path / "check.cpp"),
                    "-o", str(path / "check")], check=True)
    subprocess.run([str(path / "check")], check=True)
print("PASS AirDrop TLS uses bounded PSRAM, fails without fallback, and isolates other tasks")
