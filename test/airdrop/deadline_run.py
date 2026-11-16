#!/usr/bin/env python3
"""Exercise actual HAL cancellation predicate at publication deadline boundaries."""
from pathlib import Path
import subprocess,tempfile,re,os
root=Path(__file__).resolve().parents[2]
header=(root/'lib/AirDrop/storage/HalStorageSink.h').read_text();source=(root/'lib/AirDrop/storage/HalStorageSink.cpp').read_text()
m=re.search(r'bool cancelled\(\) const override \{([^}]+)\}',header)
if m:body=m.group(1)
else:
 m=re.search(r'bool HalFiles::cancelled\(\) const \{([^}]+)\}',source);assert m;body=m.group(1)
program='''#include <atomic>
#include <cstdint>
#include <cassert>
int64_t mono=0;int64_t esp_timer_get_time(){return mono;}
struct HalFiles{std::atomic<bool> cancelRequested_{false};uint64_t deadlineMs_=0;bool cancelled()const{BODY}};
int main(){HalFiles f;assert(!f.cancelled());f.deadlineMs_=1000;mono=999000;assert(!f.cancelled());mono=1000000;assert(f.cancelled());mono=1001000;assert(f.cancelled());f.deadlineMs_=0;assert(!f.cancelled());f.cancelRequested_.store(true);assert(f.cancelled());f.deadlineMs_=2000;assert(f.cancelled());}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='airdrop-deadline-',dir=os.environ.get('TMPDIR')) as tmp:
 p=Path(tmp);(p/'case.cpp').write_text(program);subprocess.run(['c++','-std=c++20',str(p/'case.cpp'),'-o',str(p/'case')],check=True);subprocess.run([str(p/'case')],check=True)
print('PASS actual HAL cancellation predicate: absolute publication deadline and sticky owner cancellation')
