"""Compile the production time bootstrap with deterministic clock/network doubles."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'lib/hal/HalLuaHttp.cpp').read_text()
def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    end, depth = brace+1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

program = r'''
#include <atomic>
#include <cstdint>
#include <cassert>
#include <cstring>
int timeMux=0;
int64_t trustedEpoch=0,trustedMono=0,mono=1000000;
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
int64_t esp_timer_get_time(){return mono;}
int64_t trustedNow(){return trustedEpoch;}
namespace ink::http {struct Job {std::atomic<bool> cancel{false};struct {const char* error=nullptr;}result;};}
namespace HalLuaHttp {
struct TimeProposal {int64_t epoch;int64_t monotonicUs;};
TimeProposal proposal{1794744000,1000000};
TimeProposal proposeTime(){return proposal;}
}
struct Clock {
 bool synced=false,success=true;int calls=0;std::atomic<bool>* cancelDuringSync=nullptr;
 bool hasSynchronizedSystemTime()const{return synced;}
 bool syncSystemTimeFromNTP(){++calls;if(cancelDuringSync)cancelDuringSync->store(true);return success;}
} halClock;
PIN
PREPARE
void reset(){trustedEpoch=0;trustedMono=0;halClock={};mono=1000000;HalLuaHttp::proposal={1794744000,mono};}
int main(){
 reset();ink::http::Job first;assert(prepareTime(first));assert(halClock.calls==1 && trustedEpoch==1794744000);
 ink::http::Job next;assert(prepareTime(next));assert(halClock.calls==1);
 reset();halClock.synced=true;ink::http::Job ready;assert(prepareTime(ready));assert(halClock.calls==0);
 reset();halClock.success=false;ink::http::Job failed;assert(!prepareTime(failed));assert(!strcmp(failed.result.error,"time_sync_failed") && !trustedEpoch);
 reset();ink::http::Job cancelled;cancelled.cancel.store(true);assert(!prepareTime(cancelled));assert(halClock.calls==0 && !strcmp(cancelled.result.error,"cancelled"));
 reset();ink::http::Job during;halClock.cancelDuringSync=&during.cancel;assert(!prepareTime(during));assert(!trustedEpoch && !strcmp(during.result.error,"cancelled"));
 reset();HalLuaHttp::proposal.epoch=0;ink::http::Job invalid;assert(!prepareTime(invalid));assert(!trustedEpoch && !strcmp(invalid.result.error,"time_sync_failed"));
 reset();HalLuaHttp::proposal.monotonicUs=mono+1;assert(!pinTime(HalLuaHttp::proposal));
 reset();mono+=300000001;assert(!pinTime(HalLuaHttp::proposal));
}
'''.replace('PIN', function('bool pinTime(')).replace('PREPARE', function('bool prepareTime('))
with tempfile.TemporaryDirectory() as folder:
    path = Path(folder)
    (path / 'test.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS production automatic UTC: NTP, reuse, failures, cancellation and invalid/stale anchors')
