#!/usr/bin/env python3
"""Compile the exact production worker recovery fragment against owned I/O fakes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--sanitize', action='store_true')
parser.add_argument('--output', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
source = root / 'lib/AirDrop/port/ReceiverEsp32.cpp'

def fragment():
    text = source.read_text()
    worker = text.index('void worker(void* p)')
    start = text.index('  if(stop.load(std::memory_order_acquire))goto cleanup;', worker)
    end = text.index('  if(!HalAirDropRadio::releaseArduino())', start)
    return text[start:end]

block = fragment()
prefix = r'''
#include "StorageSink.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <initializer_list>
namespace storage = airdrop::storage;
std::atomic<bool> stop{false};
struct { bool publicationUncertain=false; } status;
int statusMux=0;
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
struct Config {
  unsigned stopAt=0;
  bool stoppedInitially=false, tokenOnCancel=true, unreadable=false;
  storage::State recovered=storage::State::RetainedPartial;
};
struct FakeStorage {
  Config config;
  unsigned calls=0, afterStop=0;
  explicit FakeStorage(Config value):config(value) {}
  void step() {
    if(stop.load()) ++afterStop;
    if(++calls==config.stopAt) stop.store(true);
  }
  bool recoveryToken(unsigned index,char token[33],bool& ok) {
    step(); std::memset(token,'0',32); token[32]=0;
    if(stop.load()) { ok=config.tokenOnCancel&&index==0; return ok; }
    ok=!config.unreadable; return ok&&index==0;
  }
  storage::State inspectJournal(const char*) { step(); return stop.load()?storage::State::Uncertain:config.recovered; }
  storage::State inspect(const char*) { step(); return stop.load()?storage::State::Uncertain:config.recovered; }
};
struct Context { FakeStorage storage; bool failed=false; explicit Context(Config cfg):storage(cfg) {} };
void failure(Context& context,const char*) { context.failed=true; }
struct Outcome { bool failed,uncertain,entered; unsigned calls,afterStop; };
Outcome run(Config cfg) {
  Context c(cfg); bool entered=false;
  stop.store(cfg.stoppedInitially); status.publicationUncertain=false;
'''
suffix = r'''
  entered=true; // Reached the real block's radio-acquisition boundary.
cleanup:
  return {c.failed,status.publicationUncertain,entered,c.storage.calls,c.storage.afterStop};
}
bool cancelledCase(unsigned step,bool tokenOnCancel=true) {
  Config cfg; cfg.stopAt=step; cfg.tokenOnCancel=tokenOnCancel;
  auto out=run(cfg);
  if(out.failed||out.uncertain||out.entered||out.afterStop) {
    std::fprintf(stderr,"FAIL cancel at step %u token=%d: failure=%d publicationUncertain=%d enteredRadio=%d afterStopCalls=%u\n",
      step,tokenOnCancel,out.failed,out.uncertain,out.entered,out.afterStop); return false;
  }
  return true;
}
int main() {
  // Start with the observed false-publication-uncertainty race for a clear RED.
  if(!cancelledCase(2)||!cancelledCase(3)||!cancelledCase(1)||!cancelledCase(1,false)||!cancelledCase(4)) return 1;
  Config cfg; cfg.stoppedInitially=true; auto out=run(cfg);
  if(out.calls||out.failed||out.uncertain||out.entered) return 2;
  cfg={}; cfg.recovered=storage::State::Uncertain; out=run(cfg);
  if(!out.failed||!out.uncertain||out.entered) return 3;
  cfg={}; cfg.unreadable=true; out=run(cfg);
  if(!out.failed||out.uncertain||out.entered) return 4;
  for(auto state:{storage::State::RetainedPartial,storage::State::PublishedVerified}) {
    cfg={}; cfg.recovered=state; out=run(cfg);
    if(out.failed||out.uncertain||!out.entered) return 5;
  }
  std::puts("PASS actual worker fragment: UI stop at every recovery call aborts cleanly; genuine unreadable/uncertain inventory still fails closed");
}
'''
with tempfile.TemporaryDirectory(prefix='airdrop-recovery-cancel-') as temp:
    out = args.output or Path(temp)
    out.mkdir(parents=True, exist_ok=True)
    cpp = out / 'recovery_cancel.cpp'
    exe = out / 'recovery_cancel'
    cpp.write_text(prefix + block + suffix)
    flags = ['-std=c++20', '-O1', '-g', '-Wall', '-Wextra', '-Werror']
    if args.sanitize:
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    command = ['c++', *flags, '-I'+str(root/'lib/AirDrop/storage'), str(cpp), '-o', str(exe)]
    results = []
    for cmd, name in [(command,'compile'),([str(exe)],'run')]:
        print(json.dumps(cmd), flush=True)
        result = subprocess.run(cmd, capture_output=True, text=True,
            env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        (out/(name+'.log')).write_text(result.stdout+result.stderr)
        print(result.stdout+result.stderr, end='', flush=True)
        results.append({'command':cmd,'exit':result.returncode})
        (out/'receipt.json').write_text(json.dumps({'production_source':str(source),
            'actual_fragment_sha256':hashlib.sha256(block.encode()).hexdigest(),
            'actual_fragment_unchanged_during_test':fragment()==block,
            'sanitizers':args.sanitize,'commands':results,'physical_proof':False},indent=2)+'\n')
        if result.returncode:
            raise SystemExit(1)
    assert fragment()==block, 'Recovery source changed while qualifying its actual fragment'
