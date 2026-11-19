#!/usr/bin/env python3
"""Extract actual allocator/init pool selection and test with real firmware Lua."""
from pathlib import Path
import subprocess,tempfile,json
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
source=(ROOT/'src/activities/apps/lua/LuaRunner.cpp').read_text()
body=source[source.index('void* LuaRunner::customLuaAlloc('):source.index('\nbool LuaRunner::init()')]
start=source.index('  // Select the pool once, before the first VM allocation.')
selection=source[start:source.index('#endif',start)+len('#endif')+1]
results=[]
with tempfile.TemporaryDirectory(prefix='lua-allocator-')as tmp:
 work=Path(tmp)
 (work/'production.inc').write_text('void LuaRunner::selectHeap(){\n'+selection+'}\n#define realloc fakeRealloc\n#define free fakeFree\n'+body+'\n#undef realloc\n#undef free\n')
 objects=[]
 for path in (ROOT/'lib/Lua/src').glob('*.c'):
  obj=work/(path.stem+'.o');subprocess.run(['cc','-O2','-I'+str(ROOT/'lib/Lua/src'),'-c',str(path),'-o',str(obj)],check=True);objects.append(str(obj))
 for name,flags,pool in [('psram',['-DARDUINO_ARCH_ESP32','-DBOARD_HAS_PSRAM'],'psram'),('psram-unavailable',['-DARDUINO_ARCH_ESP32','-DBOARD_HAS_PSRAM'],'none'),('c3',['-DARDUINO_ARCH_ESP32'],'none'),('simulator',['-DSIMULATOR','-DBOARD_HAS_PSRAM','-DARDUINO_ARCH_ESP32'],'psram')]:
  exe=work/name
  subprocess.run(['c++','-std=c++17','-O2',*flags,'-I'+str(ROOT/'lib/LuaBrowser'),'-I'+str(work),'-I'+str(ROOT/'lib/Lua/src'),str(HERE/'fixture.cpp'),*objects,'-lm','-o',str(exe)],check=True)
  r=subprocess.run([str(exe),pool],text=True,capture_output=True,timeout=60);print(name,r.returncode,r.stdout,r.stderr,flush=True)
  results.append(dict(mode=name,code=r.returncode,stdout=r.stdout,stderr=r.stderr))
Path('/tmp/lua-allocator-results.json').write_text(json.dumps(results,indent=2)+'\n');assert all(r['code']==0 for r in results)
