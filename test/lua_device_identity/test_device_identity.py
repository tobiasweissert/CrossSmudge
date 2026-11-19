#!/usr/bin/env python3
"""Compile production Lua board reporting with actual PlatformIO board defines."""
import configparser,pathlib,re,subprocess,tempfile,os
R=pathlib.Path(__file__).resolve().parents[2]
c=configparser.ConfigParser(interpolation=None,strict=False);c.read(R/'platformio.ini')
def option(section,key):
 if c.has_option(section,key):value=c.get(section,key)
 elif c.has_option(section,'extends'):return option(c.get(section,'extends').strip(),key)
 else:return ''
 return re.sub(r'\$\{([^}.]+)\.([^}]+)\}',lambda m:option(m[1],m[2]),value)
s=(R/'src/activities/apps/lua/LuaRunner.cpp').read_text();match=re.search(r'int LuaRunner::l_getDevice\(lua_State\* L\) \{.*?\n\}',s,re.S);assert match;body=match.group()
# Preserve the actual compatibility header's defined-but-zero identity macros.
cap=(R/'include/AppCapabilities.h').read_text();mapping=''
prefix=mapping+'\n#include <cstring>\n#include <cstdio>\nstruct lua_State {}; static const char* result; void lua_pushstring(lua_State*,const char* s){result=s;} class LuaRunner {public:static int l_getDevice(lua_State*);};\n'
failed=[]
import argparse, shutil
parser = argparse.ArgumentParser()
parser.add_argument('--baseline', metavar='REV', help='Exercise the production function at a historical Git revision')
a = parser.parse_args()
if a.baseline:
 old = subprocess.check_output(['git', 'show', a.baseline+':src/activities/apps/lua/LuaRunner.cpp'], cwd=R, text=True)
 body = re.search(r'int LuaRunner::l_getDevice\(lua_State\* L\) \{.*?\n\}', old, re.S).group()

with tempfile.TemporaryDirectory(prefix='sticky-device-') as tmp:
 p=pathlib.Path(tmp);src=p/'device.cpp';src.write_text(prefix+body+'\nint main(){lua_State L;LuaRunner::l_getDevice(&L);puts(result);}\n')
 for env,expected in [('sticky','sticky'),('sticky-debug','sticky'),('sticky-owner-test','sticky'),('default','sticky'),('debug','sticky')]:
  flags=option('env:'+env,'build_flags');defines=re.findall(r'-D((?:FREEINK_DEVICE_|CROSSINK_APP_DEVICE_)[A-Z0-9_]+)(?:=([^\s]+))?',flags);args=['-D'+name+'='+ (value or '1') for name,value in defines]
  subprocess.run(['g++',*args,str(src),'-o',str(p/'device')],check=True);actual=subprocess.check_output([str(p/'device')],text=True).strip();print(env,dict(defines),'expected',expected,'actual',actual)
  if actual!=expected:failed.append(env)
 # Simulator identity must win even when hardware defines are also present.
 subprocess.run(['g++','-DSIMULATOR=1','-DFREEINK_DEVICE_STICKY=1',str(src),'-o',str(p/'device')],check=True);assert subprocess.check_output([str(p/'device')],text=True).strip()=='simulator'
 # Link the exact production function to vendored Lua and the existing native adapter.
 # The app dispatcher and touch callback run unchanged; no Lua identity override.
 touch = re.search(r'void LuaRunner::onTouch\(int x, int y, const char\* eventType\) \{.*?\n\}', s, re.S).group()
 linked = p/'linked.cpp'
 linked.write_text(mapping + '\nextern "C" {\n#include "lua.h"\n}\n'
                   + '#include <cstdio>\nclass LuaRunner {public:lua_State* L;bool hasError_=false;static LuaRunner* s_activeRunner;static int l_getDevice(lua_State*);void onTouch(int,int,const char*);void setError(const char* e){hasError_=true;fprintf(stderr,"%s\\n",e);}};\nLuaRunner* LuaRunner::s_activeRunner=nullptr;\n'
                   + touch + '\nextern "C" int production_touch(lua_State* L) {LuaRunner runner;runner.L=L;runner.onTouch(lua_tointeger(L,1),lua_tointeger(L,2),lua_tostring(L,3));lua_pushboolean(L,!runner.hasError_);return 1;}\n'
                   + body + '\nextern "C" int production_get_device(lua_State* L) {return LuaRunner::l_getDevice(L); }\n')
 inc = '-I'+str(R/'lib/Lua/src')
 owner_defines = re.findall(r'-D((?:FREEINK_DEVICE_|CROSSINK_APP_DEVICE_)[A-Z0-9_]+)(?:=([^\s]+))?', option('env:sticky-owner-test','build_flags'))
 owner_args = ['-D'+name+'='+(value or '1') for name,value in owner_defines]
 subprocess.run(['g++', *owner_args, inc, '-c', str(linked), '-o', str(p/'linked.o')], check=True)
 runner = p/'runner'
 subprocess.run(['cc', '-O2', '-DPRODUCTION_DEVICE_IDENTITY', inc,
                 str(R/'test/replacement_solitaire/runner.c'),
                 *map(str, sorted((R/'lib/Lua/src').glob('*.c'))), str(p/'linked.o'), '-lm', '-o', str(runner)], check=True)
 fixture=p/'app';shutil.copytree(R/'apps/solitaire',fixture)
 (fixture/'storage').mkdir();(fixture/'import').mkdir()
 for cap,width,height in [(cap,w,h) for cap in (76800,2097152) for w,h in ((480,800),(800,480))]:
  for f in (fixture/'storage').iterdir():f.unlink()
  result=subprocess.run([str(runner),str(R/'test/lua_device_identity/stock_tap.lua'),str(cap),str(width),str(height)],cwd=fixture)
  if result.returncode:failed.append('production Lua stock tap '+str((width,height)))
assert not failed,'Production get_device regression: '+', '.join(failed)
print('PASS: configured board identity and simulator precedence')
