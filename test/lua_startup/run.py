#!/usr/bin/env python3
"""Execute exact firmware library registration with vendored int32/float32 Lua."""
import os,pathlib,re,subprocess,json,hashlib
R=pathlib.Path(__file__).resolve().parents[2];os.chdir(R)
out=pathlib.Path(os.environ.get('STARTUP_OUT',str(pathlib.Path(os.environ.get('TMPDIR','/tmp'))/'lua-startup')));out.mkdir(exist_ok=True)
s=(R/'src/activities/apps/lua/LuaRunner.cpp').read_text()
baseline=subprocess.check_output(['git','show','810854354cab8e64126bb26058b8aa90ca0902ce:src/activities/apps/lua/LuaRunner.cpp'],text=True)
block=re.search(r'  static const luaL_Reg essentialLibs\[\].*?\n  }',baseline,re.S).group()
(out/'baseline.h').write_text('inline void baselineEssential(lua_State* L){\n'+block+'\n}\n')
if 'runtime::openEssential' in s:
 header='#include "LuaRuntime.h"\nusing ink::runtime::openEssential; using ink::runtime::openNetwork;\n'
else:
 block=re.search(r'  static const luaL_Reg essentialLibs\[\].*?\n  }',s,re.S).group()
 header='extern "C" {\n#include "lauxlib.h"\n#include "lualib.h"\n}\nint openEssential(lua_State* L){\n'+block+'\nreturn LUA_OK;}\nint openNetwork(lua_State*,bool){return LUA_OK;}\n'
(out/'startup.h').write_text(header)
fixtures=pathlib.Path(os.environ['CP_FIXTURES'])
for item in json.loads((fixtures/'provenance.json').read_text()):
 if item['name'] in ('feed','item'):
  assert hashlib.sha256((fixtures/(item['name']+'.body')).read_bytes()).hexdigest()==item['sha256']
flags=['-O1','-g','-Ilib/Lua/src','-Ilib/LuaHttp','-Ilib/LuaBrowser','-Ilib/Memory','-Ilib/miniz/src','-Isrc/activities/apps/lua','-I'+str(out),'-I'+os.environ.get('ARDUINOJSON_INCLUDE','.pio/libdeps/sticky-owner-test/ArduinoJson/src')]
for source in sorted((R/'lib/Lua/src').glob('l*.c')):
 subprocess.run(['cc',*flags,'-c',str(source),'-o',str(out/(source.stem+'.o'))],check=True)
subprocess.run(['cc',*flags,'-c','lib/miniz/src/miniz_impl.c','-o',str(out/'miniz.o')],check=True)
subprocess.run(['c++','-std=c++20',*flags,'test/lua_startup/startup.cpp','lib/LuaHttp/LuaHttp.cpp','lib/LuaHttp/HttpCore.cpp','lib/LuaHttp/HttpGzip.cpp','lib/hal/HalLuaResources.cpp',*map(str,out.glob('*.o')),'-lm','-ldl','-o',str(out/'startup')],check=True)
for extra in ({'LUA_APP_LIMIT':'2097152'}, {'LUA_APP_LIMIT':'76800','EXPECT_OOM':'1'},
              {'HTTP_ERROR':'time_untrusted'}, {'HTTP_ERROR':'transport_failed'},
              {'CANCEL':'1'}, {'OPT_OUT':'1'}):
 env=dict(os.environ,SMUDGE_X3_CONSTRAINTS='1',LUA_APP_LIMIT='76800');env.update(extra)
 print('CASE',extra,flush=True)
 subprocess.run([str(out/'startup'),'cp-hackernews'],check=True,env=env)

subprocess.run(['c++','-std=c++20',*flags,'test/lua_startup/libraries.cpp',*map(str,out.glob('*.o')),'-lm','-ldl','-o',str(out/'libraries')],check=True)
subprocess.run([str(out/'libraries')],check=True)
subprocess.run(['c++','-std=c++20','-Itest/lua_startup/stubs','-Ilib/LuaHttp','-Ilib/LuaBrowser','-Ilib/Memory','-Ilib/miniz/src','test/lua_startup/policy.cpp','-o',str(out/'policy')],check=True)
subprocess.run([str(out/'policy')],check=True)
print('PASS missing NVS/key allow, opt-out/reenable, storage errors fail closed')
