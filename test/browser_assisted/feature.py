#!/usr/bin/env python3
"""Execute the actual LuaRunner feature/version bodies with minimal host fields."""
import pathlib,os,re,subprocess,json,hashlib
R=pathlib.Path(__file__).resolve().parents[2];os.chdir(R);O=pathlib.Path(os.environ['ASSISTED_TEST_OUT'])
cpp=(R/'src/activities/apps/lua/LuaRunner.cpp').read_text();header=(R/'src/activities/apps/lua/LuaRunner.h').read_text()
feature=cpp[cpp.index('int LuaRunner::l_hasFeature('):cpp.index('\n}  // namespace ink',cpp.index('int LuaRunner::l_hasFeature('))]
version=cpp[cpp.index('int LuaRunner::l_getApiVersion('):cpp.index('\nint LuaRunner::l_hasFeature(')]
constant=re.search(r'static constexpr int LUA_API_VERSION = \d+;',header).group()
source='''
#define main priorRunnerMain
#include "RUNNER"
#undef main
#include "BackendConfig.h"
#include <cstring>
#define CROSSINK_APP_CAP_TOUCH 1
class LuaRunner {public: @@CONSTANT@@ bool browser_=false;struct Http {ink::http::BackendAccess backendAccess;} http_;static LuaRunner* s_activeRunner;static int l_hasFeature(lua_State*);static int l_getApiVersion(lua_State*);};
LuaRunner* LuaRunner::s_activeRunner=nullptr;
@@VERSION@@
@@FEATURE@@
static void script(lua_State* L,const char* s){if(luaL_dostring(L,s)){fprintf(stderr,"%s\\n",lua_tostring(L,-1));abort();}}
int main(){Heap heap;auto* L=lua_newstate(Heap::alloc,&heap);assert(ink::runtime::openEssential(L)==LUA_OK);lua_register(L,"feature",LuaRunner::l_hasFeature);lua_register(L,"api",LuaRunner::l_getApiVersion);
script(L,"assert(api()==7 and not feature('browser_backend_v1'))");LuaRunner runner;LuaRunner::s_activeRunner=&runner;
runner.http_.backendAccess.read=[](void*,char (&)[65]){return false;};script(L,"assert(not feature('browser_backend_v1'))");runner.browser_=true;script(L,"assert(feature('browser_backend_v1')and feature('browser_v6')and not feature('unknown'))");runner.http_.backendAccess.read=nullptr;script(L,"assert(not feature('browser_backend_v1'))");lua_close(L);assert(!heap.used);puts("PASS actual LuaRunner API7 and Browser/native-backend capability bodies; minimal host fields");}
'''.replace('RUNNER',str(R/'test/browser/runner.cpp')).replace('@@CONSTANT@@',constant).replace('@@VERSION@@',version).replace('@@FEATURE@@',feature)
p=O/'feature.cpp';p.write_text(source)
flags=['-O1','-g','-std=c++20','-Ilib/Lua/src','-Ilib/LuaHttp','-Ilib/LuaBrowser','-Ilib/Memory','-Isrc/activities/apps/lua']
if os.environ.get('SANITIZE'):flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
cmd=['c++',*flags,str(p),*[str(O/(s.stem+'.o'))for s in sorted(pathlib.Path('lib/Lua/src').glob('l*.c'))],'-lm','-ldl','-o',str(O/'feature')]
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
subprocess.run(cmd,env=env,check=True);subprocess.run([str(O/'feature')],env=env,check=True)
(O/'feature-receipt.json').write_text(json.dumps({'command':cmd,'exit':0,'LuaRunner_cpp_sha256':hashlib.sha256(cpp.encode()).hexdigest(),'LuaRunner_h_sha256':hashlib.sha256(header.encode()).hexdigest(),'scope':'actual function bodies, minimal host fields; not full LuaRunner firmware linkage'},indent=2)+'\n')
