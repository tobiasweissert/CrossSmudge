#!/usr/bin/env python3
"""Production-bound AirDrop regression suite; synthetic radio HAL, no device I/O."""
import fcntl, os, re, subprocess, sys, tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[2]
lock=open('/root/.hermes/cache/scratch/crosscub-review-probes.lock','a')
try: fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
except BlockingIOError: raise SystemExit('BUSY: compiler lock (nonblocking)')
def section(s,name):
 m=re.search(r'static (?:bool|void) '+name+r'\([^;]*?\)\s*\{',s)
 assert m, 'Production radio method missing: '+name
 start=m.start();at=m.end();depth=1
 while depth:
  if s[at]=='{':depth+=1
  if s[at]=='}':depth-=1
  at+=1
 return s[start:at]
flags=['-O1','-g','-fno-omit-frame-pointer']
if '--sanitize' in sys.argv: flags+=['-fsanitize=address,undefined','-fno-pie','-no-pie']
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',PYTHONDONTWRITEBYTECODE='1')
with tempfile.TemporaryDirectory(prefix='airdrop-fixes-') as temp:
 t=Path(temp)
 source=(R/'lib/AirDrop/vendor/ESP32Drop/src/awdl/port/awdl_port_esp32.cpp').read_text()
 methods='\n'.join(section(source,n) for n in ['netif_inflight_room','netif_inflight_begin','netif_inflight_done','raw_tx_done_cb','awdl_netif_flush_tx'])
 fixture=(R/'test/airdrop/multicast_hal.inc').read_text().replace('// Production methods are extracted by fixes_run.py here.',methods)
 (t/'multicast.cpp').write_text(fixture)
 # Retain the actual HAL declaration/fields; remove only unused virtual dispatch
 # so this narrow method fixture does not need unrelated filesystem operations.
 header=(R/'lib/AirDrop/storage/HalStorageSink.h').read_text().replace('class HalFiles final : public Files','class HalFiles').replace(' override','')
 header=header[:header.index('class HalStorageSink {')]+'} // namespace airdrop::storage\n#endif\n'
 (t/'HalStorageSink.h').write_text(header)
 hal=(R/'lib/AirDrop/storage/HalStorageSink.cpp').read_text()
 names=['cancelled','writeStage','read']
 if 'void HalFiles::cooperate(' in hal:names+=['cooperate']
 methods=[]
 for name in names:
  m=re.search(r'(?:bool|size_t|int|void) HalFiles::'+name+r'\([^;]*?\)(?: const)?\s*\{',hal)
  assert m, 'Production HAL method missing: '+name
  at=m.end();depth=1
  while depth:
   if hal[at]=='{':depth+=1
   if hal[at]=='}':depth-=1
   at+=1
  methods.append(hal[m.start():at])
 (t/'hal.cpp').write_text((R/'test/airdrop/hal_yield_cases.cpp').read_text().replace('// PRODUCTION METHODS','\n'.join(methods)))
 commands=[['c++','-std=c++17',*flags,str(t/'multicast.cpp'),'-o',str(t/'multicast')],[str(t/'multicast')],
 ['c++','-std=c++20',*flags,'-Ilib/AirDrop','test/airdrop/chunk_budget_cases.cpp','lib/AirDrop/core/HttpRequest.cpp','-o',str(t/'chunk-budget')],[str(t/'chunk-budget')],
 ['c++','-std=c++20',*flags,'-DFREEINK_DEVICE_STICKY=1','-I'+str(t),'-Itest/airdrop/hal_stubs','-Ilib/AirDrop/storage',str(t/'hal.cpp'),'-o',str(t/'hal')],[str(t/'hal')]]
 for command in commands:
  print('COMMAND',*command,flush=True)
  result=subprocess.run(command,cwd=R,env=env)
  if result.returncode:raise SystemExit(result.returncode)
print('PASS AirDrop repairs production-bound regressions')
